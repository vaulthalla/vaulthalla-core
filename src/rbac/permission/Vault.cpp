#include "rbac/permission/Vault.hpp"
#include "rbac/permission/bitmask.hpp"

#include <nlohmann/json.hpp>

namespace vh::rbac::permission {

unsigned int toBit(const VaultPermission& perm) {
    if (perm == VaultPermission::ManageVault) return 0;
    if (perm == VaultPermission::ManageAccess) return 1;
    if (perm == VaultPermission::ManageTags) return 2;
    if (perm == VaultPermission::ManageMetadata) return 3;
    if (perm == VaultPermission::ManageVersions) return 4;
    if (perm == VaultPermission::ManageFileLocks) return 5;
    if (perm == VaultPermission::Share) return 6;
    if (perm == VaultPermission::Sync) return 7;
    if (perm == VaultPermission::Create) return 8;
    if (perm == VaultPermission::Download) return 9;
    if (perm == VaultPermission::Delete) return 10;
    if (perm == VaultPermission::Rename) return 11;
    if (perm == VaultPermission::Move) return 12;
    if (perm == VaultPermission::List) return 13;
    return 16; // Invalid
}

std::string to_string(const VaultPermission perm) {
    switch (perm) {
    case VaultPermission::ManageVault: return "Manage Vault";
    case VaultPermission::ManageAccess: return "Manage Access";
    case VaultPermission::ManageTags: return "Manage Tags";
    case VaultPermission::ManageMetadata: return "Manage Metadata";
    case VaultPermission::ManageVersions: return "Manage Versions";
    case VaultPermission::ManageFileLocks: return "Manage File Locks";
    case VaultPermission::Share: return "Share Files/Directories";
    case VaultPermission::Sync: return "Sync with Cloud Storage";
    case VaultPermission::Create: return "Create & Upload Files/Directories";
    case VaultPermission::Download: return "Download Files/Directories";
    case VaultPermission::Delete: return "Delete Files/Directories";
    case VaultPermission::Rename: return "Rename Files/Directories";
    case VaultPermission::Move: return "Move Files/Directories";
    case VaultPermission::List: return "List Directories";
    default: return "Unknown File/Directory Permission";
    }
}

std::string name(const VaultPermission& p) {
    switch (p) {
    case VaultPermission::ManageVault: return "manage_vault";
    case VaultPermission::ManageAccess: return "manage_access";
    case VaultPermission::ManageTags: return "manage_tags";
    case VaultPermission::ManageMetadata: return "manage_metadata";
    case VaultPermission::ManageVersions: return "manage_versions";
    case VaultPermission::ManageFileLocks: return "manage_file_locks";
    case VaultPermission::Share: return "share";
    case VaultPermission::Sync: return "sync";
    case VaultPermission::Create: return "create";
    case VaultPermission::Download: return "download";
    case VaultPermission::Delete: return "delete";
    case VaultPermission::Rename: return "rename";
    case VaultPermission::Move: return "move";
    case VaultPermission::List: return "list";
    default: return "unknown-vault-permission";
    }
}

nlohmann::json jsonFromMask(const uint16_t mask) {
    return {
        {"manage_vault", (mask & (1 << 0)) != 0},
        {"manage_access", (mask & (1 << 1)) != 0},
        {"manage_tags", (mask & (1 << 2)) != 0},
        {"manage_metadata", (mask & (1 << 3)) != 0},
        {"manage_versions", (mask & (1 << 4)) != 0},
        {"manage_file_locks", (mask & (1 << 5)) != 0},
        {"share", (mask & (1 << 6)) != 0},
        {"sync", (mask & (1 << 7)) != 0},
        {"create", (mask & (1 << 8)) != 0},
        {"download", (mask & (1 << 9)) != 0},
        {"delete", (mask & (1 << 10)) != 0},
        {"rename", (mask & (1 << 11)) != 0},
        {"move", (mask & (1 << 12)) != 0},
        {"list", (mask & (1 << 13)) != 0}
    };
}



uint16_t maskFromJson(const nlohmann::json& j) {
    uint16_t mask = 0;
    if (j.at("manage_vault").get<bool>()) mask |= (1 << 0);
    if (j.at("manage_access").get<bool>()) mask |= (1 << 1);
    if (j.at("manage_tags").get<bool>()) mask |= (1 << 2);
    if (j.at("manage_metadata").get<bool>()) mask |= (1 << 3);
    if (j.at("manage_versions").get<bool>()) mask |= (1 << 4);
    if (j.at("manage_file_locks").get<bool>()) mask |= (1 << 5);
    if (j.at("share").get<bool>()) mask |= (1 << 6);
    if (j.at("sync").get<bool>()) mask |= (1 << 7);
    if (j.at("create").get<bool>()) mask |= (1 << 8);
    if (j.at("download").get<bool>()) mask |= (1 << 9);
    if (j.at("delete").get<bool>()) mask |= (1 << 10);
    if (j.at("rename").get<bool>()) mask |= (1 << 11);
    if (j.at("move").get<bool>()) mask |= (1 << 12);
    if (j.at("list").get<bool>()) mask |= (1 << 13);
    return mask;
}

std::string perms_to_string(const uint16_t mask, const unsigned short indent) {
    const auto perms = permsFromBitmask<VaultPermission>(mask);
    if (perms.empty()) return "No vault permissions";

    std::string out;
    out.reserve(64 + perms.size() * 16);

    const std::string prefix(indent, ' ');
    for (const auto& p : perms) out += prefix + to_string(p) + '\n';

    return out;
}

}
