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

namespace {

// Small carrier for lookups that might fail, with an error message.
template <typename T>
struct Lookup {
    std::shared_ptr<T> ptr;
    std::string error;
    explicit operator bool() const { return static_cast<bool>(ptr); }
};

// ---------- Common parsing helpers ----------

std::optional<unsigned int> parsePositiveUint(const std::string& s, const char* errLabel, std::string& errOut) {
    if (const auto v = parseInt(s)) {
        if (*v <= 0) { errOut = std::string(errLabel) + " must be a positive integer"; return std::nullopt; }
        return static_cast<unsigned int>(*v);
    }
    errOut = std::string(errLabel) + " must be a positive integer";
    return std::nullopt;
}

// Resolve owner id from --owner <id|name>. When resolving a vault by name,
// we *require* --owner to disambiguate (matches your usage text).
Lookup<struct User> resolveOwnerRequired(const CommandCall& call, const std::string& errPrefix) {
    Lookup<User> out;
    const auto ownerOpt = optVal(call, "owner");
    if (!ownerOpt || ownerOpt->empty()) {
        out.error = errPrefix + ": when using a vault name, you must specify --owner <id|name>";
        return out;
    }
    if (const auto idOpt = parseInt(*ownerOpt)) {
        if (*idOpt <= 0) { out.error = errPrefix + ": --owner must be a positive integer"; return out; }
        out.ptr = UserQueries::getUserById(*idOpt);
        if (!out.ptr) out.error = errPrefix + ": owner id not found: " + *ownerOpt;
        return out;
    }
    out.ptr = UserQueries::getUserByName(*ownerOpt);
    if (!out.ptr) out.error = errPrefix + ": owner not found: " + *ownerOpt;
    return out;
}

// Resolve vault from first positional: <vault-id|vault-name>
// If name, requires --owner (per usage).
Lookup<Vault> resolveVault(const CommandCall& call, const std::string& vaultArg, const std::string& errPrefix) {
    Lookup<Vault> out;
    if (const auto idOpt = parseInt(vaultArg)) {
        if (*idOpt <= 0) { out.error = errPrefix + ": vault ID must be a positive integer"; return out; }
        out.ptr = VaultQueries::getVault(*idOpt);
        if (!out.ptr) out.error = errPrefix + ": vault with id " + std::to_string(*idOpt) + " not found";
        return out;
    }
    // Named vault -> require owner
    auto ownerLkp = resolveOwnerRequired(call, errPrefix);
    if (!ownerLkp) { out.error = std::move(ownerLkp.error); return out; }
    out.ptr = VaultQueries::getVault(vaultArg, ownerLkp.ptr->id);
    if (!out.ptr) out.error = errPrefix + ": vault named '" + vaultArg + "' (owner id " + std::to_string(ownerLkp.ptr->id) + ") not found";
    return out;
}

// Check permissions to manage overrides on a vault.
std::optional<std::string> checkOverridePermissions(const CommandCall& call, const std::shared_ptr<Vault>& vault, const std::string& errPrefix) {
    if (vault->owner_id == call.user->id) return std::nullopt;
    const bool canManageThisVault =
        call.user->canManageVaults() || call.user->canManageVaultAccess(vault->id);
    if (!canManageThisVault)
        return errPrefix + ": you do not have permission to override roles for this vault";
    if (!call.user->canManageRoles())
        return errPrefix + ": you do not have permission to manage roles";
    return std::nullopt;
}

// Parse --user/-u or --group/-g to (type, id)
struct Subject {
    std::string type;   // "user" or "group"
    unsigned int id = 0;
};

Lookup<Subject> parseSubject(const CommandCall& call, const std::string& errPrefix) {
    Lookup<Subject> out;

    const std::vector<std::string> userFlags = {"user", "u"};
    for (const auto& f : userFlags) {
        if (const auto v = optVal(call, f)) {
            if (const auto idOpt = parseInt(*v)) {
                if (*idOpt <= 0) { out.error = errPrefix + ": user ID must be a positive integer"; return out; }
                out.ptr = std::make_shared<Subject>(Subject{"user", static_cast<unsigned int>(*idOpt)});
                return out;
            }
            auto user = UserQueries::getUserByName(*v);
            if (!user) { out.error = errPrefix + ": user not found: " + *v; return out; }
            out.ptr = std::make_shared<Subject>(Subject{"user", static_cast<unsigned int>(user->id)});
            return out;
        }
    }

    const std::vector<std::string> groupFlags = {"group", "g"};
    for (const auto& f : groupFlags) {
        if (const auto v = optVal(call, f)) {
            if (const auto idOpt = parseInt(*v)) {
                if (*idOpt <= 0) { out.error = errPrefix + ": group ID must be a positive integer"; return out; }
                out.ptr = std::make_shared<Subject>(Subject{"group", static_cast<unsigned int>(*idOpt)});
                return out;
            }
            auto group = GroupQueries::getGroupByName(*v);
            if (!group) { out.error = errPrefix + ": group not found: " + *v; return out; }
            out.ptr = std::make_shared<Subject>(Subject{"group", static_cast<unsigned int>(group->id)});
            return out;
        }
    }

    out.error = errPrefix + ": must specify either --user/-u or --group/-g";
    return out;
}

// Role resolution: if roleArg is an int, fetch by id.
// Otherwise (non-int), and a subject was provided, fallback to subject+vault.
// (Matches your original behavior.)
Lookup<VaultRole> resolveRole(const std::string& roleArg,
                              const std::shared_ptr<Vault>& vault,
                              const Subject* subjectOrNull,
                              const std::string& errPrefix) {
    Lookup<VaultRole> out;
    if (const auto idOpt = parseInt(roleArg)) {
        if (*idOpt <= 0) { out.error = errPrefix + ": role ID must be a positive integer"; return out; }
        out.ptr = PermsQueries::getVaultRole(*idOpt);
        if (!out.ptr) out.error = errPrefix + ": role with id " + std::to_string(*idOpt) + " not found";
        return out;
    }
    if (!subjectOrNull) {
        out.error = errPrefix + ": non-integer role arg requires a subject (--user/--group) to infer the role";
        return out;
    }
    out.ptr = PermsQueries::getVaultRoleBySubjectAndVaultId(subjectOrNull->id, subjectOrNull->type, vault->id);
    if (!out.ptr) out.error = errPrefix + ": role not found for " + subjectOrNull->type + " id " + std::to_string(subjectOrNull->id);
    return out;
}

// Parse pattern (support both --path and --pattern). If required and missing -> error.
struct PatternParse {
    bool ok = false;
    std::string raw;
    std::optional<std::regex> compiled;
    std::string error;
};
PatternParse parsePatternOpt(const CommandCall& call, bool required, const std::string& errPrefix) {
    PatternParse out;
    auto p = optVal(call, "path");
    if (!p) p = optVal(call, "pattern");
    if (!p || p->empty()) {
        if (required) out.error = errPrefix + ": --path/--pattern is required";
        else { out.ok = true; }
        return out;
    }
    out.raw = *p;
    try {
        out.compiled = std::regex(out.raw);
    } catch (const std::regex_error&) {
        out.error = errPrefix + ": invalid regex for --path/--pattern";
        return out;
    }
    out.ok = true;
    return out;
}

// Parse enable/disable. Return {std::nullopt} if neither present.
// Error if both are present.
struct EnableParse {
    bool ok = false;
    std::optional<bool> value;
    std::string error;
};
EnableParse parseEnableDisableOpt(const CommandCall& call, const std::string& errPrefix) {
    EnableParse out;
    const bool hasEnable  = hasFlag(call, "enable");
    const bool hasDisable = hasFlag(call, "disable");
    if (hasEnable && hasDisable) {
        out.error = errPrefix + ": cannot specify both --enable and --disable";
        return out;
    }
    if (hasEnable)  out.value = true;
    if (hasDisable) out.value = false;
    out.ok = true;
    return out;
}

// Parse effect change for UPDATE: support --allow / --deny and also
// --allow-effect / --deny-effect as synonyms.
struct EffectParse {
    bool ok = false;
    std::optional<OverrideOpt> value;
    std::string error;
};
EffectParse parseEffectChangeOpt(const CommandCall& call, const std::string& errPrefix) {
    EffectParse out;
    const bool allowFlag = hasFlag(call, "allow") || hasFlag(call, "allow-effect");
    const bool denyFlag  = hasFlag(call, "deny")  || hasFlag(call, "deny-effect");
    if (allowFlag && denyFlag) {
        out.error = errPrefix + ": cannot set both --allow and --deny";
        return out;
    }
    if (allowFlag) out.value = OverrideOpt::ALLOW;
    if (denyFlag)  out.value = OverrideOpt::DENY;
    out.ok = true;
    return out;
}

} // namespace


