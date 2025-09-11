#include "CLITestRunner.hpp"
#include "TestUsageManager.hpp"
#include "logging/LogRegistry.hpp"
#include "CLITestContext.hpp"
#include "Validator.hpp"
#include "EntityRegistrar.hpp"
#include "TestCase.hpp"
#include "CommandRouter.hpp"
#include "CommandBuilderRegistry.hpp"

#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sys/ioctl.h>

using namespace vh::shell;
using namespace vh::args;

namespace vh::test::cli {

CLITestRunner::CLITestRunner(CLITestConfig&& config)
    : config_(config),
      ctx_(std::make_shared<CLITestContext>()),
      usage_(std::make_shared<TestUsageManager>()),
      router_(std::make_shared<CommandRouter>(ctx_)) {
    CommandBuilderRegistry::init(usage_, ctx_);
}

void CLITestRunner::registerStdoutContains(const std::string& path, std::string s) {
    per_path_stdout_contains_[path].push_back(std::move(s));
}
void CLITestRunner::registerStdoutNotContains(const std::string& path, std::string s) {
    per_path_stdout_not_contains_[path].push_back(std::move(s));
}

void CLITestRunner::registerStdoutContains(const std::string& path, std::vector<std::string> needles) {
    auto& vec = per_path_stdout_contains_[path];
    vec.insert(vec.end(), std::make_move_iterator(needles.begin()), std::make_move_iterator(needles.end()));
}

void CLITestRunner::registerStdoutNotContains(const std::string& path, std::vector<std::string> needles) {
    auto& vec = per_path_stdout_not_contains_[path];
    vec.insert(vec.end(), std::make_move_iterator(needles.begin()), std::make_move_iterator(needles.end()));
}

int CLITestRunner::operator()() {
    seedRoles();
    seedUsers();
    seedGroups();
    seedVaults();
    read();
    update();
    validateTestObjects();
    teardown();
    return printResults();
}

static std::string errToStr(const std::vector<std::string>& errors) {
    std::ostringstream oss;
    for (const auto& e : errors) oss << e << "\n";
    return oss.str();
}

void CLITestRunner::validateResponse(const unsigned int stage) const {
    const auto stageName = kDefaultTestStages[stage].name;
    const auto tests = kDefaultTestStages[stage].tests;
    for (const auto& test : tests) {
        if (test->expect_exit != test->result.exit_code) {
            test->assertion = AssertionResult::Fail(stageName + ": unexpected exit code for " + test->name);
            continue;
        }
        std::vector<std::string> errors;
        if (per_path_stdout_contains_.contains(test->path))
            for (const auto& needle : per_path_stdout_contains_.at(test->path))
                if (!test->result.stdout_text.contains(needle))
                    errors.push_back(stageName + ": missing expected text for " + test->name + ": " + needle);
        if (per_path_stdout_not_contains_.contains(test->path))
            for (const auto& needle : per_path_stdout_not_contains_.at(test->path))
                if (test->result.stdout_text.contains(needle))
                    errors.push_back(stageName + ": found forbidden text for " + test->name + ": " + needle);
        if (errors.empty()) test->assertion = AssertionResult::Pass();
        else test->assertion = AssertionResult::Fail(errToStr(errors));
    }
}

static std::optional<unsigned int> extractId(const std::string& output, const std::string& idPrefix = "ID: ") {
    const auto pos = output.find(idPrefix);
    if (pos == std::string::npos) return std::nullopt;
    const auto start = pos + idPrefix.size();
    const auto end = output.find('\n', start);
    if (end == std::string::npos) return std::nullopt;
    try {
        return std::stoul(output.substr(start, end - start));
    } catch (...) {
        return std::nullopt;
    }
}

void CLITestRunner::seedRoles() {
    std::vector<std::shared_ptr<TestCase>> tests;
    const size_t numTests = config_.numUserRoles + config_.numVaultRoles;
    tests.reserve(numTests);

    for (size_t i = 0; i < config_.numVaultRoles; ++i)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::VAULT_ROLE, CommandType::CREATE)));
    for (size_t i = 0; i < config_.numUserRoles; ++i)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::USER_ROLE, CommandType::CREATE)));

    const auto res = router_->route(tests);
    for (const auto& r : res) {
        if (!r->result.stderr_text.empty()) std::cerr << r->result.stderr_text << std::endl;
        if (const auto id = extractId(r->result.stdout_text, "Role ID:"); id.has_value() && r->entity) {
            if (r->path.contains("user")) {
                const auto role = std::static_pointer_cast<UserRole>(r->entity);
                role->id = *id;
                ctx_->userRoles.push_back(role);
            } else if (r->path.contains("vault")) {
                const auto role = std::static_pointer_cast<VaultRole>(r->entity);
                role->id = *id;
                ctx_->vaultRoles.push_back(role);
            }
        } else {
            r->assertion = AssertionResult::Fail("Seed: failed to extract ID from output for " + r->name);
        }
    }

    kDefaultTestStages[0] = TestStage{
        .name = "Seed Roles",
        .tests = res
    };

    validateResponse(0);
}

