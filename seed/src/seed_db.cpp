#include "seed_db.hpp"

// Database
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Transactions.hpp"

// Types
#include "types/Permission.hpp"
#include "types/S3Vault.hpp"
#include "types/RSync.hpp"
#include "types/FSync.hpp"
#include "types/User.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "types/Vault.hpp"
#include "types/Directory.hpp"

// Misc
#include "config/ConfigRegistry.hpp"
#include "keys/APIKeyManager.hpp"
#include "logging/LogRegistry.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "crypto/PasswordHash.hpp"
#include "util/bitmask.hpp"

// Libraries
#include <memory>

using namespace vh::seed;
using namespace vh::config;
using namespace vh::database;
using namespace vh::keys;
using namespace vh::types;
using namespace vh::logging;
using namespace vh::services;
using namespace vh::crypto;

void vh::seed::init() {
    LogRegistry::vaulthalla()->info("[initdb] Starting database initialization...");

    initPermissions();
    initRoles();
    initAdmin();
    initAdminGroup();
    initAdminDefaultVault();
    initRoot();
}

void vh::seed::initPermissions() {
    LogRegistry::vaulthalla()->info("[initdb] Initializing permissions...");

    const std::vector<Permission> userPerms{
        {0, "manage_admins", "Can manage admin users (create, deactivate, modify)"},
        {1, "manage_users", "Can manage regular users"},
        {2, "manage_roles", "Can create, modify, delete roles"},
        {3, "manage_settings", "Can modify system-wide settings"},
        {4, "manage_vaults", "Can create, delete, modify any vault settings"},
        {5, "access_audit_logs", "Can view system audit logs"},
        {6, "manage_api_keys", "Can manage API keys globally"}
    };

    const std::vector<Permission> vaultPerms{
        {0, "migrate_data", "Can migrate vault data to another storage backend"},
        {1, "manage_access", "Can manage vault roles and access rules"},
        {2, "manage_tags", "Can manage tags for files and directories"},
        {3, "manage_metadata", "Can manage file and directory metadata"},
        {4, "manage_versions", "Can manage file version history"},
        {5, "manage_file_locks", "Can lock or unlock files"},
        {6, "share", "Can create public sharing links"},
        {7, "sync", "Can sync vault data to external/cloud storage"},
        {8, "create", "Can create files or directories and upload files"},
        {9, "download", "Can download files or read file contents"},
        {10, "delete", "Can delete files or directories"},
        {11, "rename", "Can rename files or directories"},
        {12, "move", "Can move files or directories"},
        {13, "list", "Can list directory contents"}
    };

    Transactions::exec("initdb::initPermissions", [&](pqxx::work& txn) {
        for (const auto& p : userPerms)
            txn.exec_prepared("insert_raw_permission",
                pqxx::params{p.bit_position, p.name, p.description, "user"});

        for (const auto& p : vaultPerms)
            txn.exec_prepared("insert_raw_permission",
                pqxx::params{p.bit_position, p.name, p.description, "vault"});
    });
}

void vh::seed::initRoles() {
    LogRegistry::vaulthalla()->info("[initdb] Initializing roles...");

    std::vector<Role> roles{
        {"super_admin", "Root-level system owner with unrestricted access", "user", 0b0000000001111111},
        {"admin", "System administrator with all non-root administrative powers", "user", 0b0000000001111110},
        {"power_user", "Advanced user with full vault control but no admin authority", "vault", 0b0011111111111111},
        {"user", "Standard user with basic file operations", "vault", 0b0000000111101000},
        {"guest", "Minimal access: can download files and list directories", "vault", 0b0000000000101000}
    };

    Transactions::exec("initdb::initRoles", [&](pqxx::work& txn) {
        for (auto& r : roles) {
            r.id = txn.exec_prepared("insert_role",
                pqxx::params{r.name, r.description, r.type}).one_field().as<unsigned int>();

            txn.exec_prepared("assign_permission_to_role",
                pqxx::params{r.id, util::bitmask::bitmask_to_bitset(r.permissions).to_string()});
        }
    });
}

