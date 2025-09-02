#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "database/Queries/GroupQueries.hpp"

#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/VaultRole.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"
#include "types/Permission.hpp"

#include "services/LogRegistry.hpp"
#include "config/ConfigRegistry.hpp"

#include <optional>
#include <string>
#include <vector>
#include <memory>

using namespace vh::shell::commands;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;
using namespace vh::crypto;
using namespace vh::logging;

static VaultPermission permFromString(const std::string& perm) {
    if (perm.empty()) throw std::invalid_argument("Vault permission string cannot be empty");

    if (perm == "manage-vault")      return VaultPermission::ManageVault;
    if (perm == "manage-access")     return VaultPermission::ManageAccess;
    if (perm == "manage-tags")       return VaultPermission::ManageTags;
    if (perm == "manage-metadata")   return VaultPermission::ManageMetadata;
    if (perm == "manage-versions")   return VaultPermission::ManageVersions;
    if (perm == "manage-file-locks") return VaultPermission::ManageFileLocks;
    if (perm == "share")             return VaultPermission::Share;
    if (perm == "sync")              return VaultPermission::Sync;
    if (perm == "create")            return VaultPermission::Create;
    if (perm == "download")          return VaultPermission::Download;
    if (perm == "delete")            return VaultPermission::Delete;
    if (perm == "rename")            return VaultPermission::Rename;
    if (perm == "move")              return VaultPermission::Move;
    if (perm == "list")              return VaultPermission::List;
    throw std::invalid_argument("Unknown vault permission string: " + perm);
}

struct ParsedPermsResult { std::vector<VaultPermission> allow, deny; };

static ParsedPermsResult parseExplicitVaultPermissionFlags(const CommandCall& call) {
    ParsedPermsResult res;

    const auto parseFlag = [&call, &res](const std::string& flag) {
        if (hasFlag(call, flag) || hasFlag(call, "allow-" + flag)) res.allow.push_back(permFromString(flag));
        if (hasFlag(call, "deny-" + flag)) res.deny.push_back(permFromString(flag));
    };

    parseFlag("manage-vault");
    parseFlag("manage-access");
    parseFlag("manage-tags");
    parseFlag("manage-metadata");
    parseFlag("manage-versions");
    parseFlag("manage-file-locks");
    parseFlag("share");
    parseFlag("sync");
    parseFlag("create");
    parseFlag("download");
    parseFlag("delete");
    parseFlag("rename");
    parseFlag("move");
    parseFlag("list");

    return res;
}

static CommandResult handle_vault_role_assign(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault assign: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault assign: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);
    const auto roleIdOpt = parseInt(roleArg);

    std::shared_ptr<Vault> vault;

    if (vaultIdOpt) {
        if (*vaultIdOpt <= 0) return invalid("vault assign: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        unsigned int ownerId;
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault assign: vault does not have a owner");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault assign: --owner must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault assign: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault assign: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault assign: vault with arg " + vaultArg + " not found");

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault assign: you do not have permission to assign roles to this vault");

        if (!call.user->canManageRoles()) return invalid("vault assign: you do not have permission to manage roles");
    }

    std::shared_ptr<Role> role;

    if (roleIdOpt) {
        if (*roleIdOpt <= 0) return invalid("vault assign: role ID must be a positive integer");
        role = PermsQueries::getRole(*roleIdOpt);
    } else role = PermsQueries::getRoleByName(roleArg);

    if (!role) return invalid("vault assign: role with arg " + roleArg + " not found");

    std::string subjectType;
    unsigned int subjectId;

    if (const auto userOpt = optVal(call, "uid")) {
        subjectType = "user";
        if (const auto uidOpt = parseInt(*userOpt)) {
            if (*uidOpt <= 0) return invalid("vault assign: user ID must be a positive integer");
            subjectId = *uidOpt;
        } else {
            const auto user = UserQueries::getUserByName(*userOpt);
            if (!user) return invalid("vault assign: user not found: " + *userOpt);
            subjectId = user->id;
        }
    } else if (const auto groupOpt = optVal(call, "gid")) {
        subjectType = "group";
        if (const auto gidOpt = parseInt(*groupOpt)) {
            if (*gidOpt <= 0) return invalid("vault assign: group ID must be a positive integer");
            subjectId = *gidOpt;
        } else {
            const auto group = GroupQueries::getGroupByName(*groupOpt);
            if (!group) return invalid("vault assign: group not found: " + *groupOpt);
            subjectId = group->id;
        }
    } else return invalid("vault assign: must specify either --uid or --gid");

    const auto vr = std::make_shared<VaultRole>();
    vr->role_id = role->id;
    vr->vault_id = vault->id;
    vr->subject_type = subjectType;
    vr->subject_id = subjectId;

    PermsQueries::assignVaultRole(vr);

    return ok("Successfully assigned role '" + role->name + "' to vault '" + vault->name + "'");
}