// ---------------- ADD ----------------

static CommandResult handle_vault_role_override_add(const CommandCall& call) {
    constexpr const char* ERR = "vault override";

    if (call.positionals.size() < 2) return invalid(std::string(ERR) + ": missing <vault_id|vault_name> and <role_id|role_hint>");
    if (call.positionals.size() > 2) return invalid(std::string(ERR) + ": too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg  = call.positionals.at(1);

    // Vault
    auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    auto vault = vLkp.ptr;

    // AuthZ
    if (auto err = checkOverridePermissions(call, vault, ERR)) return invalid(*err);

    // Subject (required for ADD)
    auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const Subject subj = *subjLkp.ptr;

    // Role (by id if numeric, otherwise fallback to subject+vault)
    auto roleLkp = resolveRole(roleArg, vault, &subj, ERR);
    if (!roleLkp) return invalid(roleLkp.error);
    auto role = roleLkp.ptr;

    // Permissions to override
    const auto parsedPerms = parseExplicitVaultPermissionFlags(call);
    if (parsedPerms.allow.empty() && parsedPerms.deny.empty())
        return invalid(std::string(ERR) + ": must specify at least one permission using --<perm> (allow) or --unset-<perm> (deny)");

    // Pattern is REQUIRED for ADD (keeps your existing behavior; supports --path OR --pattern)
    auto patt = parsePatternOpt(call, /*required=*/true, ERR);
    if (!patt.ok) return invalid(patt.error);

    // Enabled?
    auto en = parseEnableDisableOpt(call, ERR);
    if (!en.ok) return invalid(en.error);
    const bool enabled = en.value.value_or(true);

    // Prepare and insert overrides
    const auto applyOne = [&](const VaultPermission& perm, OverrideOpt effect) {
        auto ov = std::make_shared<PermissionOverride>();
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


// ---------------- UPDATE ----------------

static CommandResult handle_vault_role_override_update(const CommandCall& call) {
    constexpr const char* ERR = "vault override update";

    // Now treated as: <vault-id|vault-name> <role_id|role_hint> <bit_position>
    if (call.positionals.size() < 3)
        return invalid(std::string(ERR) + ": missing <vault-id|vault-name> <role_id|role_hint> <bit_position>");
    if (call.positionals.size() > 3)
        return invalid(std::string(ERR) + ": too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto roleArg  = call.positionals.at(1);
    const auto bitArg   = call.positionals.at(2);

    // Vault
    auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp) return invalid(vLkp.error);
    auto vault = vLkp.ptr;

    // AuthZ
    if (auto err = checkOverridePermissions(call, vault, ERR)) return invalid(*err);

    // SUBJECT IS REQUIRED
    auto subjLkp = parseSubject(call, ERR);
    if (!subjLkp) return invalid(subjLkp.error);
    const Subject subj = *subjLkp.ptr;

    // Role must resolve within (vault, subject)
    auto roleLkp = resolveRole(roleArg, vault, &subj, ERR);
    if (!roleLkp) return invalid(roleLkp.error);
    auto role = roleLkp.ptr;

    // bit_position
    std::string bitErr;
    auto bitPosOpt = parsePositiveUint(bitArg, "bit position", bitErr);
    if (!bitPosOpt) return invalid(std::string(ERR) + ": " + bitErr);
    const unsigned int bitPosition = *bitPosOpt;

    // Locate override by (vault, subject, bit)
    VPermOverrideQuery q{
        .vault_id     = vault->id,
        .subject_type = subj.type,   // "user" | "group"
        .subject_id   = subj.id,
        .bit_position = bitPosition
    };

    auto ov = PermsQueries::getVPermOverride(q);
    if (!ov) {
        return invalid(std::string(ERR) + ": no override found for (vault="
                       + std::to_string(vault->id) + ", " + subj.type + "="
                       + std::to_string(subj.id) + ", bit=" + std::to_string(bitPosition) + ")");
    }

    // Defensive: ensure the override we fetched indeed belongs to the resolved role assignment
    if (ov->assignment_id != role->id) {
        return invalid(std::string(ERR) + ": override does not belong to role '"
                       + role->name + "' (assignment mismatch)");
    }

    bool changed = false;

    // Effect change? (--allow/--deny or --allow-effect/--deny-effect)
    auto eff = parseEffectChangeOpt(call, ERR);
    if (!eff.ok) return invalid(eff.error);
    if (eff.value) {
        ov->effect = *eff.value;
        changed = true;
    }

    // Pattern change? (--path/--pattern) optional
    auto patt = parsePatternOpt(call, /*required=*/false, ERR);
    if (!patt.ok) return invalid(patt.error);
    if (patt.compiled) {
        ov->pattern    = *patt.compiled;
        ov->patternStr = patt.raw;
        changed = true;
    }

    // Enabled/disabled?
    auto en = parseEnableDisableOpt(call, ERR);
    if (!en.ok) return invalid(en.error);
    if (en.value) {
        ov->enabled = *en.value;
        changed = true;
    }

    if (!changed) {
        return invalid(std::string(ERR) + ": no changes specified (set at least one of --allow/--deny, --path/--pattern, --enable/--disable)");
    }

    PermsQueries::updateVPermOverride(ov);

    return ok("Updated override (vault=" + std::to_string(vault->id) +
              ", " + subj.type + "=" + std::to_string(subj.id) +
              ", bit=" + std::to_string(bitPosition) + ") on role '" + role->name + "'");
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

static CommandResult handle_vault_role_override_remove(const CommandCall& call) {}

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
