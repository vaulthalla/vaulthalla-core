#include "protocols/shell/commands/vault.hpp"
#include "types/Vault.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/PermissionOverride.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::services;

std::optional<unsigned int> commands::vaulparsePositiveUint(const std::string& s, const char* errLabel, std::string& errOut) {
    if (const auto v = parseInt(s)) {
        if (*v <= 0) { errOut = std::string(errLabel) + " must be a positive integer"; return std::nullopt; }
        return static_cast<unsigned int>(*v);
    }
    errOut = std::string(errLabel) + " must be a positive integer";
    return std::nullopt;
}

// Resolve owner id from --owner <id|name>. When resolving a vault by name,
// we *require* --owner to disambiguate (matches your usage text).
Lookup<User> commands::vaulresolveOwnerRequired(const CommandCall& call, const std::string& errPrefix) {
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

Lookup<Vault> commands::vaulresolveVault(const CommandCall& call, const std::string& vaultArg, const std::string& errPrefix) {
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

Lookup<StorageEngine> commands::vaulresolveEngine(const CommandCall& call, const std::string& vaultArg, const std::string& errPrefix) {
    Lookup<StorageEngine> out;
    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) {
            out.error = errPrefix + ": vault ID must be a positive integer";
            return out;
        }
        out.ptr = ServiceDepsRegistry::instance().storageManager->getEngine(*vaultIdOpt);
        if (!out.ptr) out.error = errPrefix + ": no storage engine found for vault ID " + std::to_string(*vaultIdOpt);
        return out;
    }

    unsigned int ownerId;
    if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
        if (*ownerIdOpt <= 0) {
            out.error = errPrefix + ": --owner must be a positive integer";
            return out;
        }
        ownerId = *ownerIdOpt;
    } else if (const auto ownerOpt = optVal(call, "owner")) {
        if (ownerOpt->empty()) {
            out.error = errPrefix + ": --owner requires a value";
            return out;
        }
        const auto owner = UserQueries::getUserByName(*ownerOpt);
        if (!owner) {
            out.error = errPrefix + ": owner not found";
            return out;
        }
        ownerId = owner->id;
    } else ownerId = call.user->id;

    const auto vault = VaultQueries::getVault(vaultArg, ownerId);
    if (!vault) {
        out.error = errPrefix + ": vault with name '" + vaultArg + "' not found for user ID " + std::to_string(ownerId);
        return out;
    }

    out.ptr = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    if (!out.ptr) out.error = errPrefix + ": no storage engine found for vault '" + vaultArg + "'";
    return out;
}

std::optional<std::string> commands::vaulcheckOverridePermissions(const CommandCall& call, const std::shared_ptr<Vault>& vault, const std::string& errPrefix) {
    if (vault->owner_id == call.user->id) return std::nullopt;
    const bool canManageThisVault =
        call.user->canManageVaults() || call.user->canManageVaultAccess(vault->id);
    if (!canManageThisVault)
        return errPrefix + ": you do not have permission to override roles for this vault";
    if (!call.user->canManageRoles())
        return errPrefix + ": you do not have permission to manage roles";
    return std::nullopt;
}

Lookup<Role> commands::vaulresolveRole(const std::string& roleArg, const std::string& errPrefix) {
    Lookup<Role> out;

    if (const auto roleIdOpt = parseInt(roleArg)) {
        if (*roleIdOpt <= 0) {
            out.error = errPrefix + ": role ID must be a positive integer";
            return out;
        }
        out.ptr = PermsQueries::getRole(*roleIdOpt);
    } else out.ptr = PermsQueries::getRoleByName(roleArg);

    if (!out.ptr) out.error = errPrefix + ": role not found";
    return out;
}

Lookup<VaultRole> commands::vaulresolveVRole(const std::string& roleArg,
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

PatternParse commands::vaulparsePatternOpt(const CommandCall& call, bool required, const std::string& errPrefix) {
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

EnableParse commands::vaulparseEnableDisableOpt(const CommandCall& call, const std::string& errPrefix) {
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

EffectParse commands::vaulparseEffectChangeOpt(const CommandCall& call, const std::string& errPrefix) {
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