static CommandResult handle_vault_role_remove(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault remove: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault remove: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);

    std::shared_ptr<Vault> vault;

    if (vaultIdOpt) {
        if (*vaultIdOpt <= 0) return invalid("vault remove: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        unsigned int ownerId;
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault remove: vault does not have a owner");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault remove: --owner must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault remove: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault remove: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault remove: vault with arg " + vaultArg + " not found");

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault remove: you do not have permission to remove roles from this vault");

        if (!call.user->canManageRoles()) return invalid("vault remove: you do not have permission to manage roles");
    }

    std::shared_ptr<Role> role;


    std::string subjectType;
    unsigned int subjectId;

    // TODO: Support removing by name
}

static CommandResult handle_vault_role_list(const CommandCall& call) {
    if (call.positionals.size() > 1) return invalid("vault list: too many arguments");

    std::shared_ptr<Vault> vault;
    if (!call.positionals.empty()) {
        const auto vaultArg = call.positionals.at(0);
        const auto vaultIdOpt = parseInt(vaultArg);

        if (vaultIdOpt) {
            if (*vaultIdOpt <= 0) return invalid("vault list: vault ID must be a positive integer");
            vault = VaultQueries::getVault(*vaultIdOpt);
        } else {
            unsigned int ownerId;
            const auto ownerOpt = optVal(call, "owner");
            if (!ownerOpt) return invalid("vault list: vault does not have a owner");
            if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
                if (*ownerIdOpt <= 0) return invalid("vault list: --owner must be a positive integer");
                ownerId = *ownerIdOpt;
            } else if (ownerOpt) {
                if (ownerOpt->empty()) return invalid("vault list: --owner requires a value");
                const auto owner = UserQueries::getUserByName(*ownerOpt);
                if (!owner) return invalid("vault list: owner not found: " + *ownerOpt);
                ownerId = owner->id;
            } else ownerId = call.user->id;
            vault = VaultQueries::getVault(vaultArg, ownerId);
        }

        if (!vault) return invalid("vault list: vault with arg " + vaultArg + " not found");

        if (vault->owner_id != call.user->id) {
            if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
                "vault list: you do not have permission to view roles for this vault");
        }
    }

    std::vector<std::shared_ptr<VaultRole>> roles;
    if (vault) roles = PermsQueries::listVaultAssignedRoles(vault->id);
    else {
        if (!call.user->canManageRoles()) return invalid("vault list: you do not have permission to manage roles");
        roles = PermsQueries::listVaultAssignedRoles(0); // List all roles
    }

    nlohmann::json j;
    j["roles"] = roles;
    return ok(j.dump(4));
}

