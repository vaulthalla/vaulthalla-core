#include "CLITestRunner.hpp"

#include "TestUsageManager.hpp"
#include "logging/LogRegistry.hpp"
#include "CLITestContext.hpp"
#include "Validator.hpp"
#include "EntityRegistrar.hpp"
#include "TestCase.hpp"
#include "CommandRouter.hpp"
#include "CommandBuilderRegistry.hpp"
#include "EntityType.hpp"

#include "types/User.hpp"
#include "types/Group.hpp"
#include "types/Vault.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unistd.h>

using namespace vh::shell;
using namespace vh::args;


// forward decls so casts compile even if headers aren’t pulled here
namespace vh::types {
struct User;
struct Group;
struct Vault;
struct UserRole;
struct VaultRole;
}


namespace vh::test::cli {

// ---------- Small utilities

static std::string joinLines(const std::vector<std::string>& lines) {
    std::ostringstream oss;
    for (const auto& e : lines) oss << e << '\n';
    return oss.str();
}

std::optional<unsigned int>
CLITestRunner::extractId(std::string_view output, std::string_view idPrefix) {
    const auto pos = output.find(idPrefix);
    if (pos == std::string_view::npos) return std::nullopt;
    const auto start = pos + idPrefix.size();
    const auto nl = output.find('\n', start);
    const auto end = (nl == std::string_view::npos) ? output.size() : nl;
    try {
        return static_cast<unsigned int>(std::stoul(std::string(output.substr(start, end - start))));
    } catch (...) {
        return std::nullopt;
    }
}

// ---------- Traits describing each entity bucket

template <EntityType E> struct EntityTraits;

template <> struct EntityTraits<EntityType::USER> {
    using Type = vh::types::User;
    static constexpr std::string_view kStage = "Users";
    static constexpr std::string_view kIdPrefix = "User ID:";
    static std::vector<std::shared_ptr<Type>>& vec(CLITestContext& c) { return c.users; }
};

template <> struct EntityTraits<EntityType::GROUP> {
    using Type = vh::types::Group;
    static constexpr std::string_view kStage = "Groups";
    static constexpr std::string_view kIdPrefix = "ID:";
    static std::vector<std::shared_ptr<Type>>& vec(CLITestContext& c) { return c.groups; }
};

template <> struct EntityTraits<EntityType::VAULT> {
    using Type = vh::types::Vault;
    static constexpr std::string_view kStage = "Vaults";
    static constexpr std::string_view kIdPrefix = "ID:";
    static std::vector<std::shared_ptr<Type>>& vec(CLITestContext& c) { return c.vaults; }
};

template <> struct EntityTraits<EntityType::USER_ROLE> {
    using Type = vh::types::UserRole;
    static constexpr std::string_view kStage = "User Roles";
    static constexpr std::string_view kIdPrefix = "Role ID:";
    static std::vector<std::shared_ptr<Type>>& vec(CLITestContext& c) { return c.userRoles; }
};

// VaultRole
template <> struct EntityTraits<EntityType::VAULT_ROLE> {
    using Type = vh::types::VaultRole;
    static constexpr std::string_view kStage = "Vault Roles";
    static constexpr std::string_view kIdPrefix = "Role ID:";
    static std::vector<std::shared_ptr<Type>>& vec(CLITestContext& c) { return c.vaultRoles; }
};

// ---------- Tiny generic helpers (local to this TU)

template <EntityType E>
static std::vector<std::shared_ptr<TestCase>>
makeCreateTests(std::size_t count) {
    std::vector<std::shared_ptr<TestCase>> v;
    v.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        v.push_back(std::make_shared<TestCase>(TestCase::Generate(E, CommandType::CREATE)));
    }
    return v;
}

template <EntityType E>
static std::vector<std::shared_ptr<TestCase>>
makeInfoTests(const std::vector<std::shared_ptr<typename EntityTraits<E>::Type>>& src) {
    std::vector<std::shared_ptr<TestCase>> v;
    v.reserve(src.size());
    for (const auto& e : src) {
        v.push_back(std::make_shared<TestCase>(TestCase::Generate(E, CommandType::INFO, e)));
    }
    return v;
}

template <EntityType E>
static std::vector<std::shared_ptr<TestCase>>
makeUpdateTests(const std::vector<std::shared_ptr<typename EntityTraits<E>::Type>>& src) {
    std::vector<std::shared_ptr<TestCase>> v;
    v.reserve(src.size());
    for (const auto& e : src) {
        v.push_back(std::make_shared<TestCase>(TestCase::Generate(E, CommandType::UPDATE, e)));
    }
    return v;
}

