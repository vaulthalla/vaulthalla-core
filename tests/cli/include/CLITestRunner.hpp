#pragma once

#include "protocols/shell/types.hpp"
#include "ArgsGenerator.hpp"

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
struct TestCommandUsage;
}

namespace vh::types {
struct User;
struct APIKey;
struct Vault;
struct Role;
struct Group;
}

namespace vh::test {

struct CLITestContext {
    std::vector<types::User> users;
    std::vector<types::APIKey> api_keys;
    std::vector<types::Vault> vaults;
    std::vector<types::Role> roles;
    std::vector<types::Group> groups;

    std::shared_ptr<types::User> pickRandomUser();
    std::shared_ptr<types::Vault> pickVaultOwnedBy(const std::shared_ptr<types::User>& user);
    std::shared_ptr<types::Role> pickRoleByName(std::string_view);
};

class TestUsageManager;

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

struct GenerateConfig {
    bool help_tests = true;
    bool happy_path = true;
    bool negative_required = true;
    bool negative_invalid_values = true;
    bool matrix_variants = true;
    std::size_t max_examples_per_cmd = 2;
    std::size_t max_variants_per_cmd = 3;

    // NEW knobs for test_usage-driven lifecycles
    bool test_usage_scenarios = true;
    bool lifecycle_use_max_iters = false; // default: deterministic minimal path
};

class CLITestRunner {
public:
    explicit CLITestRunner(TestUsageManager& usage,
                           ExecFn exec,
                           std::shared_ptr<args::ArgValueProvider> provider = std::make_shared<args::ArgsGeneratorProvider>(args::ArgsGenerator::WithDefaults()));

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
    TestUsageManager& usage_;
    ExecFn exec_;
    std::shared_ptr<args::ArgValueProvider> provider_;
    std::unordered_map<std::string, std::vector<Context>> open_objects_;

    std::unordered_map<std::string, std::vector<ValidatorFn>> per_path_validators_;
    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_contains_;
    std::unordered_map<std::string, std::vector<std::string>> per_path_stdout_not_contains_;
    std::vector<TestCase> tests_;

    static std::string dashify(const std::string& t);
    static std::string pickBestToken(const std::vector<std::string>& tokens); // prefers long
    static std::optional<std::string> firstValueFromTokens(const std::vector<std::string>& value_tokens,
                                                           args::ArgValueProvider& provider,
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

    // NEW: test_usage scenario pipeline
    void genTestUsageScenarios(const std::shared_ptr<vh::shell::CommandUsage>& root,
                               const GenerateConfig& cfg);

    void emitPhaseRun_(const std::vector<std::string>& base_path,
                       const std::vector<shell::TestCommandUsage>& phase,
                       const GenerateConfig& cfg,
                       bool is_teardown);

    static std::vector<std::string> pathAliasesOf(const std::shared_ptr<vh::shell::CommandUsage>& u);

    // Convenience: merges the “latest” ctx from all open paths for seeding
    Context mergedOpenContext_() const;
};

}
