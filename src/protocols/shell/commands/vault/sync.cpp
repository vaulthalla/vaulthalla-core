#include "protocols/shell/commands/vault.hpp"
#include "util/shellArgsHelpers.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "logging/LogRegistry.hpp"
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
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"

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
    constexpr const auto* ERR = "vault sync";

    if (call.positionals.size() > 1)
        return invalid(call.constructFullArgs(), "vault sync: too many arguments");

    const auto vaultArg = call.positionals[0];

    const auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp || !vLkp.ptr) return invalid(vLkp.error);
    const auto vault = vLkp.ptr;

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id && !call.user->
        canSyncVaultData(vault->id)) return invalid("vault sync: you do not have permission to manage this vault");

    ServiceDepsRegistry::instance().syncController->runNow(vault->id);

    return ok("Vault sync initiated for '" + vault->name + "' (ID: " + std::to_string(vault->id) + ")");
}

static CommandResult handle_vault_sync_update(const CommandCall& call) {
    constexpr const auto* ERR = "vault sync update";

    if (call.positionals.empty()) return usage(call.constructFullArgs());
    if (call.positionals.size() > 1)
        return invalid(call.constructFullArgs(), "vault sync update: too many arguments");

    const auto eLkp = resolveEngine(call, call.positionals[0], ERR);
    if (!eLkp || !eLkp.ptr) return invalid(eLkp.error);
    const auto engine = eLkp.ptr;

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
    constexpr const auto* ERR = "vault sync info";
    if (call.positionals.empty()) return usage(call.constructFullArgs());
    if (call.positionals.size() > 1)
        return invalid(call.constructFullArgs(), "vault sync info: too many arguments");

    try {
        const auto eLkp = resolveEngine(call, call.positionals[0], ERR);
        if (!eLkp || !eLkp.ptr) return invalid(eLkp.error);
        const auto engine = eLkp.ptr;

        if (!call.user->canManageVaults() && engine->vault->owner_id != call.user->id &&
            !call.user->canSyncVaultData(engine->vault->id))
            return invalid("vault sync info: you do not have permission to view this vault's sync configuration");

        if (!engine->sync) return invalid("vault sync info: vault does not have a sync configuration");

        return ok(to_string(engine->sync));

    } catch (const std::invalid_argument& e) {
        return invalid("vault sync info: " + std::string(e.what()));
    }
}

static bool isVaultSyncMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"vault", "sync", cmd}, input);
}

CommandResult commands::vault::handle_sync(const CommandCall& call) {
    if (call.positionals.empty()) return usage(call.constructFullArgs());
    if (call.positionals.size() > 2)
        return invalid(call.constructFullArgs(), "vault sync: too many arguments");

    const auto [arg, subcall] = descend(call);

    if (call.positionals.size() == 1) return handle_vault_sync(call);

    if (isVaultSyncMatch("update", arg)) return handle_vault_sync_update(subcall);
    if (isVaultSyncMatch("info", arg)) return handle_vault_sync_info(subcall);

    return invalid(call.constructFullArgs(), "vault sync: unknown subcommand: '" + std::string(arg) + "'");
}
