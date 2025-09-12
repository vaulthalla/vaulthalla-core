#include "CLITestRunner.hpp"

#include "UsageManager.hpp"
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

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string_view>

using namespace vh::shell;
using namespace vh::args;
using namespace vh::types;


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
    static constexpr std::string_view kIdPrefix = "Group ID:";
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
            std::cerr << "Warning: failed to extract ID from output for " << r->name << '\n' << r->result.stdout_text << '\n';
            r->assertion = AssertionResult::Fail("Seed: failed to extract ID from output for " + r->name);
        }
    }
}

// ---------- Runner

CLITestRunner::CLITestRunner(CLITestConfig&& cfg)
    : config_(cfg),
      ctx_(std::make_shared<CLITestContext>()),
      usage_(std::make_shared<shell::UsageManager>()),
      router_(std::make_shared<CommandRouter>(ctx_)) {
    CommandBuilderRegistry::init(usage_, ctx_);
    registerAllContainsAssertions();
}

// ----- pipeline

int CLITestRunner::operator()() {
    seed();
    readStage();
    updateStage();
    validateAllTestObjects();
    teardownStage();
    return printResults();
}

void CLITestRunner::seed() {
    seed<EntityType::USER_ROLE>(config_.numUserRoles);
    seed<EntityType::VAULT_ROLE>(config_.numVaultRoles);
    seed<EntityType::USER>(config_.numUsers);
    seed<EntityType::GROUP>(config_.numGroups);
    seed<EntityType::VAULT>(config_.numVaults);
}

template <EntityType E>
void CLITestRunner::seed(const size_t count) {
    const auto tests = makeCreateTests<E>(count);
    const auto res   = router_->route(tests);

    harvestIdsIntoContext<E>(*ctx_, res, EntityTraits<E>::kIdPrefix, std::cerr);

    stages_.push_back(TestStage(std::string("Seed ") + std::string(EntityTraits<E>::kStage), res));
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
        append(makeDeleteTests<EntityType::VAULT>(C.vaults));
        append(makeDeleteTests<EntityType::USER>(C.users));
        append(makeDeleteTests<EntityType::GROUP>(C.groups));
        append(makeDeleteTests<EntityType::USER_ROLE>(C.userRoles));
        append(makeDeleteTests<EntityType::VAULT_ROLE>(C.vaultRoles));
    }

    auto res = router_->route(tests);
    stages_.push_back(TestStage{ "Teardown", res });
    validateStage(stages_.back());
}

// ---------- Validation

void CLITestRunner::validateAllTestObjects() const {
    Validator<EntityType::USER,       User>::assert_all_exist(ctx_->users);
    Validator<EntityType::VAULT,      Vault>::assert_all_exist(ctx_->vaults);
    Validator<EntityType::GROUP,      Group>::assert_all_exist(ctx_->groups);
    Validator<EntityType::USER_ROLE,  UserRole>::assert_all_exist(ctx_->userRoles);
    Validator<EntityType::VAULT_ROLE, VaultRole>::assert_all_exist(ctx_->vaultRoles);
}

}
