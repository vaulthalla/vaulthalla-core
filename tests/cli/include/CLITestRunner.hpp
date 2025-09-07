#pragma once

#include "protocols/shell/types.hpp"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <regex>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <random>

namespace vh::shell {
class UsageManager;
class CommandUsage;
}

namespace vh::test {

using ExecFn = std::function<shell::CommandResult(const std::string& line)>;

struct AssertionResult {
    bool ok{true};
    std::string message;
    static AssertionResult Pass() { return {true, {}}; }
    static AssertionResult Fail(std::string msg) { return {false, std::move(msg)}; }
};

using Context = std::unordered_map<std::string, std::string>;
using ValidatorFn = std::function<AssertionResult(const std::string& cmd,
                                                 const shell::CommandResult& res,
                                                 const Context& ctx)>;

struct TestCase {
    std::string name;          // Human friendly
    std::string path;          // e.g. "user/create"
    std::string command;       // e.g. "vh user create --name foo --role admin"
    int expect_exit{0};
    std::vector<std::string> must_contain;     // stdout contains
    std::vector<std::string> must_not_contain; // stdout must NOT contain
    std::vector<ValidatorFn> validators;       // DB/cache checks, etc.
    Context ctx;                                // generated values (e.g., name/email)
};

class ArgValueProvider {
public:
    virtual ~ArgValueProvider() = default;
    // token is the name inside <angle_brackets>, e.g. "name", "email", "role", "uid"
    virtual std::optional<std::string> valueFor(const std::string& token,
                                                const std::string& usage_path) = 0;
};

class DefaultArgValueProvider : public ArgValueProvider {
public:
    std::optional<std::string> valueFor(const std::string& token,
                                        const std::string& /*usage_path*/) override {
        auto rnd = []() -> std::string {
            static thread_local std::mt19937_64 rng(
                std::chrono::high_resolution_clock::now().time_since_epoch().count());
            return std::to_string(rng() % 1000000ULL);
        };
        if (token == "name" || token == "username") return "user_" + rnd();
        if (token == "new_name") return "user_new_" + rnd();
        if (token == "email") return "user_" + rnd() + "@example.org";
        if (token == "role") return "admin";          // override per-env if needed
        if (token == "uid" || token == "linux-uid") return "2000";
        if (token == "vault_id" || token == "id") return "1";
        if (token == "size" || token == "quota") return "10G";
        if (token == "permissions") return "15"; // 0b1111
        // Unknown token → give nothing; caller decides whether to skip positive test
        return std::nullopt;
    }
};

struct GenerateConfig {
    bool help_tests = true;
    bool happy_path = true;
    bool negative_required = true;
    bool negative_invalid_values = true;
    bool matrix_variants = true;          // exercise alt tokens / value tokens
    std::size_t max_examples_per_cmd = 2; // cap example-based tests per cmd
    std::size_t max_variants_per_cmd = 3; // cap matrix tests per cmd
};

class CLITestRunner {
public:
    explicit CLITestRunner(vh::shell::UsageManager& usage,
                           ExecFn exec,
                           std::shared_ptr<ArgValueProvider> provider = std::make_shared<DefaultArgValueProvider>());

    // Generate tests by traversing the usage tree
    void generateFromUsage(bool help_tests = true,
                           bool happy_path = true,
                           bool negative_required = true,
                           size_t max_examples_per_cmd = 2);

    void generateFromUsage(const GenerateConfig& cfg);

    // Register extra validators for a path, e.g. "user/create"
    void registerValidator(const std::string& path, ValidatorFn v);

    // Register a canned STDOUT checker (contains / not contains) for a path
    void registerStdoutContains(const std::string& path, std::string needle);
    void registerStdoutNotContains(const std::string& path, std::string needle);

    // Directly add a bespoke test
    void addTest(TestCase t);

    // Run everything; returns number of failures
    int runAll(std::ostream& out);

    // Expose collected tests (for filtering, etc.)
    const std::vector<TestCase>& tests() const { return tests_; }

private:
    vh::shell::UsageManager& usage_;
    ExecFn exec_;
    std::shared_ptr<ArgValueProvider> provider_;

    std::unordered_map<std::string, std::vector<ValidatorFn>> per_path_validators_;
    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_contains_;
    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_not_contains_;
    std::vector<TestCase> tests_;

    static std::string dashify(const std::string& t);
    static std::string pickBestToken(const std::vector<std::string>& tokens); // prefers long
    static std::optional<std::string> firstValueFromTokens(const std::vector<std::string>& value_tokens,
                                                           ArgValueProvider& provider,
                                                           const std::string& usage_path,
                                                           Context& ctx,
                                                           std::string* chosen_token_out = nullptr);

    static std::string usagePathFor(const std::vector<std::string>& path_aliases);

    // richer generators
    void genMatrixVariants(const std::vector<std::string>& path_aliases,
                           const std::shared_ptr<shell::CommandUsage>& u,
                           const GenerateConfig& cfg);

    void genMissingEachRequiredTest(const std::vector<std::string>& path_aliases,
                                    const std::shared_ptr<shell::CommandUsage>& u);

    void genInvalidValueTests(const std::vector<std::string>& path_aliases,
                              const std::shared_ptr<shell::CommandUsage>& u,
                              std::size_t max_variants);

    static std::string primaryAlias(const std::shared_ptr<vh::shell::CommandUsage>& u);
    static std::string joinPath(const std::vector<std::string>& segs);
    static bool isLeaf(const std::shared_ptr<vh::shell::CommandUsage>& u);

    void traverse(const std::shared_ptr<vh::shell::CommandUsage>& node,
                  std::vector<std::string>& path_aliases,
                  bool help_tests,
                  bool happy_path,
                  bool negative_required,
                  size_t max_examples);

    void genHelpTest(const std::vector<std::string>& path_aliases,
                     const std::shared_ptr<vh::shell::CommandUsage>& u);

    void genExampleTests(const std::vector<std::string>& path_aliases,
                         const std::shared_ptr<vh::shell::CommandUsage>& u,
                         size_t max_examples);

    void genHappyPathTest(const std::vector<std::string>& path_aliases,
                          const std::shared_ptr<vh::shell::CommandUsage>& u);

    void genMissingRequiredTest(const std::vector<std::string>& path_aliases,
                                const std::shared_ptr<vh::shell::CommandUsage>& u);

    // Build args string for required/positional using provider; fills context
    // Returns std::nullopt if a required token couldn't be satisfied
    std::optional<std::string> buildArgs(const std::shared_ptr<vh::shell::CommandUsage>& u,
                                         Context& out_ctx);
    static std::vector<std::string> extractAngleTokens(const std::string& spec);
};

}
