#include "IntegrationsTestRunner.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "generators.hpp"
#include "types/VaultRole.hpp"
#include "types/UserRole.hpp"
#include "types/PermissionOverride.hpp"
#include "types/FSEntry.hpp"
#include "types/FSync.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "seed/include/seed_db.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/Filesystem.hpp"
#include "fuse_test_helpers.hpp"
#include "util/fsPath.hpp"

using namespace vh::test::cli;
using namespace vh::test::fuse;
using namespace vh::database;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::services;

static unsigned int uid_index = 1001;

void IntegrationsTestRunner::runFUSETests() {
    testFUSECRUD();
    // testFUSEAllow();
    // testFUSEDeny();
}

std::shared_ptr<User> IntegrationsTestRunner::createUser(const unsigned int vaultId, const uint16_t vaultPerms, const std::vector<std::shared_ptr<PermissionOverride>>& overrides) {
    const auto user = std::make_shared<User>();
    user->name = "testuser_" + generateRandomIndex(50000);

    const auto userRole = PermsQueries::getRoleByName("unprivileged");
    if (!userRole) throw std::runtime_error("Admin role not found");

    user->role = std::make_shared<UserRole>();
    user->role->name = userRole->name;
    user->role->permissions = userRole->permissions;
    user->role->id = userRole->id;
    user->role->description = userRole->description;

    user->linux_uid = uid_index++;
    linux_uids_.push_back(*user->linux_uid);

    const auto vaultRole = std::make_shared<VaultRole>();
    vaultRole->name = "vault_role_" + generateRandomIndex(50000);
    vaultRole->permissions = vaultPerms;
    vaultRole->description = "Vault role for user " + user->name;
    vaultRole->type = "vault";
    vaultRole->vault_id = vaultId;
    vaultRole->permissions = vaultPerms;
    vaultRole->permission_overrides = overrides;
    vaultRole->role_id = PermsQueries::addRole(std::static_pointer_cast<Role>(vaultRole));
    user->roles.push_back(vaultRole);
    user->id = UserQueries::createUser(user);
    return user;
}

static std::shared_ptr<StorageEngine> createVault() {
    auto vault = std::make_shared<Vault>();
    vault->name = "testvault_" + generateRandomIndex(50000);
    vault->description = "Test Vault";
    vault->owner_id = UserQueries::getUserByName("admin")->id;

    const auto sync = std::make_shared<FSync>();
    sync->interval = std::chrono::minutes(15);
    sync->conflict_policy = FSync::ConflictPolicy::Overwrite;

    vault = ServiceDepsRegistry::instance().storageManager->addVault(vault, sync);
    return ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
}

void IntegrationsTestRunner::testFUSECRUD() {
    const auto admin = UserQueries::getUserByName("admin");
    if (!admin) throw std::runtime_error("Admin user/uid not found");
    if (!admin->linux_uid) throw std::runtime_error("Admin user linux_uid not set");

    const auto vault = VaultQueries::getVault(std::string(seed::ADMIN_DEFAULT_VAULT_NAME), admin->id);
    if (!vault) throw std::runtime_error("Admin default vault not found");

    const auto engine = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    if (!engine) throw std::runtime_error("Engine not found for admin vault");

    const auto root = std::filesystem::path(engine->paths->fuseRoot / to_snake_case(vault->name));
    std::cout << "FUSE vault root for '" << vault->name << "':  " << root.string() << std::endl;

    std::vector<FuseStep> steps;

    steps.push_back({
        make_fuse_case("FUSE mkdir (admin)", "fuse/mkdir", /*expect=*/0, {"OK mkdir"}),
        [=]{ return mkdir_as(*admin->linux_uid, root / "crud_test"); }
    });

    steps.push_back({
        make_fuse_case("FUSE write (admin)", "fuse/write", 0, {"OK write"}),
        [=]{ return write_as(*admin->linux_uid, root / "crud_test" / "hello.txt", "hello world!\n"); }
    });

    steps.push_back({
        make_fuse_case("FUSE read (admin)", "fuse/read", 0 /* stdout will contain file data */),
        [=] { return read_as(*admin->linux_uid, root / "crud_test" / "hello.txt"); }
    });

    steps.push_back({
        make_fuse_case("FUSE rename (admin)", "fuse/rename", 0, {"OK mv"}),
        [=]{ return mv_as(*admin->linux_uid, root / "crud_test" / "hello.txt", root / "crud_test" / "hello2.txt"); }
    });

    steps.push_back({
        make_fuse_case("FUSE rm -rf (admin)", "fuse/rmrf", 0, {"OK rm -rf"}),
        [=]{ return rmrf_as(*admin->linux_uid, root / "crud_test"); }
    });

    const auto cases = run_fuse_steps(steps);
    stages_.push_back(TestStage{ "FUSE: CRUD (admin default vault)", cases });
    validateStage(stages_.back());
}

