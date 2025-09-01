#include "protocols/shell/commands/vault.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/WaiverQueries.hpp"

#include "storage/StorageManager.hpp"
#include "storage/cloud/s3/S3Controller.hpp"

#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/APIKey.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "types/User.hpp"

#include "services/LogRegistry.hpp"
#include "config/ConfigRegistry.hpp"

#include <optional>
#include <string>
#include <vector>
#include <memory>

using namespace vh::shell::commands::vault;
using namespace vh::shell::commands;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;
using namespace vh::crypto;
using namespace vh::util;
using namespace vh::logging;
using namespace vh::cloud;

CommandResult vault::handle_vault_update(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault update: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("vault update: too many arguments");

    const auto nameOrId = call.positionals[0];
    const auto idOpt = parseInt(nameOrId);

    std::shared_ptr<Vault> vault;
    if (idOpt) vault = VaultQueries::getVault(*idOpt);
    else {
        if (call.positionals.size() < 2) return invalid(
            "vault update: missing required <owner-id> for vault name update");

        const auto ownerId = std::stoi(call.positionals[1]);
        vault = VaultQueries::getVault(nameOrId, ownerId);
    }

    if (!vault) return invalid("vault update: vault with ID " + std::to_string(*idOpt) + " not found");

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id) return invalid(
        "vault update: you do not have permission to update this vault");

    const auto descOpt = optVal(call, "desc");
    const auto quotaOpt = optVal(call, "quota");
    const auto ownerIdOpt = optVal(call, "owner-id");
    const auto syncStrategyOpt = optVal(call, "sync-strategy");
    const auto onSyncConflictOpt = optVal(call, "on-sync-conflict");
    const auto apiKeyOpt = optVal(call, "api-key");
    const auto bucketOpt = optVal(call, "bucket");

    if (vault->type == VaultType::Local && (apiKeyOpt || bucketOpt)) return invalid(
        "vault update: --api-key and --bucket are not valid for local vaults");

    if (descOpt) vault->description = *descOpt;

    if (quotaOpt) {
        if (*quotaOpt == "unlimited") vault->quota = 0; // 0 means unlimited
        else vault->quota = parseSize(*quotaOpt);
    }

    if (ownerIdOpt) {
        const auto ownerId = parseInt(*ownerIdOpt);
        if (!ownerId || *ownerId <= 0) return invalid("vault update: --owner <id> must be a positive integer");
        vault->owner_id = *ownerId;
    }

    std::shared_ptr<Sync> sync;
    if (syncStrategyOpt || onSyncConflictOpt) {
        sync = VaultQueries::getVaultSyncConfig(vault->id);
        if (!sync) return invalid("vault update: vault does not have a sync configuration");

        if (vault->type == VaultType::Local) {
            if (syncStrategyOpt) return invalid("vault update: --sync-strategy is not valid for local vaults");
            if (onSyncConflictOpt) {
                const auto fsync = std::static_pointer_cast<FSync>(sync);
                fsync->conflict_policy = fsConflictPolicyFromString(*onSyncConflictOpt);
            }
        }

        if (syncStrategyOpt) {
            const auto strategy = strategyFromString(*syncStrategyOpt);
            if (vault->type == VaultType::S3) {
                const auto rsync = std::static_pointer_cast<RSync>(sync);
                rsync->strategy = strategy;
            }
        }
    }

    if (apiKeyOpt || bucketOpt) {
        if (vault->type == VaultType::Local) return invalid(
            "vault update: --api-key and --bucket are not valid for local vaults");

        const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);

        if (apiKeyOpt) {
            const auto apiKeyId = parseInt(*apiKeyOpt);
            if (apiKeyId && APIKeyQueries::getAPIKey(*apiKeyId)) s3Vault->api_key_id = *apiKeyId;
            else {
                const auto apiKey = APIKeyQueries::getAPIKey(*apiKeyOpt);
                if (!apiKey) return invalid("vault update: API key not found: " + *apiKeyOpt);
                s3Vault->api_key_id = apiKey->id;
            }
        }

        if (bucketOpt) s3Vault->bucket = *bucketOpt;
    }

    const auto [okToProceed, waiver] = handle_encryption_waiver({call, vault, true});
    if (!okToProceed) return invalid("vault create: user did not accept encryption waiver");
    if (waiver) WaiverQueries::addWaiver(waiver);

    VaultQueries::upsertVault(vault, sync);

    return ok("Successfully updated vault!\n" + to_string(vault));
}


CommandResult commands::vault::handle_vault_delete(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault delete: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault delete: too many arguments");

    const std::string name = call.positionals[0];
    const auto idOpt = parseInt(name);

    std::shared_ptr<Vault> vault;

    if (idOpt) vault = VaultQueries::getVault(*idOpt);
    else {
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault delete: missing required --owner <id | name> for vault name lookup");

        if (const auto ownerIdOpt = parseInt(*ownerOpt)) vault = VaultQueries::getVault(name, *ownerIdOpt);
        else if (const auto owner =
            UserQueries::getUserByName(*ownerOpt)) vault = VaultQueries::getVault(name, call.user->id);
        else return invalid("vault delete: owner '" + *ownerOpt + "' not found");
    }

    if (!vault) return invalid("vault delete: vault with ID " + std::to_string(*idOpt) + " not found");

    if (!call.user->canManageVaults()) {
        if (call.user->id != vault->owner_id) return invalid(
            "vault delete: you do not have permission to delete this vault");

        if (!call.user->canDeleteVaultData(vault->id)) return invalid(
            "vault delete: you do not have permission to delete this vault's data");
    }

    ServiceDepsRegistry::instance().storageManager->removeVault(*idOpt);

    return ok("Successfully deleted vault '" + vault->name + "' (ID: " + std::to_string(vault->id) + ")\n");
}