void CLITestRunner::seedUsers() {
    std::vector<std::shared_ptr<TestCase>> tests;
    tests.reserve(config_.numUsers);

    for (size_t i = 0; i < config_.numUsers; ++i)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::USER, CommandType::CREATE)));

    const auto res = router_->route(tests);
    if (res.empty()) throw std::runtime_error("Seed Users: no test results from routing");
    for (const auto& r : res) {
        if (!r->result.stderr_text.empty()) std::cerr << r->result.stderr_text << std::endl;
        if (const auto id = extractId(r->result.stdout_text, "User ID:"); id.has_value() && r->entity) {
            const auto user = std::static_pointer_cast<User>(r->entity);
            user->id = *id;
            ctx_->users.push_back(user);
        } else std::cerr << "Seed Users: failed to extract ID from output for " + r->name + " - output:\n" << r->result.stdout_text << std::endl;
    }

    kDefaultTestStages[1] = TestStage{
        .name = "Seed Users",
        .tests = res
    };

    validateResponse(1);
}

void CLITestRunner::seedGroups() {
    std::vector<std::shared_ptr<TestCase>> tests;
    tests.reserve(config_.numGroups);

    for (size_t i = 0; i < config_.numGroups; ++i)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::GROUP, CommandType::CREATE)));

    const auto res = router_->route(tests);
    if (res.empty()) throw std::runtime_error("Seed Groups: no test results from routing");
    for (const auto& r : res) {
        if (!r->result.stderr_text.empty()) std::cerr << r->result.stderr_text << std::endl;
        if (const auto id = extractId(r->result.stdout_text); id.has_value() && r->entity) {
            const auto group = std::static_pointer_cast<Group>(r->entity);
            group->id = *id;
            ctx_->groups.push_back(group);
        } else std::cerr << "Seed Groups: failed to extract ID from output for " + r->name << " - output:\n" << r->result.stdout_text << std::endl;
    }

    kDefaultTestStages[2] = TestStage{
        .name = "Seed Groups",
        .tests = res
    };

    validateResponse(2);
}

void CLITestRunner::seedVaults() {
    std::vector<std::shared_ptr<TestCase>> tests;
    tests.reserve(config_.numVaults);

    for (size_t i = 0; i < config_.numVaults; ++i)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::VAULT, CommandType::CREATE)));

    const auto res = router_->route(tests);
    if (res.empty()) throw std::runtime_error("Seed Vaults: no test results from routing");
    for (const auto& r : res) {
        if (!r->result.stderr_text.empty()) std::cerr << r->result.stderr_text << std::endl;
        if (const auto id = extractId(r->result.stdout_text); id.has_value() && r->entity) {
            const auto vault = std::static_pointer_cast<Vault>(r->entity);
            vault->id = *id;
            ctx_->vaults.push_back(vault);
        } else std::cerr << "Seed Vaults: failed to extract ID from output for " + r->name << " - output:\n" << r->result.stdout_text << std::endl;
    }

    kDefaultTestStages[3] = TestStage{
        .name = "Seed Vaults",
        .tests = res
    };

    validateResponse(3);
}