template <EntityType E>
static std::vector<std::shared_ptr<TestCase>>
makeDeleteTests(const std::vector<std::shared_ptr<typename EntityTraits<E>::Type>>& src) {
    std::vector<std::shared_ptr<TestCase>> v;
    v.reserve(src.size());
    for (const auto& e : src) {
        v.push_back(std::make_shared<TestCase>(TestCase::Delete(E, e)));
    }
    return v;
}

// one LIST per-entity type
template <EntityType E>
static std::shared_ptr<TestCase> makeListTest() {
    return std::make_shared<TestCase>(TestCase::List(E));
}

// Inject IDs into ctx after CREATE
template <EntityType E>
static void harvestIdsIntoContext(
    CLITestContext& ctx,
    const std::vector<std::shared_ptr<TestCase>>& results,
    std::string_view idPrefix,
    std::ostream& err
) {
    using T = typename EntityTraits<E>::Type;
    auto& bucket = EntityTraits<E>::vec(ctx);

    for (const auto& r : results) {
        if (!r) continue;
        if (!r->result.stderr_text.empty()) err << r->result.stderr_text << '\n';

        if (const auto id = CLITestRunner::extractId(r->result.stdout_text, idPrefix); id.has_value() && r->entity) {
            const auto obj = std::static_pointer_cast<T>(r->entity);
            obj->id = *id;
            bucket.push_back(obj);
        } else {
            r->assertion = AssertionResult::Fail("Seed: failed to extract ID from output for " + r->name);
        }
    }
}

// ---------- Runner

CLITestRunner::CLITestRunner(CLITestConfig&& cfg)
    : config_(cfg),
      ctx_(std::make_shared<CLITestContext>()),
      usage_(std::make_shared<TestUsageManager>()),
      router_(std::make_shared<CommandRouter>(ctx_)) {
    CommandBuilderRegistry::init(usage_, ctx_);
}

// expectations API
void CLITestRunner::registerStdoutContains(const std::string& path, std::string needle) {
    expectations_by_path_[path].must_have.push_back(std::move(needle));
}
void CLITestRunner::registerStdoutNotContains(const std::string& path, std::string needle) {
    expectations_by_path_[path].must_not_have.push_back(std::move(needle));
}
void CLITestRunner::registerStdoutContains(const std::string& path, std::vector<std::string> needles) {
    auto& e = expectations_by_path_[path].must_have;
    e.insert(e.end(),
             std::make_move_iterator(needles.begin()),
             std::make_move_iterator(needles.end()));
}
void CLITestRunner::registerStdoutNotContains(const std::string& path, std::vector<std::string> needles) {
    auto& e = expectations_by_path_[path].must_not_have;
    e.insert(e.end(),
             std::make_move_iterator(needles.begin()),
             std::make_move_iterator(needles.end()));
}

// ----- pipeline

int CLITestRunner::operator()() {
    seedUserRoles();
    seedVaultRoles();
    seedUsers();
    seedGroups();
    seedVaults();
    readStage();
    updateStage();
    validateAllTestObjects();
    teardownStage();
    return printResults();
}

void CLITestRunner::seedUserRoles() {
    auto tests = makeCreateTests<EntityType::USER_ROLE>(config_.numUserRoles);
    auto res   = router_->route(tests);

    harvestIdsIntoContext<EntityType::USER_ROLE>(*ctx_, res, EntityTraits<EntityType::USER_ROLE>::kIdPrefix, std::cerr);

    stages_.push_back(TestStage{ std::string("Seed ") + std::string(EntityTraits<EntityType::USER_ROLE>::kStage), res });
    validateStage(stages_.back());
}

void CLITestRunner::seedVaultRoles() {
    auto tests = makeCreateTests<EntityType::VAULT_ROLE>(config_.numVaultRoles);
    auto res   = router_->route(tests);

    harvestIdsIntoContext<EntityType::VAULT_ROLE>(*ctx_, res, EntityTraits<EntityType::VAULT_ROLE>::kIdPrefix, std::cerr);

    stages_.push_back(TestStage{ std::string("Seed ") + std::string(EntityTraits<EntityType::VAULT_ROLE>::kStage), res });
    validateStage(stages_.back());
}

