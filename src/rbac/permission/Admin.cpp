#include "rbac/permission/Admin.hpp"
#include "rbac/permission/bitmask.hpp"

#include <nlohmann/json.hpp>

namespace vh::rbac::permission {

unsigned int toBit(const AdminPermission& perm) {
    if (perm == AdminPermission::ManageEncryptionKeys) return 0;
    if (perm == AdminPermission::ManageAdmins) return 1;
    if (perm == AdminPermission::ManageUsers) return 2;
    if (perm == AdminPermission::ManageGroups) return 3;
    if (perm == AdminPermission::ManageRoles) return 4;
    if (perm == AdminPermission::ManageSettings) return 5;
    if (perm == AdminPermission::ManageVaults) return 6;
    if (perm == AdminPermission::ManageAPIKeys) return 7;
    if (perm == AdminPermission::AuditLogAccess) return 8;
    if (perm == AdminPermission::CreateVaults) return 9;
    return 16; // Invalid
}

std::string to_string(const AdminPermission perm) {
    switch (perm) {
    case AdminPermission::ManageEncryptionKeys: return "Export Encryption Keys";
    case AdminPermission::ManageAdmins: return "Manage Admins";
    case AdminPermission::ManageUsers: return "Manage Users";
    case AdminPermission::ManageGroups: return "Manage Groups";
    case AdminPermission::ManageRoles: return "Manage Roles";
    case AdminPermission::ManageSettings: return "Manage Settings";
    case AdminPermission::ManageVaults: return "Manage Vaults";
    case AdminPermission::AuditLogAccess: return "Audit Log Access";
    case AdminPermission::ManageAPIKeys: return "Manage API Keys";
    case AdminPermission::CreateVaults: return "Create Vaults";
    default: return "Unknown Admin Permission";
    }
}

nlohmann::json jsonFromMask(const uint16_t mask) {
    return {
            {"export_encryption_keys", (mask & (1 << 0)) != 0},
            {"manage_admins", (mask & (1 << 1)) != 0},
            {"manage_users", (mask & (1 << 2)) != 0},
            {"manage_groups", (mask & (1 << 3)) != 0},
            {"manage_roles", (mask & (1 << 4)) != 0},
            {"manage_settings", (mask & (1 << 5)) != 0},
            {"manage_vaults", (mask & (1 << 6)) != 0},
            {"manage_api_keys", (mask & (1 << 7)) != 0},
            {"audit_log_access", (mask & (1 << 8)) != 0},
            {"create_vaults", (mask & (1 << 9)) != 0}
    };
}

uint16_t maskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("export_encryption_keys").get<bool>()) mask |= (1 << 0);
    if (j.at("manage_admins").get<bool>()) mask |= (1 << 1);
    if (j.at("manage_users").get<bool>()) mask |= (1 << 2);
    if (j.at("manage_groups").get<bool>()) mask |= (1 << 3);
    if (j.at("manage_roles").get<bool>()) mask |= (1 << 4);
    if (j.at("manage_settings").get<bool>()) mask |= (1 << 5);
    if (j.at("manage_vaults").get<bool>()) mask |= (1 << 6);
    if (j.at("manage_api_keys").get<bool>()) mask |= (1 << 7);
    if (j.at("audit_log_access").get<bool>()) mask |= (1 << 8);
    if (j.at("create_vaults").get<bool>()) mask |= (1 << 9);
    return mask;
}

std::string perms_to_string(const uint16_t mask, const unsigned short indent) {
    const auto perms = permsFromBitmask<AdminPermission>(mask);
    if (perms.empty()) return "No admin permissions";

    std::string out;
    out.reserve(64 + perms.size() * 16);

    const std::string prefix(indent, ' ');
    for (const auto& p : perms) out += prefix + to_string(p) + '\n';

    return out;
}

}