void CLITestRunner::read() {
    std::vector<std::shared_ptr<TestCase>> tests;
    size_t numTests = ctx_->users.size() + ctx_->vaults.size() + ctx_->groups.size() +
                      ctx_->userRoles.size() + ctx_->vaultRoles.size() + 5;
    tests.reserve(numTests + CLITestContext::ENTITIES.size());

    for (const auto& u : ctx_->users)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::USER, CommandType::INFO, u)));
    for (const auto& v : ctx_->vaults)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::VAULT, CommandType::INFO, v)));
    for (const auto& g : ctx_->groups)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::GROUP, CommandType::INFO, g)));
    for (const auto& r : ctx_->userRoles)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::USER_ROLE, CommandType::INFO, r)));
    for (const auto& r : ctx_->vaultRoles)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::VAULT_ROLE, CommandType::INFO, r)));

    tests.push_back(std::make_shared<TestCase>(TestCase::List(EntityType::USER)));
    tests.push_back(std::make_shared<TestCase>(TestCase::List(EntityType::VAULT)));
    tests.push_back(std::make_shared<TestCase>(TestCase::List(EntityType::GROUP)));
    tests.push_back(std::make_shared<TestCase>(TestCase::List(EntityType::USER_ROLE)));
    tests.push_back(std::make_shared<TestCase>(TestCase::List(EntityType::VAULT_ROLE)));

    kDefaultTestStages[4] = TestStage{
        .name = "Read",
        .tests = router_->route(tests)
    };

    validateResponse(4);
}

void CLITestRunner::update() {
    std::vector<std::shared_ptr<TestCase>> tests;
    size_t numTests = ctx_->users.size() + ctx_->vaults.size() + ctx_->groups.size() +
                      ctx_->userRoles.size() + ctx_->vaultRoles.size();
    tests.reserve(numTests);

    for (const auto& u : ctx_->users)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::USER, CommandType::UPDATE, u)));
    for (const auto& v : ctx_->vaults)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::VAULT, CommandType::UPDATE, v)));
    for (const auto& g : ctx_->groups)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::GROUP, CommandType::UPDATE, g)));
    for (const auto& r : ctx_->userRoles)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::USER, CommandType::UPDATE, r)));
    for (const auto& r : ctx_->vaultRoles)
        tests.push_back(std::make_shared<TestCase>(TestCase::Generate(EntityType::VAULT_ROLE, CommandType::UPDATE, r)));

    kDefaultTestStages[5] = TestStage{
        .name = "Update",
        .tests = router_->route(tests)
    };

    validateResponse(5);
}

void CLITestRunner::validateTestObjects() const {
    Validator<EntityType::USER, User>::assert_all_exist(ctx_->users);
    Validator<EntityType::VAULT, Vault>::assert_all_exist(ctx_->vaults);
    Validator<EntityType::GROUP, Group>::assert_all_exist(ctx_->groups);
    Validator<EntityType::USER_ROLE, UserRole>::assert_all_exist(ctx_->userRoles);
    Validator<EntityType::VAULT_ROLE, VaultRole>::assert_all_exist(ctx_->vaultRoles);
}

void CLITestRunner::teardown() {
    std::vector<std::shared_ptr<TestCase>> tests;
    const size_t numTests = ctx_->users.size() + ctx_->vaults.size() + ctx_->groups.size() +
                      ctx_->userRoles.size() + ctx_->vaultRoles.size();
    tests.reserve(numTests);

    for (const auto& r : ctx_->userRoles)
        tests.push_back(std::make_shared<TestCase>(TestCase::Delete(EntityType::USER_ROLE, r)));
    for (const auto& g : ctx_->groups)
        tests.push_back(std::make_shared<TestCase>(TestCase::Delete(EntityType::GROUP, g)));
    for (const auto& u : ctx_->users)
        tests.push_back(std::make_shared<TestCase>(TestCase::Delete(EntityType::USER, u)));
    for (const auto& r : ctx_->vaultRoles)
        tests.push_back(std::make_shared<TestCase>(TestCase::Delete(EntityType::VAULT_ROLE, r)));
    for (const auto& v : ctx_->vaults)
        tests.push_back(std::make_shared<TestCase>(TestCase::Delete(EntityType::VAULT, v)));

    kDefaultTestStages[6] = TestStage{
        .name = "Teardown",
        .tests = router_->route(tests)
    };

    validateResponse(6);
}

