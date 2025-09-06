#include "CLITestRunner.hpp"
#include "CommandUsage.hpp"
#include "UsageManager.hpp"

#include <sstream>
#include <iostream>
#include <algorithm>

using namespace vh::shell;

namespace {
// trim helpers
std::string trim(std::string s) {
    auto wsfront = std::find_if_not(s.begin(), s.end(),
                                    [](unsigned char c){ return std::isspace(c); });
    auto wsback  = std::find_if_not(s.rbegin(), s.rend(),
                                    [](unsigned char c){ return std::isspace(c); }).base();
    if (wsfront >= wsback) return {};
    return std::string(wsfront, wsback);
}

// Choose a canonical flag from an Entry (prefer aliases; else parse label)
// label examples: "-n | --name <name>", "--role <role>", "--force"
std::string canonical_flag_from(const Entry& e) {
    // Prefer aliases if present (pick a long one; else any dash-prefixed)
    if (!e.aliases.empty()) {
        for (const auto& a : e.aliases) if (a.rfind("--", 0) == 0) return a;
        for (const auto& a : e.aliases) if (a.rfind("-",  0) == 0) return a;
        // fall through to label parsing if aliases are non-flags
    }

    std::string spec = e.label;
    // drop anything after the first '<' so we just have the flag variants
    if (auto lt = spec.find('<'); lt != std::string::npos) spec = spec.substr(0, lt);

    // split by '|'
    std::vector<std::string> parts;
    for (size_t i = 0, j; i <= spec.size(); i = j + 1) {
        j = spec.find('|', i);
        std::string p = trim(spec.substr(i, j == std::string::npos ? j : j - i));
        if (!p.empty()) parts.push_back(std::move(p));
        if (j == std::string::npos) break;
    }
    if (parts.empty()) return trim(spec);

    for (const auto& p : parts) if (p.rfind("--", 0) == 0) return p;
    for (const auto& p : parts) if (p.rfind("-",  0) == 0) return p;
    return parts.front(); // raw (shouldn’t happen for flags)
}
}

namespace vh::test {

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

void CLITestRunner::addTest(TestCase t) {
    tests_.push_back(std::move(t));
}

std::string CLITestRunner::primaryAlias(const std::shared_ptr<CommandUsage>& u) {
    if (!u || u->aliases.empty()) return {};
    return u->aliases.front();
}

std::string CLITestRunner::joinPath(const std::vector<std::string>& segs) {
    std::string out;
    for (size_t i = 0; i < segs.size(); ++i) {
        out += segs[i];
        if (i + 1 < segs.size()) out += '/';
    }
    return out;
}

bool CLITestRunner::isLeaf(const std::shared_ptr<CommandUsage>& u) {
    return !u || u->subcommands.empty();
}

void CLITestRunner::generateFromUsage(bool help_tests,
                                      bool happy_path,
                                      bool negative_required,
                                      size_t max_examples_per_cmd) {
    auto root = usage_.getFilteredTestUsage();
    std::vector<std::string> segs;
    traverse(root, segs, help_tests, happy_path, negative_required, max_examples_per_cmd);
}

void CLITestRunner::traverse(const std::shared_ptr<CommandUsage>& node,
                             std::vector<std::string>& path_aliases,
                             bool help_tests,
                             bool happy_path,
                             bool negative_required,
                             size_t max_examples) {
    if (!node) return;

    // Skip adding "vh" itself to the path segments; commands start after it
    if (primaryAlias(node) != "vh") {
        path_aliases.push_back(primaryAlias(node));
    }

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

void CLITestRunner::genHelpTest(const std::vector<std::string>& path_aliases,
                                const std::shared_ptr<CommandUsage>& u) {
    // "vh <path> --help"
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

    // Attach any per-path stdout contains / validators
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
        t.command     = ex.cmd;   // Example::cmd
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

std::vector<std::string> CLITestRunner::extractAngleTokens(const std::string& spec) {
    std::vector<std::string> out;
    // find <token> occurrences
    for (size_t i = 0; i < spec.size(); ++i) {
        if (spec[i] == '<') {
            size_t j = spec.find('>', i + 1);
            if (j != std::string::npos && j > i + 1) {
                out.push_back(spec.substr(i + 1, j - i - 1));
                i = j;
            }
        }
    }
    return out;
}

std::optional<std::string> CLITestRunner::buildArgs(const std::shared_ptr<CommandUsage>& u,
                                                    Context& ctx) {
    if (!u) return std::string{};
    std::ostringstream args;

    // 1) Required flags (Entry{label, desc, aliases})
    for (const auto& req : u->required) {
        const std::string& spec = req.label;          // "--name <name>", "-n | --name <name>", etc.
        const auto flag        = canonical_flag_from(req);
        const auto tokens      = extractAngleTokens(spec); // ["name"], ["role"], or []

        if (!flag.empty() && flag.rfind("-", 0) == 0) {
            if (tokens.empty()) {
                // boolean like "--force"
                args << ' ' << flag;
            } else {
                for (const auto& tok : tokens) {
                    auto val = provider_->valueFor(tok, /*usage_path*/"");
                    if (!val.has_value()) return std::nullopt;
                    ctx[tok] = *val;
                    args << ' ' << flag << ' ' << *val;
                }
            }
        } else {
            // In case someone put a positional-looking spec into required by mistake,
            // treat tokens as positionals so we don't crash.
            for (const auto& tok : tokens) {
                auto val = provider_->valueFor(tok, "");
                if (!val.has_value()) return std::nullopt;
                ctx[tok] = *val;
                args << ' ' << *val;
            }
        }
    }

    // 2) Positionals (Entry{label="<name>", ...})
    for (const auto& pos : u->positionals) {
        const auto tokens = extractAngleTokens(pos.label); // "<name>"
        if (tokens.empty()) {
            // Literal positional (rare). Append the label text as-is (trimmed).
            args << ' ' << trim(pos.label);
            continue;
        }
        for (const auto& tok : tokens) {
            auto val = provider_->valueFor(tok, /*usage_path*/"");
            if (!val.has_value()) return std::nullopt;
            ctx[tok] = *val;
            args << ' ' << *val;
        }
    }

    // Intentionally NOT auto-appending optionals; keep happy-path minimal/stable.
    return args.str();
}

void CLITestRunner::genHappyPathTest(const std::vector<std::string>& path_aliases,
                                     const std::shared_ptr<CommandUsage>& u) {
    // Build "vh <path> <required/positionals>"
    std::ostringstream cmd;
    for (auto& seg : path_aliases) cmd << ' ' << seg;

    Context ctx;
    auto argStr = buildArgs(u, ctx);
    if (!argStr.has_value()) {
        // Skip happy path if we cannot synthesize required args
        return;
    }
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

void CLITestRunner::genMissingRequiredTest(const std::vector<std::string>& path_aliases,
                                           const std::shared_ptr<CommandUsage>& u) {
    if (!u || u->required.empty()) return;

    std::ostringstream cmd;
    cmd << "vh";
    for (auto& seg : path_aliases) cmd << ' ' << seg;

    // Intentionally omit required flags to provoke an error
    TestCase t;
    t.name        = "neg:missing-required: " + joinPath(path_aliases);
    t.path        = joinPath(path_aliases);
    t.command     = cmd.str();
    t.expect_exit = 1; // assume non-zero on validation error

    tests_.push_back(std::move(t));
}

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