void IntegrationsTestRunner::testFUSEAllow() {
    const auto admin = UserQueries::getUserByName("admin");
    if (!admin || !admin->linux_uid) throw std::runtime_error("Admin user/uid not found");

    const auto engine = createVault();
    if (!engine) throw std::runtime_error("Failed to create vault");

    const auto root = std::filesystem::path(engine->paths->fuseRoot / to_snake_case(engine->vault->name));

    // Seed files as admin
    seed_vault_tree(*admin->linux_uid, root);

    // Create user with vault perms (power_user)
    const auto powerUserRole = PermsQueries::getRoleByName("power_user");
    if (!powerUserRole) throw std::runtime_error("Power user role not found");
    const auto user = createUser(engine->vault->id, powerUserRole->permissions, {});
    if (!user || !user->linux_uid) throw std::runtime_error("Failed to create user / uid");

    std::vector<FuseStep> steps;

    steps.push_back({
        make_fuse_case("FUSE allow: ls seed", "fuse/ls", 0 /* allow */),
        [=]{ return ls_as(*user->linux_uid, root / "perm_seed"); }
    });

    steps.push_back({
        make_fuse_case("FUSE allow: read secret", "fuse/read", 0 /* allow */),
        [=]{ return read_as(*user->linux_uid, root / "perm_seed" / "docs" / "secret.txt"); }
    });

    steps.push_back({
        make_fuse_case("FUSE allow: write user_note", "fuse/write", 0, {"OK write"}),
        [=]{ return write_as(*user->linux_uid, root / "perm_seed" / "docs" / "user_note.txt", "hey\n"); }
    });

    const auto cases = run_fuse_steps(steps);
    stages_.push_back(TestStage{ "FUSE: Permissions Allow", cases });
    validateStage(stages_.back());
}

void IntegrationsTestRunner::testFUSEDeny() {
    const auto admin = UserQueries::getUserByName("admin");
    if (!admin || !admin->linux_uid) throw std::runtime_error("Admin user/uid not found");

    const auto engine = createVault();
    if (!engine) throw std::runtime_error("Failed to create vault");

    const auto root = std::filesystem::path(engine->paths->fuseRoot / to_snake_case(engine->vault->name));

    // Seed files as admin
    seed_vault_tree(*admin->linux_uid, root);

    // Create user with NO perms
    const auto user = createUser(engine->vault->id, /*vaultPerms=*/0, /*overrides=*/{});
    if (!user || !user->linux_uid) throw std::runtime_error("Failed to create user / uid");

    std::vector<FuseStep> steps;

    steps.push_back({
        make_fuse_case("FUSE deny: read secret", "fuse/read", EACCES /* deny */),
        [=]{ return read_as(*user->linux_uid, root / "perm_seed" / "docs" / "secret.txt"); }
    });

    steps.push_back({
        make_fuse_case("FUSE deny: write hax", "fuse/write", EACCES /* deny */),
        [=]{ return write_as(*user->linux_uid, root / "perm_seed" / "docs" / "hax.txt", "nope\n"); }
    });

    steps.push_back({
        make_fuse_case("FUSE deny: rm -rf seed", "fuse/rmrf", EACCES /* deny */),
        [=]{ return rmrf_as(*user->linux_uid, root / "perm_seed"); }
    });

    const auto cases = run_fuse_steps(steps);
    stages_.push_back(TestStage{ "FUSE: Permissions Deny", cases });
    validateStage(stages_.back());
}