int CLITestRunner::printResults() const {
    std::ostream& os = std::cout;

    // --- TTY / color detection
    auto color_enabled = []() -> bool {
        if (std::getenv("NO_COLOR")) return false;             // https://no-color.org
        if (std::getenv("CLICOLOR_FORCE")) return true;        // force on
        // require a TTY
        if (!isatty(fileno(stdout))) return false;
        const char* term = std::getenv("TERM");
        if (!term) return false;
        if (std::string(term) == "dumb") return false;
        return true;
    }();

    // --- ANSI styles (no-op if disabled)
    const char* reset = color_enabled ? "\033[0m"  : "";
    const char* bold  = color_enabled ? "\033[1m"  : "";
    // const char* dim   = color_enabled ? "\033[2m"  : "";
    const char* red   = color_enabled ? "\033[31m" : "";
    const char* green = color_enabled ? "\033[32m" : "";
    const char* yellow= color_enabled ? "\033[33m" : "";
    const char* cyan  = color_enabled ? "\033[36m" : "";
    const char* gray  = color_enabled ? "\033[90m" : "";

    // UTF-8 glyphs with ASCII fallback
    const char* ok_glyph    = color_enabled ? "✔" : "OK";
    const char* fail_glyph  = color_enabled ? "✘" : "X";

    // --- terminal width (best effort)
    constexpr int term_cols = 100;
    auto hr = [&](char c = '-') {
        for (int i = 0; i < term_cols; ++i) os << c;
        os << '\n';
    };

    // --- Totals
    int total = 0;
    int passed = 0;
    int failed = 0;

    // Header
    os << bold << "CLI Test Results" << reset << "\n";
    hr();

    // Stage-by-stage
    for (const auto& stage : kDefaultTestStages) {
        if (stage.name.empty()) continue;

        int stage_total = 0;
        int stage_pass = 0;
        int stage_fail = 0;

        os << bold << stage.name << reset << "\n";

        for (const auto& t : stage.tests) {
            if (!t) continue;
            ++stage_total; ++total;

            const bool ok = t->assertion.ok;
            if (ok) { ++stage_pass; ++passed; } else { ++stage_fail; ++failed; }

            // Left status "✔ PASS" or "✘ FAIL"
            const char* col = ok ? green : red;
            os << "  " << col << (ok ? ok_glyph : fail_glyph) << ' '
               << (ok ? "PASS" : "FAIL") << reset
               << "  " << bold << t->name << reset;

            // Helpful note if exit code mismatched
            if (t->expect_exit != t->result.exit_code) {
                os << ' ' << yellow << "[exit " << t->result.exit_code
                   << " ≠ expected " << t->expect_exit << ']' << reset;
            }
            os << '\n';

            // Failure details (multi-line friendly)
            if (!ok && !t->assertion.message.empty()) {
                std::istringstream iss(t->assertion.message);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) continue;
                    os << "      " << yellow << "• " << reset << line << '\n';
                }
            }
        }

        // Stage summary
        os << "  " << cyan << "Stage summary:" << reset
           << " " << stage_pass << "/" << stage_total << " passed";
        if (stage_fail) os << "  " << red << stage_fail << " failed" << reset;
        os << '\n';

        // Light divider between stages
        os << gray; hr(); os << reset;
    }

    // Overall summary
    os << bold << "Overall: "
       << (failed ? red : green) << (total - failed) << "/" << total << " passed"
       << reset << "\n";

    // Non-zero return for failures (also conveys # of failures)
    return failed;
}

}