void vh::seed::initAdmin() {
    LogRegistry::vaulthalla()->info("[initdb] Initializing admin user...");

    const auto user = std::make_shared<User>();
    user->name = "admin";
    user->email = "";
    user->setPasswordHash(hashPassword("vh!adm1n"));
    user->linux_uid = ConfigRegistry::get().fuse.admin_linux_uid;

    const auto role = PermsQueries::getRoleByName("super_admin");
    user->role = std::make_shared<UserRole>();
    user->role->id = role->id;
    user->role->name = role->name;
    user->role->description = role->description;
    user->role->type = role->type;
    user->role->permissions = role->permissions;

    UserQueries::createUser(user);
}

void vh::seed::initAdminGroup() {
    LogRegistry::vaulthalla()->info("[initdb] Initializing admin group...");

    const auto gid = GroupQueries::createGroup("admin", "Core administrative group for system management");
    GroupQueries::addMemberToGroup(gid, "admin");
    const auto group = GroupQueries::getGroupByName("admin");
    if (!group) throw std::runtime_error("Failed to create admin group");
    if (group->members.front()->user->name != "admin") throw std::runtime_error("Admin user not added to admin group");
}

void vh::seed::initAdminDefaultVault() {
    LogRegistry::vaulthalla()->info("[initdb] Initializing admin default vault...");

    const auto vault = std::make_shared<Vault>();
    vault->name = "Admin Default Vault";
    vault->description = "Default vault for the admin user";
    vault->mount_point = "/users/admin";
    vault->type = VaultType::Local;
    vault->owner_id = 1;
    vault->quota = 0; // No quota for admin vault

    const auto sync = std::make_shared<FSync>();
    sync->interval = std::chrono::minutes(10);
    sync->conflict_policy = FSync::ConflictPolicy::Overwrite;

    VaultQueries::upsertVault(vault, sync);
}

void vh::seed::initRoot() {
    const auto dir = std::make_shared<Directory>();
    dir->name = "/";
    dir->created_by = dir->last_modified_by = 1;
    dir->path = "/";
    dir->fuse_path = "/";
    dir->inode = 1;
    dir->mode = 0755; // Standard directory permissions
    dir->is_hidden = false;
    dir->is_system = true;

    DirectoryQueries::upsertDirectory(dir);
}

void vh::seed::initDevCloudVault() {
    LogRegistry::vaulthalla()->info("[initdb] Initializing development Cloudflare R2 vault...");

    try {
        const std::string prefix = "VAULTHALLA_TEST_R2_";
        const auto accessKey = prefix + "ACCESS_KEY";
        const auto secretKey = prefix + "SECRET_ACCESS_KEY";
        const auto endpoint = prefix + "ENDPOINT";

        auto key = std::make_shared<api::APIKey>();
        key->user_id = 1; // Default user ID for dev mode
        key->name = "R2 Test Key";
        key->provider = api::S3Provider::CloudflareR2;
        key->region = "wnam";

        if (std::getenv(accessKey.c_str())) key->access_key = std::getenv(accessKey.c_str());
        else return;

        if (std::getenv(secretKey.c_str())) key->secret_access_key = std::getenv(secretKey.c_str());
        else return;

        if (std::getenv(endpoint.c_str())) key->endpoint = std::getenv(endpoint.c_str());
        else return;

        key->id = ServiceDepsRegistry::instance().apiKeyManager->addAPIKey(key);

        if (key->id == 0) {
            LogRegistry::storage()->error("[StorageManager] Failed to create API key for Cloudflare R2");
            return;
        }

        const auto vault = std::make_shared<S3Vault>();
        vault->name = "R2 Test Vault";
        vault->description = "Test vault for Cloudflare R2 in development mode";
        vault->mount_point = "/cloud/r2_test_vault";
        vault->api_key_id = key->id;
        vault->owner_id = 1;
        vault->bucket = "vaulthalla-test";
        vault->type = VaultType::S3;

        const auto sync = std::make_shared<RSync>();
        sync->interval = std::chrono::minutes(10);
        sync->conflict_policy = RSync::ConflictPolicy::KeepLocal;
        sync->strategy = RSync::Strategy::Sync;

        vault->id = VaultQueries::upsertVault(vault, sync);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageManager] Error initializing dev Cloudflare R2 vault: {}", e.what());
    }
}
