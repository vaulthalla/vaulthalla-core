#include "types/Permission.hpp"
#include "util/timestamp.hpp"
#include <nlohmann/json.hpp>

namespace vh::types {

Permission::Permission(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      display_name(row["display_name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      bit_position(row["bit_position"].as<uint16_t>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())) {}

std::string to_string(const AdminPermission p) {
    switch (p) {
    case AdminPermission::CreateUser: return "Create User";
    case AdminPermission::CreateAdminUser: return "Create Admin User";
    case AdminPermission::DeactivateUser: return "Deactivate User";
    case AdminPermission::ResetUserPassword: return "Reset User Password";
    case AdminPermission::ManageRoles: return "Manage Roles";
    case AdminPermission::ManageSettings: return "Manage Settings";
    case AdminPermission::ViewAuditLog: return "View Audit Log";
    case AdminPermission::ManageAPIKeys: return "Manage API Keys";
    case AdminPermission::CreateLocalVault: return "Create Local Vault";
    case AdminPermission::CreateCloudVault: return "Create Cloud Vault";
    case AdminPermission::DeleteVault: return "Delete Vault";
    case AdminPermission::MigrateVaultData: return "Migrate Vault Data";
    case AdminPermission::ManageVaultSettings: return "Manage Vault Settings";
    case AdminPermission::ManageVaultRoles: return "Manage Vault Roles";
    case AdminPermission::ManageAllVaults: return "Manage All Vaults";
    default: return "Unknown Admin Permission";
    }
}

std::string to_string(const FSPermission p) {
    switch (p) {
        case FSPermission::Upload: return "Upload";
        case FSPermission::Download: return "Download";
        case FSPermission::Delete: return "Delete";
        case FSPermission::SharePublic: return "Share Public";
        case FSPermission::ShareInternal: return "Share Internal";
        case FSPermission::Lock: return "Lock";
        case FSPermission::Rename: return "Rename";
        case FSPermission::Move: return "Move";
        case FSPermission::SyncLocal: return "Sync Local";
        case FSPermission::SyncCloud: return "Sync Cloud";
        case FSPermission::ModifyMetadata: return "Modify Metadata";
        case FSPermission::ChangeIcons: return "Change Icons";
        case FSPermission::ManageTags: return "Manage Tags";
        case FSPermission::ManageVersions: return "Manage Versions";
        case FSPermission::List: return "List Directory";
        default: return "Unknown File/Directory Permission";
    }
}

void to_json(nlohmann::json& j, const Permission& p) {
    j = {
        {"id", p.id},
        {"name", p.name},
        {"display_name", p.display_name},
        {"description", p.description},
        {"bit_position", p.bit_position},
        {"created_at", vh::util::timestampToString(p.created_at)},
        {"updated_at", vh::util::timestampToString(p.updated_at)}
    };
}

void from_json(const nlohmann::json& j, Permission& p) {
    p.id = j.at("id").get<unsigned int>();
    p.name = j.at("name").get<std::string>();
    p.display_name = j.at("display_name").get<std::string>();
    p.description = j.at("description").get<std::string>();
    p.bit_position = j.at("bit_position").get<uint16_t>();
}

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Permission>>& permissions) {
    j = nlohmann::json::array();
    for (const auto& perm : permissions) j.push_back(*perm);
}

nlohmann::json jsonFromAdminMask(const uint16_t mask) {
    return {
        { "create_user",            (mask & (1 << 0)) != 0 },
        { "create_admin_user",      (mask & (1 << 1)) != 0 },
        { "deactivate_user",        (mask & (1 << 2)) != 0 },
        { "reset_user_password",    (mask & (1 << 3)) != 0 },
        { "manage_roles",           (mask & (1 << 4)) != 0 },
        { "manage_settings",        (mask & (1 << 5)) != 0 },
        { "view_audit_log",         (mask & (1 << 6)) != 0 },
        { "manage_api_keys",        (mask & (1 << 7)) != 0 }
    };
}

nlohmann::json jsonFromFSMask(const uint16_t mask) {
    return {
        { "create_local_vault",     (mask & (1 << 0)) != 0 },
        { "create_cloud_vault",     (mask & (1 << 1)) != 0 },
        { "delete_vault",           (mask & (1 << 2)) != 0 },
        { "adjust_vault_settings",  (mask & (1 << 3)) != 0 },
        { "migrate_vault_data",     (mask & (1 << 4)) != 0 },
        { "create_volume",          (mask & (1 << 5)) != 0 },
        { "delete_volume",          (mask & (1 << 6)) != 0 },
        { "resize_volume",          (mask & (1 << 7)) != 0 },
        { "move_volume",            (mask & (1 << 8)) != 0 },
        { "assign_volume_to_group", (mask & (1 << 9)) != 0 }
    };
}

uint16_t adminMaskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("create_user").get<bool>()) mask |= (1 << 0);
    if (j.at("create_admin_user").get<bool>()) mask |= (1 << 1);
    if (j.at("deactivate_user").get<bool>()) mask |= (1 << 2);
    if (j.at("reset_user_password").get<bool>()) mask |= (1 << 3);
    if (j.at("manage_roles").get<bool>()) mask |= (1 << 4);
    if (j.at("manage_settings").get<bool>()) mask |= (1 << 5);
    if (j.at("view_audit_log").get<bool>()) mask |= (1 << 6);
    if (j.at("manage_api_keys").get<bool>()) mask |= (1 << 7);
    return mask;
}

uint16_t fsMaskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("create_local_vault").get<bool>()) mask |= (1 << 0);
    if (j.at("create_cloud_vault").get<bool>()) mask |= (1 << 1);
    if (j.at("delete_vault").get<bool>()) mask |= (1 << 2);
    if (j.at("adjust_vault_settings").get<bool>()) mask |= (1 << 3);
    if (j.at("migrate_vault_data").get<bool>()) mask |= (1 << 4);
    if (j.at("create_volume").get<bool>()) mask |= (1 << 5);
    if (j.at("delete_volume").get<bool>()) mask |= (1 << 6);
    if (j.at("resize_volume").get<bool>()) mask |= (1 << 7);
    if (j.at("move_volume").get<bool>()) mask |= (1 << 8);
    if (j.at("assign_volume_to_group").get<bool>()) mask |= (1 << 9);
    return mask;
}

}
