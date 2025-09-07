#include "CLITestRunner.hpp"
#include "CommandUsage.hpp"
#include "UsageManager.hpp"

#include <sstream>
#include <iostream>
#include <algorithm>

using namespace vh::shell;

namespace {
// trim helpers
// std::string trim(std::string s) {
//     auto wsfront = std::find_if_not(s.begin(), s.end(),
//                                     [](unsigned char c){ return std::isspace(c); });
//     auto wsback  = std::find_if_not(s.rbegin(), s.rend(),
//                                     [](unsigned char c){ return std::isspace(c); }).base();
//     if (wsfront >= wsback) return {};
//     return std::string(wsfront, wsback);
// }

// Choose a canonical flag/option token (prefer long; else first)
std::string prefer_long_token(const std::vector<std::string>& toks) {
    if (toks.empty()) return {};
    auto best = toks.front();
    for (const auto& t : toks) {
        if (t.size() > best.size()) best = t;
        // break ties preferring alphabetically earlier long forms
    }
    return best;
}
}

namespace vh::test {

// ======= ctor / basic registration =======

CLITestRunner::CLITestRunner(UsageManager& usage,
                             ExecFn exec,
                             std::shared_ptr<ArgValueProvider> provider)
    : usage_(usage), exec_(std::move(exec)), provider_(std::move(provider)) {}

void CLITestRunner::registerValidator(const std::string& path, ValidatorFn v) {
    per_path_validators_[path].push_back(std::move(v));
}
void CLITestRunner::registerStdoutContains(const std::string& path, std::string s) {
    per_path_stdout_contains_[path].push_back(std::move(s));
}
void CLITestRunner::registerStdoutNotContains(const std::string& path, std::string s) {
    per_path_stdout_not_contains_[path].push_back(std::move(s));
}
void CLITestRunner::addTest(TestCase t) { tests_.push_back(std::move(t)); }

// ======= small helpers =======

std::string CLITestRunner::dashify(const std::string& t) {
    if (t.empty()) return t;
    if (t[0] == '-') return t;
    return (t.size() == 1 ? "-" : "--") + t;
}
std::string CLITestRunner::pickBestToken(const std::vector<std::string>& tokens) {
    return dashify(prefer_long_token(tokens.empty() ? std::vector<std::string>{} : tokens));
}
std::optional<std::string> CLITestRunner::firstValueFromTokens(const std::vector<std::string>& value_tokens,
                                                               ArgValueProvider& provider,
                                                               const std::string& usage_path,
                                                               Context& ctx,
                                                               std::string* chosen_token_out) {
    for (const auto& tok : value_tokens) {
        if (auto v = provider.valueFor(tok, usage_path)) {
            ctx[tok] = *v;
            if (chosen_token_out) *chosen_token_out = tok;
            return v;
        }
    }
    return std::nullopt;
}

std::string CLITestRunner::usagePathFor(const std::vector<std::string>& segs) {
    std::string out;
    for (size_t i = 0; i < segs.size(); ++i) {
        out += segs[i];
        if (i + 1 < segs.size()) out += '/';
    }
    return out;
}

std::string CLITestRunner::primaryAlias(const std::shared_ptr<CommandUsage>& u) {
    if (!u || u->aliases.empty()) return {};
    return u->aliases.front();
}
std::string CLITestRunner::joinPath(const std::vector<std::string>& segs) { return usagePathFor(segs); }
bool CLITestRunner::isLeaf(const std::shared_ptr<CommandUsage>& u) { return !u || u->subcommands.empty(); }

// ======= public generation API =======

void CLITestRunner::generateFromUsage(bool help_tests,
                                      bool happy_path,
                                      bool negative_required,
                                      size_t max_examples_per_cmd) {
    GenerateConfig cfg;
    cfg.help_tests = help_tests;
    cfg.happy_path = happy_path;
    cfg.negative_required = negative_required;
    cfg.max_examples_per_cmd = max_examples_per_cmd;
    generateFromUsage(cfg);
}

void CLITestRunner::generateFromUsage(const GenerateConfig& cfg) {
    auto root = usage_.getFilteredTestUsage();
    std::vector<std::string> segs;
    traverse(root, segs, cfg.help_tests, cfg.happy_path, cfg.negative_required, cfg.max_examples_per_cmd);

    // Second pass for richer tests (variants / invalids)
    // Walk again to keep original traversal unchanged
    segs.clear();
    std::function<void(std::shared_ptr<CommandUsage>&, std::vector<std::string>&)> walk =
        [&](std::shared_ptr<CommandUsage>& node, std::vector<std::string>& path) {
            if (!node) return;
            if (primaryAlias(node) != "vh") path.push_back(primaryAlias(node));
            if (isLeaf(node)) {
                if (cfg.matrix_variants) genMatrixVariants(path, node, cfg);
                if (cfg.negative_invalid_values) genInvalidValueTests(path, node, cfg.max_variants_per_cmd);
                if (cfg.negative_required) genMissingEachRequiredTest(path, node);
            } else {
                for (auto& sub : node->subcommands) walk(sub, path);
            }
            if (!path.empty() && primaryAlias(node) != "vh") path.pop_back();
        };
    walk(root, segs);
}

// ======= traversal =======

void CLITestRunner::traverse(const std::shared_ptr<CommandUsage>& node,
                             std::vector<std::string>& path_aliases,
                             bool help_tests,
                             bool happy_path,
                             bool negative_required,
                             size_t max_examples) {
    if (!node) return;

    if (primaryAlias(node) != "vh") path_aliases.push_back(primaryAlias(node));

    if (help_tests) genHelpTest(path_aliases, node);
    if (isLeaf(node)) {
        if (happy_path) genHappyPathTest(path_aliases, node);
        genExampleTests(path_aliases, node, max_examples);
        if (negative_required) genMissingRequiredTest(path_aliases, node);
    } else {
        for (const auto& sub : node->subcommands) {
            traverse(sub, path_aliases, help_tests, happy_path, negative_required, max_examples);
        }
    }

    if (!path_aliases.empty() && primaryAlias(node) != "vh") path_aliases.pop_back();
}

// ======= generators: simple =======

void CLITestRunner::genHelpTest(const std::vector<std::string>& path_aliases,
                                const std::shared_ptr<CommandUsage>& u) {
    std::ostringstream cmd;
    cmd << "vh";
    for (auto& seg : path_aliases) cmd << ' ' << seg;
    cmd << " --help";

    TestCase t;
    t.name   = "help: " + joinPath(path_aliases);
    t.path   = joinPath(path_aliases);
    t.command = cmd.str();
    t.expect_exit = 0;
    if (u && !u->description.empty()) t.must_contain.push_back(u->description);

    if (per_path_stdout_contains_.count(t.path))
        for (auto& s : per_path_stdout_contains_[t.path]) t.must_contain.push_back(s);
    if (per_path_stdout_not_contains_.count(t.path))
        for (auto& s : per_path_stdout_not_contains_[t.path]) t.must_not_contain.push_back(s);
    if (per_path_validators_.count(t.path))
        for (auto& v : per_path_validators_[t.path]) t.validators.push_back(v);

    tests_.push_back(std::move(t));
}

void CLITestRunner::genExampleTests(const std::vector<std::string>& path_aliases,
                                    const std::shared_ptr<CommandUsage>& u,
                                    size_t max_examples) {
    if (!u) return;

    size_t count = 0;
    for (const auto& ex : u->examples) {
        if (count++ >= max_examples) break;

        TestCase t;
        t.name        = "example: " + ex.cmd;
        t.path        = joinPath(path_aliases);
        t.command     = ex.cmd;
        t.expect_exit = 0;
        if (!u->description.empty()) t.must_contain.push_back(u->description);

        if (per_path_stdout_contains_.count(t.path))
            for (auto& s : per_path_stdout_contains_[t.path]) t.must_contain.push_back(s);
        if (per_path_stdout_not_contains_.count(t.path))
            for (auto& s : per_path_stdout_not_contains_[t.path]) t.must_not_contain.push_back(s);
        if (per_path_validators_.count(t.path))
            for (auto& v : per_path_validators_[t.path]) t.validators.push_back(v);

        tests_.push_back(std::move(t));
    }
}

// ======= argument synthesis (new model-aware) =======

std::optional<std::string> CLITestRunner::buildArgs(const std::shared_ptr<CommandUsage>& u,
                                                    Context& ctx) {
    if (!u) return std::string{};
    std::ostringstream args;
    const std::string usage_path = "<" + primaryAlias(u) + ">"; // fine-grained path is injected by callers

    // 1) Positionals
    for (const auto& pos : u->positionals) {
        // In your new model, Positional.label is the token name (e.g., "role_id").
        // Try provider by label; else by first alias; else fallback.
        std::string token_name = pos.label.empty() && !pos.aliases.empty() ? pos.aliases.front() : pos.label;
        if (token_name.empty()) token_name = "arg";
        auto val = provider_->valueFor(token_name, usage_path);
        if (!val) {
            // try aliases
            for (const auto& a : pos.aliases) {
                val = provider_->valueFor(a, usage_path);
                if (val) { token_name = a; break; }
            }
        }
        if (!val) return std::nullopt;
        ctx[token_name] = *val;
        args << ' ' << *val;
    }

    // 2) Required flags (boolean)
    for (const auto& f : u->required_flags) {
        const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
        if (!tok.empty()) args << ' ' << tok;
    }

    // 3) Required options (with values)
    for (const auto& o : u->required) {
        const auto opt_tok = !o.option_tokens.empty() ? pickBestToken(o.option_tokens)
                                                      : dashify(o.label);
        if (opt_tok.empty()) return std::nullopt;

        // choose first value we can source from provider
        std::string chosen_value_token;
        auto val = firstValueFromTokens(o.value_tokens, *provider_, usage_path, ctx, &chosen_value_token);
        if (!val) {
            // fallback hard default
            std::string fallback = "1";
            if (!o.value_tokens.empty()) ctx[o.value_tokens.front()] = fallback;
            else ctx["value"] = fallback;
            args << ' ' << opt_tok << ' ' << fallback;
        } else {
            args << ' ' << opt_tok << ' ' << *val;
        }
    }

    // 4) Groups (Optional | Flag) – keep minimal for happy-path; we only enable default-on flags
    for (const auto& g : u->groups) {
        for (const auto& v : g.items) {
            if (std::holds_alternative<Flag>(v)) {
                const auto& f = std::get<Flag>(v);
                if (f.default_state) {
                    const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
                    if (!tok.empty()) args << ' ' << tok;
                }
            }
        }
    }

    return args.str();
}

// ======= happy path =======

void CLITestRunner::genHappyPathTest(const std::vector<std::string>& path_aliases,
                                     const std::shared_ptr<CommandUsage>& u) {
    std::ostringstream cmd;
    cmd << "vh";
    for (auto& seg : path_aliases) cmd << ' ' << seg;

    Context ctx;
    auto argStr = buildArgs(u, ctx);
    if (!argStr.has_value()) return;
    cmd << *argStr;

    TestCase t;
    t.name        = "happy: " + joinPath(path_aliases);
    t.path        = joinPath(path_aliases);
    t.command     = cmd.str();
    t.expect_exit = 0;
    t.ctx         = std::move(ctx);

    if (per_path_stdout_contains_.count(t.path))
        for (auto& s : per_path_stdout_contains_[t.path]) t.must_contain.push_back(s);
    if (per_path_stdout_not_contains_.count(t.path))
        for (auto& s : per_path_stdout_not_contains_[t.path]) t.must_not_contain.push_back(s);
    if (per_path_validators_.count(t.path))
        for (auto& v : per_path_validators_[t.path]) t.validators.push_back(v);

    tests_.push_back(std::move(t));
}

// ======= negatives =======

void CLITestRunner::genMissingRequiredTest(const std::vector<std::string>& path_aliases,
                                           const std::shared_ptr<CommandUsage>& u) {
    if (!u || (u->required.empty() && u->required_flags.empty())) return;

    std::ostringstream cmd;
    cmd << "vh";
    for (auto& seg : path_aliases) cmd << ' ' << seg;

    TestCase t;
    t.name        = "neg:missing-required: " + joinPath(path_aliases);
    t.path        = joinPath(path_aliases);
    t.command     = cmd.str();
    t.expect_exit = 1;

    tests_.push_back(std::move(t));
}

// missing each required item individually
void CLITestRunner::genMissingEachRequiredTest(const std::vector<std::string>& path_aliases,
                                               const std::shared_ptr<CommandUsage>& u) {
    if (!u) return;

    // Build full baseline args once
    Context base_ctx;
    auto base_args_opt = buildArgs(u, base_ctx);
    if (!base_args_opt) return;
    const std::string baseline = *base_args_opt;

    // Expand into tokenized form we can prune
    struct Chunk { std::string text; bool isRequiredOpt=false; std::string tag; };
    std::vector<Chunk> chunks;

    // Reconstruct chunks according to our buildArgs rules
    // 1) positionals
    for (const auto& pos : u->positionals) {
        std::string token = pos.label.empty() && !pos.aliases.empty() ? pos.aliases.front() : pos.label;
        chunks.push_back({" <" + token + ">", true, "pos:" + token});
    }
    // 2) required flags
    for (const auto& f : u->required_flags) {
        const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
        chunks.push_back({" " + tok, true, "flag:" + tok});
    }
    // 3) required options
    for (const auto& o : u->required) {
        const auto opt_tok = !o.option_tokens.empty() ? pickBestToken(o.option_tokens) : dashify(o.label);
        std::string val_tag = o.value_tokens.empty() ? "value" : o.value_tokens.front();
        chunks.push_back({" " + opt_tok + " <" + val_tag + ">", true, "opt:" + opt_tok});
    }

    // Now produce tests removing one required chunk at a time
    for (const auto& c : chunks) {
        if (!c.isRequiredOpt) continue;

        std::ostringstream cmd;
        cmd << "vh";
        for (auto& seg : path_aliases) cmd << ' ' << seg;

        // Re-synthesize args and skip this chunk's tag during real construction
        Context ctx;
        std::ostringstream args;

        // positions
        for (const auto& pos : u->positionals) {
            std::string token = pos.label.empty() && !pos.aliases.empty() ? pos.aliases.front() : pos.label;
            if ("pos:" + token == c.tag) continue;
            auto val = provider_->valueFor(token, joinPath(path_aliases));
            if (!val) val = std::string("1");
            args << ' ' << *val;
        }
        // required flags
        for (const auto& f : u->required_flags) {
            const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
            if ("flag:" + tok == c.tag) continue;
            args << ' ' << tok;
        }
        // required options
        for (const auto& o : u->required) {
            const auto opt_tok = !o.option_tokens.empty() ? pickBestToken(o.option_tokens) : dashify(o.label);
            if ("opt:" + opt_tok == c.tag) continue;
            std::string dummy;
            auto val = firstValueFromTokens(o.value_tokens, *provider_, joinPath(path_aliases), ctx, &dummy);
            if (!val) val = std::string("1");
            args << ' ' << opt_tok << ' ' << *val;
        }

        cmd << args.str();

        TestCase t;
        t.name        = "neg:missing-each: " + c.tag + " @ " + joinPath(path_aliases);
        t.path        = joinPath(path_aliases);
        t.command     = cmd.str();
        t.expect_exit = 1;
        tests_.push_back(std::move(t));
    }
}

void CLITestRunner::genInvalidValueTests(const std::vector<std::string>& path_aliases,
                                         const std::shared_ptr<CommandUsage>& u,
                                         std::size_t max_variants) {
    if (!u) return;
    std::size_t produced = 0;

    for (const auto& o : u->required) {
        if (o.value_tokens.empty()) continue; // nothing to poison
        if (produced >= max_variants) break;

        // Synthesize baseline with an invalid value for this option
        std::ostringstream cmd;
        cmd << "vh";
        for (auto& seg : path_aliases) cmd << ' ' << seg;

        Context ctx;
        // positionals
        for (const auto& pos : u->positionals) {
            std::string token = pos.label.empty() && !pos.aliases.empty() ? pos.aliases.front() : pos.label;
            auto v = provider_->valueFor(token, joinPath(path_aliases));
            if (!v) v = std::string("1");
            ctx[token] = *v;
            cmd << ' ' << *v;
        }
        // required flags
        for (const auto& f : u->required_flags) {
            const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
            cmd << ' ' << tok;
        }
        // required options with one poisoned
        for (const auto& x : u->required) {
            const auto opt_tok = !x.option_tokens.empty() ? pickBestToken(x.option_tokens) : dashify(x.label);
            cmd << ' ' << opt_tok << ' ';
            if (&x == &o) {
                cmd << "__INVALID__";
            } else {
                std::string dummy;
                auto v = firstValueFromTokens(x.value_tokens, *provider_, joinPath(path_aliases), ctx, &dummy);
                if (!v) v = std::string("1");
                cmd << *v;
            }
        }

        TestCase t;
        t.name        = "neg:invalid-value: " + joinPath(path_aliases) + " [" + o.label + "]";
        t.path        = joinPath(path_aliases);
        t.command     = cmd.str();
        t.expect_exit = 1;
        tests_.push_back(std::move(t));
        ++produced;
    }
}

// ======= variants (exercise alt tokens / value tokens) =======

void CLITestRunner::genMatrixVariants(const std::vector<std::string>& path_aliases,
                                      const std::shared_ptr<CommandUsage>& u,
                                      const GenerateConfig& cfg) {
    if (!u) return;
    std::size_t produced = 0;
    const auto path = joinPath(path_aliases);

    // Variant A: use alternate option token if available
    for (const auto& o : u->required) {
        if (o.option_tokens.size() < 2) continue;
        if (produced >= cfg.max_variants_per_cmd) break;

        const auto alt_tok = dashify(o.option_tokens.back());

        std::ostringstream cmd; Context ctx;
        cmd << "vh"; for (auto& s : path_aliases) cmd << ' ' << s;

        // positionals
        for (const auto& pos : u->positionals) {
            std::string token = pos.label.empty() && !pos.aliases.empty() ? pos.aliases.front() : pos.label;
            auto v = provider_->valueFor(token, path); if (!v) v = std::string("1");
            ctx[token] = *v; cmd << ' ' << *v;
        }
        // required flags
        for (const auto& f : u->required_flags) {
            const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
            cmd << ' ' << tok;
        }
        // options with one alternative token
        for (const auto& x : u->required) {
            const auto tok = (&x == &o) ? alt_tok
                                        : (!x.option_tokens.empty() ? pickBestToken(x.option_tokens) : dashify(x.label));
            std::string dummy;
            auto v = firstValueFromTokens(x.value_tokens, *provider_, path, ctx, &dummy);
            if (!v) v = std::string("1");
            cmd << ' ' << tok << ' ' << *v;
        }

        TestCase t;
        t.name        = "matrix:alt-opt-token: " + path + " -> " + alt_tok;
        t.path        = path;
        t.command     = cmd.str();
        t.expect_exit = 0;
        tests_.push_back(std::move(t));
        ++produced;
    }

    // Variant B: use alternate value token (id vs name, etc.)
    for (const auto& o : u->required) {
        if (o.value_tokens.size() < 2) continue;
        if (produced >= cfg.max_variants_per_cmd) break;

        const auto opt_tok = !o.option_tokens.empty() ? pickBestToken(o.option_tokens) : dashify(o.label);
        const auto alt_value_token = o.value_tokens.back();

        std::ostringstream cmd; Context ctx;
        cmd << "vh"; for (auto& s : path_aliases) cmd << ' ' << s;

        // positionals
        for (const auto& pos : u->positionals) {
            std::string token = pos.label.empty() && !pos.aliases.empty() ? pos.aliases.front() : pos.label;
            auto v = provider_->valueFor(token, path); if (!v) v = std::string("1");
            ctx[token] = *v; cmd << ' ' << *v;
        }
        // required flags
        for (const auto& f : u->required_flags) {
            const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);
            cmd << ' ' << tok;
        }
        // options with one alternative value token
        for (const auto& x : u->required) {
            const auto tok = !x.option_tokens.empty() ? pickBestToken(x.option_tokens) : dashify(x.label);
            if (&x == &o) {
                // try specifically the alt token
                auto v = provider_->valueFor(alt_value_token, path);
                if (!v) v = std::string("2"); // fallback alt
                ctx[alt_value_token] = *v;
                cmd << ' ' << tok << ' ' << *v;
            } else {
                std::string dummy;
                auto v = firstValueFromTokens(x.value_tokens, *provider_, path, ctx, &dummy);
                if (!v) v = std::string("1");
                cmd << ' ' << tok << ' ' << *v;
            }
        }

        TestCase t;
        t.name        = "matrix:alt-value-token: " + path + " -> <" + alt_value_token + ">";
        t.path        = path;
        t.command     = cmd.str();
        t.expect_exit = 0;
        tests_.push_back(std::move(t));
        ++produced;
    }

