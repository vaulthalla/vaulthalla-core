#include "protocols/shell/commands/vault.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/User.hpp"
#include "types/VaultRole.hpp"
#include "types/PermissionOverride.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "CommandUsage.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "util/interval.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::services;

namespace vh::shell::commands::vault {

std::optional<unsigned int> parsePositiveUint(const std::string& s, const char* errLabel, std::string& errOut) {
    if (const auto v = parseUInt(s)) {
        if (*v <= 0) { errOut = std::string(errLabel) + " must be a positive integer"; return std::nullopt; }
        return static_cast<unsigned int>(*v);
    }
    errOut = std::string(errLabel) + " must be a positive integer";
    return std::nullopt;
}

std::shared_ptr<User> resolveOwner(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage) {
    if (const auto ownerOpt = optVal(call, usage->resolveOptional("owner")->option_tokens)) {
        if (const auto idOpt = parseUInt(*ownerOpt)) {
            if (*idOpt <= 0) throw std::runtime_error("owner must be a positive integer");
            const auto user = UserQueries::getUserById(*idOpt);
            if (!user) throw std::runtime_error("owner id not found: " + *ownerOpt);
            return user;
        }
        const auto user = UserQueries::getUserByName(*ownerOpt);
        if (!user) throw std::runtime_error("owner not found: " + *ownerOpt);
        return user;
    }
    return call.user;
}

Lookup<User> resolveOwnerRequired(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::string& errPrefix) {
    Lookup<User> out;
    const auto ownerOpt = optVal(call, usage->resolveOptional("owner")->option_tokens);
    if (!ownerOpt || ownerOpt->empty()) {
        out.error = errPrefix + ": when using a vault name, you must specify --owner <id|name>";
        return out;
    }
    if (const auto idOpt = parseUInt(*ownerOpt)) {
        if (*idOpt <= 0) { out.error = errPrefix + ": --owner must be a positive integer"; return out; }
        out.ptr = UserQueries::getUserById(*idOpt);
        if (!out.ptr) out.error = errPrefix + ": owner id not found: " + *ownerOpt;
        return out;
    }
    out.ptr = UserQueries::getUserByName(*ownerOpt);
    if (!out.ptr) out.error = errPrefix + ": owner not found: " + *ownerOpt;
    return out;
}

Lookup<Vault> resolveVault(const CommandCall& call, const std::string& vaultArg, const std::shared_ptr<CommandUsage>& usage, const std::string& errPrefix) {
    Lookup<Vault> out;
    if (const auto idOpt = parseUInt(vaultArg)) {
        if (*idOpt <= 0) { out.error = errPrefix + ": vault ID must be a positive integer"; return out; }
        out.ptr = VaultQueries::getVault(*idOpt);
        if (!out.ptr) out.error = errPrefix + ": vault with id " + std::to_string(*idOpt) + " not found";
        return out;
    }

    auto ownerLkp = resolveOwnerRequired(call, usage, errPrefix);
    if (!ownerLkp) { out.error = std::move(ownerLkp.error); return out; }
    out.ptr = VaultQueries::getVault(vaultArg, ownerLkp.ptr->id);
    if (!out.ptr) out.error = errPrefix + ": vault named '" + vaultArg + "' (owner id " + std::to_string(ownerLkp.ptr->id) + ") not found";
    return out;
}

Lookup<StorageEngine> resolveEngine(const CommandCall& call, const std::string& vaultArg, const std::shared_ptr<CommandUsage>& usage, const std::string& errPrefix) {
    Lookup<StorageEngine> out;

    const auto vLkp = resolveVault(call, vaultArg, usage, errPrefix);
    if (!vLkp || !vLkp.ptr) { out.error = vLkp.error; return out; }
    const auto vault = vLkp.ptr;

    out.ptr = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    if (!out.ptr) out.error = errPrefix + ": no storage engine found for vault '" + vaultArg + "'";
    return out;
}

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

Lookup<VaultRole> resolveVRole(const std::string& roleArg,
                              const std::shared_ptr<Vault>& vault,
                              const Subject* subjectOrNull,
                              const std::string& errPrefix) {
    Lookup<VaultRole> out;
    if (const auto idOpt = parseUInt(roleArg)) {
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

std::unique_ptr<VaultType> parseVaultType(const CommandCall& call) {
    const bool local = hasFlag(call, "local");
    const bool s3    = hasFlag(call, "s3");
    if (local && s3) throw std::runtime_error("--local and --s3 are mutually exclusive");
    if (local) return std::make_unique<VaultType>(VaultType::Local);
    if (s3)    return std::make_unique<VaultType>(VaultType::S3);

    throw std::runtime_error("Vault type not specified: must provide either --local or --s3");
}

void assignDescIfAvailable(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<Vault>& vault) {
    if (const auto descOpt = optVal(call, usage->resolveOptional("description")->option_tokens))
        vault->description = *descOpt;
}

void assignQuotaIfAvailable(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<Vault>& vault) {
    if (const auto quotaOpt = optVal(call, usage->resolveOptional("quota")->option_tokens)) {
        if (*quotaOpt == "none" || *quotaOpt == "unlimited") vault->quota = 0;
        else vault->quota = parseSize(*quotaOpt);
    }
}

void assignOwnerIfAvailable(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<types::Vault>& vault) {
    if (const auto ownerOpt = optVal(call, usage->resolveOptional("owner")->option_tokens)) {
        Lookup<User> ownerLkp;
        if (const auto idOpt = parseUInt(*ownerOpt)) {
            if (*idOpt <= 0) throw std::runtime_error("vault create: --owner must be a positive integer");
            ownerLkp.ptr = UserQueries::getUserById(*idOpt);
        } else ownerLkp.ptr = UserQueries::getUserByName(*ownerOpt);
        if (!ownerLkp.ptr) throw std::runtime_error("vault create: owner not found: " + *ownerOpt);
        vault->owner_id = ownerLkp.ptr->id;
    }
}

void parseSync(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<Vault>& vault, const std::shared_ptr<Sync>& sync) {
    if (const auto syncIntervalOpt = optVal(call, usage->resolveOptional("interval")->option_tokens))
        sync->interval = util::parseSyncInterval(*syncIntervalOpt);

    if (vault->type == VaultType::Local) {
        if (const auto conflictOpt = optVal(call, usage->resolveGroupOptional("Local Vault Options", "conflict")->option_tokens)) {
            const auto fsync = std::static_pointer_cast<FSync>(sync);
            fsync->conflict_policy = fsConflictPolicyFromString(*conflictOpt);
        }
    } else if (vault->type == VaultType::S3) {
        if (const auto conflictOpt = optVal(call, usage->resolveGroupOptional("S3 Vault Options", "conflict")->option_tokens)) {
            const auto rsync = std::static_pointer_cast<RSync>(sync);
            rsync->conflict_policy = rsConflictPolicyFromString(*conflictOpt);
        }
    }

    if (vault->type == VaultType::S3) {
        const auto rsync = std::static_pointer_cast<RSync>(sync);
        if (const auto syncStrategyOpt = optVal(call, usage->resolveGroupOptional("S3 Vault Options", "strategy")->option_tokens))
            rsync->strategy = strategyFromString(*syncStrategyOpt);
    }
}

void parseS3API(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<Vault>& vault, const unsigned int ownerId, const bool required) {
    if (vault->type == VaultType::Local) return;

    const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);

    if (const auto apiKeyOpt = optVal(call, usage->resolveGroupOptional("S3 Vault Options", "api-key")->option_tokens)) {
        if (const auto apiKeyId = parseUInt(*apiKeyOpt); APIKeyQueries::getAPIKey(*apiKeyId)) s3Vault->api_key_id = *apiKeyId;
        else {
            const auto apiKey = APIKeyQueries::getAPIKey(*apiKeyOpt);
            if (!apiKey) throw std::runtime_error("API key not found: " + *apiKeyOpt);

            if ((ownerId != call.user->id || call.user->id != apiKey->user_id) && !call.user->canManageAPIKeys())
                throw std::runtime_error("you do not have permission to use an API key for another user");

            if (call.user->id != apiKey->user_id && !call.user->canManageAPIKeys())
                throw std::runtime_error("you do not have permission to use this API key");

            s3Vault->api_key_id = apiKey->id;
        }
    } else if (required) throw std::runtime_error("--api-key is required for S3 vaults");

    if (const auto bucketOpt = optVal(call, usage->resolveGroupOptional("S3 Vault Options", "bucket")->option_tokens)) {
        if (bucketOpt->empty()) throw std::runtime_error("--bucket cannot be empty");
        s3Vault->bucket = *bucketOpt;
    } else if (required) throw std::runtime_error("--bucket is required for S3 vaults");
}

}