void CLITestRunner::seedUsers() {
    auto tests = makeCreateTests<EntityType::USER>(config_.numUsers);
    auto res   = router_->route(tests);

    harvestIdsIntoContext<EntityType::USER>(*ctx_, res, EntityTraits<EntityType::USER>::kIdPrefix, std::cerr);

    stages_.push_back(TestStage{ std::string("Seed ") + std::string(EntityTraits<EntityType::USER>::kStage), res });
    validateStage(stages_.back());
}

void CLITestRunner::seedGroups() {
    auto tests = makeCreateTests<EntityType::GROUP>(config_.numGroups);
    auto res   = router_->route(tests);

    harvestIdsIntoContext<EntityType::GROUP>(*ctx_, res, EntityTraits<EntityType::GROUP>::kIdPrefix, std::cerr);

    stages_.push_back(TestStage{ std::string("Seed ") + std::string(EntityTraits<EntityType::GROUP>::kStage), res });
    validateStage(stages_.back());
}

void CLITestRunner::seedVaults() {
    auto tests = makeCreateTests<EntityType::VAULT>(config_.numVaults);
    auto res   = router_->route(tests);

    harvestIdsIntoContext<EntityType::VAULT>(*ctx_, res, EntityTraits<EntityType::VAULT>::kIdPrefix, std::cerr);

    stages_.push_back(TestStage{ std::string("Seed ") + std::string(EntityTraits<EntityType::VAULT>::kStage), res });
    validateStage(stages_.back());
}

void CLITestRunner::readStage() {
    std::vector<std::shared_ptr<TestCase>> tests;

    // INFO for each entity
    {
        auto& C = *ctx_;
        auto append = [&](auto&& vec) {
            tests.insert(tests.end(), vec.begin(), vec.end());
        };
        append(makeInfoTests<EntityType::USER>(C.users));
        append(makeInfoTests<EntityType::VAULT>(C.vaults));
        append(makeInfoTests<EntityType::GROUP>(C.groups));
        append(makeInfoTests<EntityType::USER_ROLE>(C.userRoles));
        append(makeInfoTests<EntityType::VAULT_ROLE>(C.vaultRoles));
    }

    // LIST per entity type
    tests.push_back(makeListTest<EntityType::USER>());
    tests.push_back(makeListTest<EntityType::VAULT>());
    tests.push_back(makeListTest<EntityType::GROUP>());
    tests.push_back(makeListTest<EntityType::USER_ROLE>());
    tests.push_back(makeListTest<EntityType::VAULT_ROLE>());

    auto res = router_->route(tests);
    stages_.push_back(TestStage{ "Read", res });
    validateStage(stages_.back());
}

void CLITestRunner::updateStage() {
    std::vector<std::shared_ptr<TestCase>> tests;

    auto& C = *ctx_;
    auto append = [&](auto&& vec) {
        tests.insert(tests.end(), vec.begin(), vec.end());
    };
    append(makeUpdateTests<EntityType::USER>(C.users));
    append(makeUpdateTests<EntityType::VAULT>(C.vaults));
    append(makeUpdateTests<EntityType::GROUP>(C.groups));
    append(makeUpdateTests<EntityType::USER_ROLE>(C.userRoles));
    append(makeUpdateTests<EntityType::VAULT_ROLE>(C.vaultRoles));

    auto res = router_->route(tests);
    stages_.push_back(TestStage{ "Update", res });
    validateStage(stages_.back());
}

void CLITestRunner::teardownStage() {
    std::vector<std::shared_ptr<TestCase>> tests;
    auto& C = *ctx_;

    // Order chosen to avoid fk/rbac headaches
    {
        auto append = [&](auto&& vec) {
            tests.insert(tests.end(), vec.begin(), vec.end());
        };
        append(makeDeleteTests<EntityType::USER_ROLE>(C.userRoles));
        append(makeDeleteTests<EntityType::GROUP>(C.groups));
        append(makeDeleteTests<EntityType::USER>(C.users));
        append(makeDeleteTests<EntityType::VAULT_ROLE>(C.vaultRoles));
        append(makeDeleteTests<EntityType::VAULT>(C.vaults));
    }

    auto res = router_->route(tests);
    stages_.push_back(TestStage{ "Teardown", res });
    validateStage(stages_.back());
}

// ---------- Validation / Results

