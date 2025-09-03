#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "database/Queries/PermsQueries.hpp"

#include "types/Vault.hpp"
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
#include <regex>
#include <utility>

using namespace vh::shell::commands;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;
using namespace vh::crypto;
using namespace vh::logging;

namespace vh::shell::commands::vault {

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

static CommandResult handle_vault_role_override_add(const CommandCall& call) {
    constexpr const char* ERR = "vault role override add";

    if (call.positionals.size() < 2) return invalid(std::string(ERR) + ": missing <vault_id|vault_name> and <role_id|role_hint>");
    if (call.positionals.size() > 2) return invalid(std::string(ERR) + ": too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg  = call.positionals.at(1);

    auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    auto vault = vLkp.ptr;

    if (auto err = checkOverridePermissions(call, vault, ERR)) return invalid(*err);

    const auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const Subject subj = *subjLkp.ptr;

    const auto roleLkp = resolveVRole(roleArg, vault, &subj, ERR);
    if (!roleLkp) return invalid(roleLkp.error);
    const auto role = roleLkp.ptr;

    const auto parsedPerms = parseExplicitVaultPermissionFlags(call);
    if (parsedPerms.allow.empty() && parsedPerms.deny.empty())
        return invalid(std::string(ERR) + ": must specify at least one permission using --<perm> (allow) or --unset-<perm> (deny)");

    const auto patt = parsePatternOpt(call, /*required=*/true, ERR);
    if (!patt.ok) return invalid(patt.error);

    const auto en = parseEnableDisableOpt(call, ERR);
    if (!en.ok) return invalid(en.error);
    const bool enabled = en.value.value_or(true);

    const auto applyOne = [&](const VaultPermission& perm, OverrideOpt effect) {
        const auto ov = std::make_shared<PermissionOverride>();
        ov->assignment_id = role->id;
        ov->permission = *PermsQueries::getPermissionByName(get_vault_perm_name(perm));
        ov->pattern     = *patt.compiled;
        ov->patternStr  = patt.raw;
        ov->effect      = effect;
        ov->enabled     = enabled;
        PermsQueries::addVPermOverride(ov);
    };

    for (const auto& p : parsedPerms.allow) applyOne(p, OverrideOpt::ALLOW);
    for (const auto& p : parsedPerms.deny)  applyOne(p, OverrideOpt::DENY);

    return ok("Successfully added permission override(s) to role '" + role->name + "' on vault '" + vault->name + "'");
}

static CommandResult handle_vault_role_override_update(const CommandCall& call) {
    constexpr const char* ERR = "vault role override update";

    if (call.positionals.size() < 3)
        return invalid(std::string(ERR) + ": missing <vault-id|vault-name> <role_id|role_hint> <bit_position>");
    if (call.positionals.size() > 3)
        return invalid(std::string(ERR) + ": too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg  = call.positionals.at(1);
    const auto bitArg   = call.positionals.at(2);

    auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    auto vault = vLkp.ptr;

    if (auto err = checkOverridePermissions(call, vault, ERR)) return invalid(*err);

    auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const Subject subj = *subjLkp.ptr;

    auto roleLkp = resolveVRole(roleArg, vault, &subj, ERR);
    if (!roleLkp) return invalid(roleLkp.error);
    auto role = roleLkp.ptr;

    std::string bitErr;
    auto bitPosOpt = parsePositiveUint(bitArg, "bit position", bitErr);
    if (!bitPosOpt) return invalid(std::string(ERR) + ": " + bitErr);
    const unsigned int bitPosition = *bitPosOpt;

    VPermOverrideQuery q{
        .vault_id     = vault->id,
        .subject_type = subj.type,
        .subject_id   = subj.id,
        .bit_position = bitPosition
    };

    auto ov = PermsQueries::getVPermOverride(q);
    if (!ov) {
        return invalid(std::string(ERR) + ": no override found for (vault="
                       + std::to_string(vault->id) + ", " + subj.type + "="
                       + std::to_string(subj.id) + ", bit=" + std::to_string(bitPosition) + ")");
    }

    // Defensive: ensure the override fetched indeed belongs to the resolved role assignment
    if (ov->assignment_id != role->id) {
        return invalid(std::string(ERR) + ": override does not belong to role '"
                       + role->name + "' (assignment mismatch)");
    }

    bool changed = false;

    const auto eff = parseEffectChangeOpt(call, ERR);
    if (!eff.ok) return invalid(eff.error);
    if (eff.value) {
        ov->effect = *eff.value;
        changed = true;
    }

    const auto patt = parsePatternOpt(call, /*required=*/false, ERR);
    if (!patt.ok) return invalid(patt.error);
    if (patt.compiled) {
        ov->pattern    = *patt.compiled;
        ov->patternStr = patt.raw;
        changed = true;
    }

    const auto en = parseEnableDisableOpt(call, ERR);
    if (!en.ok) return invalid(en.error);
    if (en.value) {
        ov->enabled = *en.value;
        changed = true;
    }

    if (!changed)
        return invalid(std::string(ERR) + ": no changes specified (set at least one of --allow/--deny, --path/--pattern, --enable/--disable)");

    PermsQueries::updateVPermOverride(ov);

    return ok("Updated override (vault=" + std::to_string(vault->id) +
              ", " + subj.type + "=" + std::to_string(subj.id) +
              ", bit=" + std::to_string(bitPosition) + ") on role '" + role->name + "'");
}

static CommandResult handle_vault_role_override_remove(const CommandCall& call) {
    constexpr const char* ERR = "vault override remove";

    if (call.positionals.size() < 3)
        return invalid(std::string(ERR) + ": missing <vault-id|vault-name> <role_id|role_hint> <bit_position>");
    if (call.positionals.size() > 3)
        return invalid(std::string(ERR) + ": too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg  = call.positionals.at(1);
    const auto bitArg   = call.positionals.at(2);

    auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    auto vault = vLkp.ptr;

    if (auto err = checkOverridePermissions(call, vault, ERR)) return invalid(*err);

    auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const Subject subj = *subjLkp.ptr;

    auto roleLkp = resolveVRole(roleArg, vault, &subj, ERR);
    if (!roleLkp) return invalid(roleLkp.error);
    auto role = roleLkp.ptr;

    std::string bitErr;
    auto bitPosOpt = parsePositiveUint(bitArg, "bit position", bitErr);
    if (!bitPosOpt) return invalid(std::string(ERR) + ": " + bitErr);
    const unsigned int bitPosition = *bitPosOpt;

    VPermOverrideQuery q{
        .vault_id     = vault->id,
        .subject_type = subj.type,
        .subject_id   = subj.id,
        .bit_position = bitPosition
    };

    auto ov = PermsQueries::getVPermOverride(q);
    if (!ov) {
        return invalid(std::string(ERR) + ": no override found for (vault="
                       + std::to_string(vault->id) + ", " + subj.type + "="
                       + std::to_string(subj.id) + ", bit=" + std::to_string(bitPosition) + ")");
    }
    if (ov->assignment_id != role->id) {
        return invalid(std::string(ERR) + ": override does not belong to role '"
                       + role->name + "' (assignment mismatch)");
    }

    PermsQueries::removeVPermOverride(ov->id);

    return ok("Removed override (vault=" + std::to_string(vault->id) +
              ", " + subj.type + "=" + std::to_string(subj.id) +
              ", bit=" + std::to_string(bitPosition) + ") from role '" + role->name + "'");
}

static CommandResult handle_vault_role_override_list(const CommandCall& call) {
    constexpr const auto* ERR = "vault override list";

    if (call.positionals.size() < 2)
        return invalid(std::string(ERR) + ": missing <vault-id|vault-name> <role_id|role_hint>");
    if (call.positionals.size() > 2)
        return invalid(std::string(ERR) + ": too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg  = call.positionals.at(1);

    auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    auto vault = vLkp.ptr;

    if (auto err = checkOverridePermissions(call, vault, ERR)) return invalid(*err);

    auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const Subject subj = *subjLkp.ptr;

    auto roleLkp = resolveVRole(roleArg, vault, &subj, ERR);
    if (!roleLkp) return invalid(roleLkp.error);
    auto role = roleLkp.ptr;

    const auto& ovs = role->permission_overrides;

    if (ovs.empty()) {
        return ok("No overrides found for role '" + role->name + "' in vault '" + vault->name +
                  "' for " + subj.type + " id " + std::to_string(subj.id));
    }

    std::vector<std::shared_ptr<PermissionOverride>> overrides = ovs;
    std::ranges::sort(overrides.begin(), overrides.end(),
              [](const auto& a, const auto& b) {
                  return a->permission.bit_position < b->permission.bit_position;
              });

    // TODO: // implement a proper to_string using shell::Table
    std::string out;
    out += "Overrides for role '" + role->name + "' in vault '" + vault->name +
           "' for " + subj.type + " id " + std::to_string(subj.id) + ":\n";
    out += "  bit  | permission       | effect | enabled | pattern\n";
    out += "  -----+-------------------+--------+---------+----------------------------\n";

    for (const auto& ov : overrides) {
        const auto bit = ov->permission.bit_position;
        const auto& permName = ov->permission.name;
        const char* eff = (ov->effect == OverrideOpt::ALLOW ? "ALLOW" : "DENY");
        const char* en  = (ov->enabled ? "true" : "false");

        out += "  " + std::to_string(bit);
        out += (bit < 10 ? "   | " : (bit < 100 ? "  | " : " | ")); // basic align
        out += permName;
        if (permName.size() < 17) out.append(17 - permName.size(), ' ');
        out += " | ";
        out += eff; out += "   | ";
        out += en;  out += "    | ";
        out += ov->patternStr;
        out += "\n";
    }

    return ok(out);
}

static CommandResult handle_vault_role_assign(const CommandCall& call) {
    constexpr const auto* ERR = "vault role override update";

    if (call.positionals.size() < 2) return invalid("vault assign: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault assign: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg = call.positionals.at(1);

    const auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    const auto vault = vLkp.ptr;

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault assign: you do not have permission to assign roles to this vault");

        if (!call.user->canManageRoles()) return invalid("vault assign: you do not have permission to manage roles");
    }

    const auto roleLkp = resolveRole(roleArg, ERR);
    if (!roleLkp || !roleLkp.ptr) return invalid(roleLkp.error);
    const auto role = roleLkp.ptr;

    const auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const auto [subjectType, subjectId] = *subjLkp.ptr;

    const auto vr = std::make_shared<VaultRole>();
    vr->role_id = role->id;
    vr->vault_id = vault->id;
    vr->subject_type = subjectType;
    vr->subject_id = subjectId;

    PermsQueries::assignVaultRole(vr);

    return ok("Successfully assigned role '" + role->name + "' to vault '" + vault->name + "'");
}

static CommandResult handle_vault_role_remove(const CommandCall& call) {
    constexpr const auto* ERR = "vault role remove";

    if (call.positionals.size() < 2) return invalid("vault remove: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault remove: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg = call.positionals.at(1);

    const auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp || !vLkp.ptr) return invalid(vLkp.error);
    const auto vault = vLkp.ptr;

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault remove: you do not have permission to remove roles from this vault");

        if (!call.user->canManageRoles()) return invalid("vault remove: you do not have permission to manage roles");
    }

    const auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp || !subjLkp.ptr) return invalid(subjLkp.error);
    const auto subj = *subjLkp.ptr;

    const auto roleLkp = resolveVRole(roleArg, vault, &subj, ERR);
    if (!roleLkp || !roleLkp.ptr) return invalid(roleLkp.error);
    const auto role = roleLkp.ptr;

    PermsQueries::removeVaultRoleAssignment(role->id);

    return ok("Successfully removed role '" + role->name + "' from vault '" + vault->name + "'");
}

static CommandResult handle_vault_role_list(const CommandCall& call) {
    constexpr const auto* ERR = "vault role list";

    if (call.positionals.size() > 1) return invalid("vault list: too many arguments");

    std::shared_ptr<Vault> vault;
    if (!call.positionals.empty()) {
        const auto vaultArg = call.positionals.at(0);

        const auto vLkp = resolveVault(call, vaultArg, ERR);
        if (!vLkp || !vLkp.ptr) return invalid(vLkp.error);
        vault = vLkp.ptr;

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

    const auto [sub, subcall] = descend(call);

    if (isVaultRoleOverrideMatch({"add"}, sub)) return handle_vault_role_override_add(subcall);
    if (isVaultRoleOverrideMatch({"remove"}, sub)) return handle_vault_role_override_remove(subcall);
    if (isVaultRoleOverrideMatch({"update"}, sub)) return handle_vault_role_override_update(subcall);
    if (isVaultRoleOverrideMatch({"list"}, sub)) return handle_vault_role_override_list(subcall);

    return invalid(call.constructFullArgs(), "Unknown vault override action: '" + std::string(sub) + "'");
}

CommandResult handle_vault_role(const CommandCall& call) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;

    if (call.positionals.empty()) return usage(call.constructFullArgs());
    if (call.positionals.size() > 1) return invalid(call.constructFullArgs(), "vault role: too many arguments");

    const auto [sub, subcall] = descend(call);

    if (isVaultRoleMatch({"assign"}, sub)) return handle_vault_role_assign(subcall);
    if (isVaultRoleMatch({"remove"}, sub)) return handle_vault_role_remove(subcall);
    if (isVaultRoleMatch({"list"}, sub)) return handle_vault_role_list(subcall);
    if (isVaultRoleMatch({"override"}, sub)) return handle_vault_role_override(subcall);

    return invalid(call.constructFullArgs(), "Unknown vault role subcommand: '" + std::string(sub) + "'");
}

}
