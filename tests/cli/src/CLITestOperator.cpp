#include "CLITestOperator.hpp"

using namespace vh::test;
using namespace vh::shell;

std::string CLITestOperator::render(const std::string& tmpl) {
    // Replace {keys} using ctx_ and provider_ fallback
    std::string out = tmpl;
    static const std::regex kRe(R"(\{([a-zA-Z0-9_\-]+)\})");
    std::smatch m;
    std::string result; result.reserve(out.size()+32);
    auto it = out.cbegin();
    while (std::regex_search(it, out.cend(), m, kRe)) {
        result.append(it, m.prefix().second);
        const std::string key = m[1].str();
        auto found = ctx_.find(key);
        if (found != ctx_.end()) {
            result += found->second;
        } else {
            auto v = provider_->valueFor(key, /*usage_path*/"");
            result += v.value_or(key); // if unknown, emit key name (visible failure)
            if (v) ctx_[key] = *v;
        }
        it = m.suffix().first;
    }
    result.append(it, out.cend());
    return result;
}

void CLITestOperator::applyCaptures(const std::vector<CaptureRule>& caps,
                                    const shell::CommandResult& r) {
    auto scan = [&](const std::string& text) {
        for (const auto& c : caps) {
            std::smatch m;
            if (std::regex_search(text, m, c.pattern) && m.size() > c.group) {
                ctx_[c.key] = m[c.group].str();
            }
        }
    };
    scan(r.stdout_text);
    scan(r.stderr_text);
}

AssertionResult CLITestOperator::runStep(const Step& s, std::ostream& out) {
    std::string line = render(s.commandTemplate);
    out << "  $ " << line << "\n";
    auto res = exec_(line);

    if (res.exit_code != 0 && !s.allowFailure) {
        return AssertionResult::Fail("exit_code=" + std::to_string(res.exit_code));
    }

    applyCaptures(s.captures, res);
    for (const auto& v : s.validators) {
        auto ar = v(line, res, ctx_);
        if (!ar.ok) return ar;
    }
    return AssertionResult::Pass();
}

int CLITestOperator::runAll(std::ostream& out) {
    int failures = 0;
    for (const auto& sc : scenarios_) {
        out << "[scenario] " << sc.persona << "\n";
        std::vector<Step> executed;

        // forward
        for (const auto& step : sc.forward) {
            auto ar = runStep(step, out);
            if (!ar.ok) {
                out << "  ✗ " << step.name << ": " << ar.message << "\n";
                ++failures;
                break;
            }
            out << "  ✓ " << step.name << "\n";
            executed.push_back(step);
        }

        // cleanup reverse (even after failure, try best-effort cleanup)
        for (auto it = sc.cleanup.rbegin(); it != sc.cleanup.rend(); ++it) {
            auto ar = runStep(*it, out);
            if (!ar.ok) out << "  (cleanup) " << it->name << " failed: " << ar.message << "\n";
        }
    }
    return failures;
}