void CLITestRunner::validateStage(const TestStage& stage) const {
    for (const auto& t : stage.tests) {
        if (!t) continue;

        std::vector<std::string> errors;

        // exit code check
        if (t->expect_exit != t->result.exit_code) {
            errors.push_back(stage.name + ": unexpected exit code for " + t->name);
        }

        // stdout expectations by path
        if (const auto it = expectations_by_path_.find(t->path); it != expectations_by_path_.end()) {
            const auto& exp = it->second;

            for (const auto& needle : exp.must_have) {
                if (!t->result.stdout_text.contains(needle)) {
                    errors.push_back(stage.name + ": missing expected text for " + t->name + ": " + needle);
                }
            }
            for (const auto& needle : exp.must_not_have) {
                if (t->result.stdout_text.contains(needle)) {
                    errors.push_back(stage.name + ": found forbidden text for " + t->name + ": " + needle);
                }
            }
        }

        // resolve final assertion (preserve earlier failures by appending)
        if (errors.empty()) {
            if (t->assertion.ok) t->assertion = AssertionResult::Pass();
            // else: keep prior fail (e.g., seed id extraction), do not override to pass
        } else {
            const bool had_prior = !t->assertion.ok && !t->assertion.message.empty();
            const auto combined = had_prior
                ? (t->assertion.message + "\n" + joinLines(errors))
                : joinLines(errors);
            t->assertion = AssertionResult::Fail(combined);
        }
    }
}

void CLITestRunner::validateAllTestObjects() const {
    Validator<EntityType::USER,       User>::assert_all_exist(ctx_->users);
    Validator<EntityType::VAULT,      Vault>::assert_all_exist(ctx_->vaults);
    Validator<EntityType::GROUP,      Group>::assert_all_exist(ctx_->groups);
    Validator<EntityType::USER_ROLE,  UserRole>::assert_all_exist(ctx_->userRoles);
    Validator<EntityType::VAULT_ROLE, VaultRole>::assert_all_exist(ctx_->vaultRoles);
}

int CLITestRunner::printResults() const {
    std::ostream& os = std::cout;

    // color / TTY
    const bool color_enabled = [=]() -> bool {
        if (std::getenv("NO_COLOR")) return false;      // https://no-color.org
        if (std::getenv("CLICOLOR_FORCE")) return true; // force on
        if (!isatty(fileno(stdout))) return false;
        const char* term = std::getenv("TERM");
        if (!term) return false;
        if (std::string(term) == "dumb") return false;
        return true;
    }();

    const char* reset = color_enabled ? "\033[0m"  : "";
    const char* bold  = color_enabled ? "\033[1m"  : "";
    const char* red   = color_enabled ? "\033[31m" : "";
    const char* green = color_enabled ? "\033[32m" : "";
    const char* yellow= color_enabled ? "\033[33m" : "";
    const char* cyan  = color_enabled ? "\033[36m" : "";
    const char* gray  = color_enabled ? "\033[90m" : "";
    const char* ok_glyph   = color_enabled ? "✔" : "OK";
    const char* fail_glyph = color_enabled ? "✘" : "X";

    constexpr int term_cols = 100;
    auto hr = [&](char c = '-') {
        for (int i = 0; i < term_cols; ++i) os << c;
        os << '\n';
    };

    int total = 0, passed = 0, failed = 0;

    os << bold << "CLI Test Results" << reset << "\n";
    hr();

    for (const auto& stage : stages_) {
        if (stage.name.empty()) continue;

        int stage_total = 0, stage_pass = 0, stage_fail = 0;

        os << bold << stage.name << reset << "\n";

        for (const auto& t : stage.tests) {
            if (!t) continue;
            ++stage_total; ++total;

            const bool ok = t->assertion.ok;
            if (ok) { ++stage_pass; ++passed; } else { ++stage_fail; ++failed; }

            const char* col = ok ? green : red;
            os << "  " << col << (ok ? ok_glyph : fail_glyph) << ' '
               << (ok ? "PASS" : "FAIL") << reset
               << "  " << bold << t->name << reset;

            if (t->expect_exit != t->result.exit_code) {
                os << ' ' << yellow << "[exit " << t->result.exit_code
                   << " ≠ expected " << t->expect_exit << ']' << reset;
            }
            os << '\n';

            if (!ok && !t->assertion.message.empty()) {
                std::istringstream iss(t->assertion.message);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) continue;
                    os << "      " << yellow << "• " << reset << line << '\n';
                }
            }
        }

        os << "  " << cyan << "Stage summary:" << reset
           << " " << stage_pass << "/" << stage_total << " passed";
        if (stage_fail) os << "  " << red << stage_fail << " failed" << reset;
        os << '\n';

        os << gray; hr(); os << reset;
    }

    os << bold << "Overall: "
       << (failed ? red : green) << (total - failed) << "/" << total << " passed"
       << reset << "\n";

    return failed; // non-zero on failures
}

}
