#include "protocols/shell/commands/vault.hpp"
#include "util/shellArgsHelpers.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "services/LogRegistry.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "services/SyncController.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "types/Sync.hpp"
#include "util/interval.hpp"
#include "VaultUsage.hpp"

#include <optional>
#include <string>
#include <vector>
#include <memory>

using namespace vh::shell;
using namespace vh::shell::commands;
using namespace vh::shell::commands::vault;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::services;
using namespace vh::util;
using namespace vh::logging;

static CommandResult handle_vault_sync(const CommandCall& call) {
    if (call.positionals.size() > 1) return invalid("vault sync: too many arguments\n\n" + VaultUsage::vault_sync().str());

    const auto vaultArg = call.positionals[0];
    std::shared_ptr<Vault> vault;

    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) return invalid("vault sync: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault sync: missing required --owner <id | name> for vault name lookup");

        unsigned int ownerId;

        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault sync: --owner <id> must be a positive integer");
            ownerId = *ownerIdOpt;
        } else {
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault sync: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        }
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault sync: vault with arg '" + vaultArg + "' not found");

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id && !call.user->
        canSyncVaultData(vault->id)) return invalid("vault sync: you do not have permission to manage this vault");

    ServiceDepsRegistry::instance().syncController->runNow(vault->id);

    return ok("Vault sync initiated for '" + vault->name + "' (ID: " + std::to_string(vault->id) + ")");
}

static std::shared_ptr<StorageEngine> extractEngineFromArgs(const CommandCall& call, const std::string& vaultArg) {
    std::shared_ptr<StorageEngine> engine;

    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) throw std::invalid_argument("vault sync update: vault ID must be a positive integer");
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(*vaultIdOpt);
    } else {
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) throw std::invalid_argument("vault sync update: missing required --owner <id | name> for vault name lookup");
        unsigned int ownerId;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) throw std::invalid_argument("vault sync update: --owner <id> must be a positive integer");
            ownerId = *ownerIdOpt;
        } else {
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) throw std::invalid_argument("vault sync update: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        }
        const auto vault = VaultQueries::getVault(vaultArg, ownerId);
        if (!vault) throw std::invalid_argument(
            "vault sync update: vault with name '" + vaultArg + "' not found for user ID " + std::to_string(ownerId));
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    }

    if (!engine) throw std::invalid_argument("vault sync update: no storage engine found for vault '" + vaultArg + "'");

    return engine;
}

static CommandResult handle_vault_sync_update(const CommandCall& call) {
    if (call.positionals.empty()) return {0, VaultUsage::vault_sync().str(), ""};
    if (call.positionals.size() > 1) return invalid("vault sync update: too many arguments\n\n" + VaultUsage::vault_sync().str());

    const auto engine = extractEngineFromArgs(call, call.positionals[0]);

    if (!engine) return invalid("vault sync update: no storage engine found for vault '" + call.positionals[0] + "'");

    if (!call.user->canManageVaults() && engine->vault->owner_id != call.user->id &&
        !call.user->canSyncVaultData(engine->vault->id))
        return invalid("vault sync update: you do not have permission to manage this vault");

    const auto& sync = engine->sync;

    if (const auto intervalOpt = optVal(call, "interval")) {
        try {
            sync->interval = parseSyncInterval(*intervalOpt);
        } catch (const std::exception& e) {
            return invalid("vault sync update: " + std::string(e.what()));
        }
    }

    if (engine->vault->type == VaultType::Local) {
        if (hasFlag(call, "sync-strategy")) return invalid(
            "vault sync update: --sync-strategy is not valid for local vaults");
        if (const auto onSyncConflictOpt = optVal(call, "on-sync-conflict")) {
            const auto fsync = std::static_pointer_cast<FSync>(sync);

            try {
                fsync->conflict_policy = fsConflictPolicyFromString(*onSyncConflictOpt);
            } catch (const std::exception& e) {
                return invalid("vault sync update: " + std::string(e.what()));
            }
        }
    } else if (engine->vault->type == VaultType::S3) {
        const auto rsync = std::static_pointer_cast<RSync>(sync);

        if (const auto syncStrategyOpt = optVal(call, "sync-strategy")) {
            try {
                rsync->strategy = strategyFromString(*syncStrategyOpt);
            } catch (const std::exception& e) {
                return invalid("vault sync update: " + std::string(e.what()));
            }
        }

        if (const auto onSyncConflictOpt = optVal(call, "on-sync-conflict")) {
            try {
                rsync->conflict_policy = rsConflictPolicyFromString(*onSyncConflictOpt);
            } catch (const std::exception& e) {
                return invalid("vault sync update: " + std::string(e.what()));
            }
        }
    }

    if (engine->vault->type == VaultType::Local) {
        const auto fsync = std::static_pointer_cast<FSync>(sync);
        return ok("Successfully updated local vault sync configuration!\n" + to_string(fsync));
    }

    if (engine->vault->type == VaultType::S3) {
        const auto rsync = std::static_pointer_cast<RSync>(sync);
        return ok("Successfully updated S3 vault sync configuration!\n" + to_string(rsync));
    }

    return invalid("vault sync update: invalid sync configuration");
}

static CommandResult handle_vault_sync_info(const CommandCall& call) {
    if (call.positionals.empty()) return {0, VaultUsage::vault_sync().str(), ""};
    if (call.positionals.size() > 1) return invalid("vault sync info: too many arguments\n\n" + VaultUsage::vault_sync().str());

    try {
        const auto engine = extractEngineFromArgs(call, call.positionals[0]);

        if (!call.user->canManageVaults() && engine->vault->owner_id != call.user->id &&
            !call.user->canSyncVaultData(engine->vault->id))
            return invalid("vault sync info: you do not have permission to view this vault's sync configuration");

        if (!engine->sync) return invalid("vault sync info: vault does not have a sync configuration");

        return ok(to_string(engine->sync));

    } catch (const std::invalid_argument& e) {
        return invalid("vault sync info: " + std::string(e.what()));
    }
}

CommandResult vault::handle_sync(const CommandCall& call) {
    if (call.positionals.empty()) return {0, VaultUsage::vault_sync().str(), ""};
    if (call.positionals.size() > 2) return invalid("vault sync: too many arguments\n\n" + VaultUsage::vault_sync().str());

    const auto arg = call.positionals[0];

    if (call.positionals.size() > 1) {
        CommandCall subcall = call;
        subcall.positionals.erase(subcall.positionals.begin());
        if (arg == "set" || arg == "update") return handle_vault_sync_update(subcall);
        if (arg == "info" || arg == "get") return handle_vault_sync_info(subcall);
    }

    if (call.positionals.size() == 1) return handle_vault_sync(call);

    return ok("Unrecognized command: " + arg + "\n" + VaultUsage::vault_sync().str());
}