static CommandResult handle_vault_role_override_add(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault override: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault override: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);

    std::shared_ptr<Vault> vault;

    if (vaultIdOpt) {
        if (*vaultIdOpt <= 0) return invalid("vault override: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        unsigned int ownerId;
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault override: vault does not have a owner");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault override: --owner must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault override: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault override: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault override: vault with arg " + vaultArg + " not found");

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault override: you do not have permission to override roles for this vault");

        if (!call.user->canManageRoles()) return invalid("vault override: you do not have permission to manage roles");
    }

    std::string subjectType;
    unsigned int subjectId = 0;

    std::vector<std::string> userFlags = {"user", "u"};
    std::vector<std::string> groupFlags = {"group", "g"};

    for (const auto& flag : userFlags) {
        if (const auto userOpt = optVal(call, flag)) {
            subjectType = "user";
            if (const auto uidOpt = parseInt(*userOpt)) {
                if (*uidOpt <= 0) return invalid("vault override: user ID must be a positive integer");
                subjectId = *uidOpt;
            } else {
                const auto user = UserQueries::getUserByName(*userOpt);
                if (!user) return invalid("vault override: user not found: " + *userOpt);
                subjectId = user->id;
            }
            break;
        }
    }

    if (subjectType.empty() && subjectId == 0) {
        for (const auto& flag : groupFlags) {
            if (const auto groupOpt = optVal(call, flag)) {
                subjectType = "group";
                if (const auto gidOpt = parseInt(*groupOpt)) {
                    if (*gidOpt <= 0) return invalid("vault override: group ID must be a positive integer");
                    subjectId = *gidOpt;
                } else {
                    const auto group = GroupQueries::getGroupByName(*groupOpt);
                    if (!group) return invalid("vault override: group not found: " + *groupOpt);
                    subjectId = group->id;
                }
                break;
            }
        }
    }

    if (subjectType.empty() && subjectId == 0) return invalid("vault override: must specify either --user/-u or --group/-g");

    std::shared_ptr<VaultRole> role;

    if (const auto roleIdOpt = parseInt(roleArg)) role = PermsQueries::getVaultRole(*roleIdOpt);
    else role = PermsQueries::getVaultRoleBySubjectAndVaultId(subjectId, subjectType, vault->id);

    if (!role) return invalid("vault override: role with arg " + roleArg + " not found");

    const auto parsedPerms = parseExplicitVaultPermissionFlags(call);
    if (parsedPerms.allow.empty() && parsedPerms.deny.empty()) return invalid(
        "vault override: must specify at least one permission to allow or deny using --<perm> or --unset-<perm>");

    std::string pattern;
    if (const auto patternOpt = optVal(call, "path")) {
        if (patternOpt->empty()) return invalid("vault override: --path requires a value");
        pattern = *patternOpt;
    } else return invalid("vault override: --path is required");

    bool enable = true;
    if (hasFlag(call, "disable")) enable = false;

    const auto regexPattern = std::regex(pattern); // Intended to throw if invalid

    const auto handleOverride = [&](const VaultPermission& perm, const OverrideOpt effect) {
        const auto override = std::make_shared<PermissionOverride>();
        override->assignment_id = role->id;
        override->permission = *PermsQueries::getPermissionByName(get_vault_perm_name(perm));
        override->pattern = regexPattern;
        override->patternStr = pattern;
        override->effect = effect;
        override->enabled = enable;
        PermsQueries::addVPermOverride(override);
    };

    for (const auto& perm : parsedPerms.allow) handleOverride(perm, OverrideOpt::ALLOW);
    for (const auto& perm : parsedPerms.deny) handleOverride(perm, OverrideOpt::DENY);

    return ok("Successfully added permission overrides to role '" + role->name + "' on vault '" + vault->name + "'");
}

static CommandResult handle_vault_role_override_remove(const CommandCall& call) {}

static CommandResult handle_vault_role_override_update(const CommandCall& call) {}

static CommandResult handle_vault_role_override_list(const CommandCall& call) {}

static bool isVaultRoleMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"vault", "role", cmd}, input);
}

static bool isVaultRoleOverrideMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"vault", "role", "override", cmd}, input);
}

static CommandResult handle_vault_role_override(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault override: missing <add|remove|list>");
    if (call.positionals.size() < 2 && call.positionals[0] != "list") return invalid("vault override: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 3) return invalid("vault override: too many arguments");

    const auto action = call.positionals[0];
    auto subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (isVaultRoleOverrideMatch({"add"}, action)) return handle_vault_role_override_add(subcall);
    if (isVaultRoleOverrideMatch({"remove"}, action)) return handle_vault_role_override_remove(subcall);
    if (isVaultRoleOverrideMatch({"update"}, action)) return handle_vault_role_override_update(subcall);
    if (isVaultRoleOverrideMatch({"list"}, action)) return handle_vault_role_override_list(subcall);

    return invalid(call.constructFullArgs(), "Unknown vault override action: '" + std::string(action) + "'");
}

CommandResult commands::vault::handle_vault_role(const CommandCall& call) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;

    if (call.positionals.empty()) return usage(call.constructFullArgs());
    if (call.positionals.size() > 1) return invalid(call.constructFullArgs(), "vault role: too many arguments");

    const auto subcommand = call.positionals[0];
    auto subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (isVaultRoleMatch({"assign"}, subcommand)) return handle_vault_role_assign(subcall);
    if (isVaultRoleMatch({"remove"}, subcommand)) return handle_vault_role_remove(subcall);
    if (isVaultRoleMatch({"list"}, subcommand)) return handle_vault_role_list(subcall);
    if (isVaultRoleMatch({"override"}, subcommand)) return handle_vault_role_override(subcall);

    return invalid(call.constructFullArgs(), "Unknown vault role subcommand: '" + std::string(subcommand) + "'");
}