    // Variant C: toggle one optional flag if present
    if (produced < cfg.max_variants_per_cmd && !u->optional_flags.empty()) {
        const auto& f = u->optional_flags.front();
        const auto tok = !f.aliases.empty() ? pickBestToken(f.aliases) : dashify(f.label);

        std::ostringstream cmd; Context ctx;
        cmd << "vh"; for (auto& s : path_aliases) cmd << ' ' << s;
        auto base = buildArgs(u, ctx); if (!base) return;
        cmd << *base << ' ' << tok;

        TestCase t;
        t.name        = "matrix:opt-flag: " + path + " -> " + tok;
        t.path        = path;
        t.command     = cmd.str();
        t.expect_exit = 0;
        tests_.push_back(std::move(t));
    }
}

// ======= runner =======

int CLITestRunner::runAll(std::ostream& out) {
    int failures = 0;
    size_t idx = 0;
    for (const auto& t : tests_) {
        out << "[" << (++idx) << "/" << tests_.size() << "] " << t.name << "\n";
        out << "  $ " << t.command << "\n";
        CommandResult r = exec_(t.command);

        auto fail = [&](const std::string& msg){
            ++failures;
            out << "  ✗ " << msg << "\n";
        };

        if (r.exit_code != t.expect_exit) {
            fail("exit_code: expected " + std::to_string(t.expect_exit) +
                 " got " + std::to_string(r.exit_code));
            out << "  stdout: " << r.stdout_text << "\n";
            out << "  stderr: " << r.stderr_text << "\n";
            continue;
        }

        for (const auto& needle : t.must_contain) {
            if (r.stdout_text.find(needle) == std::string::npos &&
                r.stderr_text.find(needle) == std::string::npos) {
                fail("missing expected text: \"" + needle + "\"");
            }
        }

        for (const auto& needle : t.must_not_contain) {
            if (r.stdout_text.find(needle) != std::string::npos ||
                r.stderr_text.find(needle) != std::string::npos) {
                fail("found forbidden text: \"" + needle + "\"");
            }
        }

        for (const auto& v : t.validators) {
            auto ar = v(t.command, r, t.ctx);
            if (!ar.ok) fail("validator: " + ar.message);
        }

        if (failures == 0 || idx == tests_.size()) {
            out << "  ✓ ok\n";
        }
    }

    out << "\nSummary: " << (tests_.size() - failures) << " passed, "
        << failures << " failed, total " << tests_.size() << "\n";
    return failures;
}

}
