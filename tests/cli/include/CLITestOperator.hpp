#pragma once

#include "protocols/shell/types.hpp"
#include "tests/cli/include/CLITestRunner.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>

namespace vh::test {

struct CaptureRule {
    std::regex pattern;     // e.g. R"(Created user\s+(\d+))"
    std::string key;        // e.g. "user_id"
    int group{1};           // capture group
};

struct Step {
    std::string name;               // "user/create"
    std::string commandTemplate;    // e.g. "vh user create --name {user_name} --role {role_name}"
    std::vector<CaptureRule> captures;
    std::vector<ValidatorFn> validators;
    // Optional: extra required placeholders for validation dependency ordering
    // Optional: step-specific allow-failure (for edge case probes)
    bool allowFailure{false};
};

struct Scenario {
    std::string persona;            // "admin", "power_user"
    std::vector<Step> forward;      // creation/use steps
    std::vector<Step> cleanup;      // reverse deletions
};

class CLITestOperator {
public:
    CLITestOperator(ExecFn exec,
                    std::shared_ptr<ArgValueProvider> provider,
                    std::string name = "op")
    : exec_(std::move(exec)), provider_(std::move(provider)), name_(std::move(name)) {}

    void addScenario(Scenario s) { scenarios_.push_back(std::move(s)); }

    int runAll(std::ostream& out);

    // shared state for captures
    const Context& context() const { return ctx_; }

private:
    ExecFn exec_;
    std::shared_ptr<ArgValueProvider> provider_;
    std::string name_;
    std::vector<Scenario> scenarios_;
    Context ctx_;

    std::string render(const std::string& tmpl);
    void applyCaptures(const std::vector<CaptureRule>& caps, const shell::CommandResult& r);
    AssertionResult runStep(const Step& s, std::ostream& out);
};


}
