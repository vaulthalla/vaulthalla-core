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

#include "logging/LogRegistry.hpp"
#include "config/ConfigRegistry.hpp"
#include "util/interval.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
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

static constexpr const auto* SYNC_STRATEGY_HELP = R"(
Sync Strategy Options:
  cache  - Local cache of S3 bucket. Changes are uploaded to S3 on demand.
           Downloads are served from cache if available, otherwise fetched from S3.
  sync   - Two-way sync between local and S3. Changes in either location are propagated
           to the other during sync operations.
  mirror - One-way mirror of local to S3. Local changes are uploaded to S3,
           but changes in S3 are not downloaded locally.

)";

static constexpr const auto* LOCAL_CONFLICT_POLICY_HELP = R"(
On-Sync-Conflict Policy Options:
  overwrite  - In case of conflict, overwrite the remote with the local version.
  keep_both  - In case of conflict, keep both versions by renaming the remote.
  ask        - Prompt the user to resolve conflicts during sync operations.
)";

static constexpr const auto* REMOTE_CONFLICT_POLICY_HELP = R"(
On-Sync-Conflict Policy Options:
  keep_local  - In case of conflict, keep the local version and overwrite the remote.
  keep_remote - In case of conflict, keep the remote version and overwrite the local.
  ask         - Prompt the user to resolve conflicts during sync operations.

)";

static constexpr const auto& SYNC_INTERVAL_HELP = R"(
Sync Interval:

  S3 Vaults: Defines how often the system will synchronize changes between the local cache and the S3 bucket.
  Local Vaults: Sync is primarily event-driven, but this interval sets how often the system checks for filesystem changes.

  ⚠️  S3 Vaults only: Setting a very short interval (e.g., every few seconds) may lead to increased API usage and potential costs.
      Choose an interval that balances timeliness with cost-effectiveness.

  ⚠️  Setting a very short interval may lead to high CPU usage due to frequent filesystem checks.
      Choose an interval that balances timeliness with system performance.

  Format: A number followed by a time unit:
      s - seconds
      m - minutes
      h - hours
      d - days

  Examples:
    30s  - Every 30 seconds
    10m  - Every 10 minutes
    1h   - Every 1 hour
  Default is 15 minutes (15m).
)";

static CommandResult finish_vault_create(const CommandCall& call, std::shared_ptr<Vault>& v, const std::shared_ptr<Sync>& s) {
    const auto [okToProceed, waiver] = handle_encryption_waiver({call, v, false});
    if (!okToProceed) return invalid("vault create: user did not accept encryption waiver");

    v = ServiceDepsRegistry::instance().storageManager->addVault(v, s);
    if (waiver) WaiverQueries::addWaiver(waiver);

    return ok("\nSuccessfully created new vault!\n" + to_string(v));
}

static CommandResult handle_vault_create_failure(const std::shared_ptr<Vault>& v, const std::string& err) {
    if (VaultQueries::vaultExists(v->name, v->owner_id))
        ServiceDepsRegistry::instance().storageManager->removeVault(v->id);

    return invalid("\nvault create error: " + err);
}

static std::string stripLeadingDashes(const std::string& s) {
    size_t pos = 0;
    while (pos < s.size() && s[pos] == '-') ++pos;
    return s.substr(pos);
}

static CommandResult handle_vault_create_interactive(const CommandCall& call) {
    const auto& io = call.io;

    std::shared_ptr<Vault> v;

    const auto helpOptions = std::vector<std::string>{"help", "h", "?"};

    try {
        std::shared_ptr<Sync> sync;

        const auto type = io->prompt("Select vault type (local/s3) [local]:", "local");
        if (type == "local") {
            v = std::make_shared<Vault>();
            v->type = VaultType::Local;
        }
        else if (type == "s3") {
            v = std::make_shared<S3Vault>();
            v->type = VaultType::S3;
        }
        else return invalid("vault create: invalid vault type");

        v->name = io->prompt("Enter vault name (required):");
        if (v->name.empty()) return invalid("vault create: vault name is required");

        v->description = io->prompt("Enter vault description (optional):");

        const auto quotaStr = io->prompt("Enter vault quota (e.g. 10G, 500M) or leave blank for unlimited:");
        v->quota = quotaStr.empty() ? 0 : parseSize(quotaStr);

        const auto owner = io->prompt("Enter owner user ID or username (leave blank for yourself):");
        if (owner.empty()) v->owner_id = call.user->id;
        else {
            if (const auto ownerIdOpt = parseInt(owner)) {
                if (*ownerIdOpt <= 0) return invalid("vault create: owner ID must be a positive integer");
                v->owner_id = *ownerIdOpt;
            } else {
                const auto ownerUser = UserQueries::getUserByName(owner);
                if (!ownerUser) return invalid("vault create: user not found: " + owner);
                v->owner_id = ownerUser->id;
            }
        }

        if (v->owner_id != call.user->id && !call.user->canCreateVaults())
            return invalid(
                "vault create: user ID " + std::to_string(call.user->id) +
                " does not have permission to create vaults for other users");

        if (v->type == VaultType::Local) {
            auto fSync = std::make_shared<FSync>();

            auto conflictStr = io->prompt("Enter on-sync-conflict policy (overwrite/keep_both/ask) [overwrite] --help for details:", "overwrite");
            while (conflictStr == "help") {
                io->print(LOCAL_CONFLICT_POLICY_HELP);
                conflictStr = io->prompt("Enter on-sync-conflict policy (overwrite/keep_both/ask) [overwrite]:", "overwrite");
            }
            fSync->conflict_policy = fsConflictPolicyFromString(conflictStr);
            sync = fSync;
        }

        if (v->type == VaultType::S3) {
            const auto s3Vault = std::static_pointer_cast<S3Vault>(v);
            const auto apiKeyStr = io->prompt("Enter API key name or ID (required):");
            if (apiKeyStr.empty()) return invalid("vault create: API key is required for S3 vaults");

            std::shared_ptr<api::APIKey> apiKey;
            if (const auto apiKeyIdOpt = parseInt(apiKeyStr)) apiKey = APIKeyQueries::getAPIKey(*apiKeyIdOpt);
            else apiKey = APIKeyQueries::getAPIKey(apiKeyStr);

            if (!apiKey) return invalid("vault create: API key not found: " + apiKeyStr);
            if (apiKey->user_id != v->owner_id) {
                if (!call.user->canManageAPIKeys())
                    return invalid(
                        "vault create: user ID " + std::to_string(call.user->id) +
                        " does not have permission to assign API keys to other users vaults"
                        );

                const auto ownerUser = UserQueries::getUserById(v->owner_id);
                if (!ownerUser) return invalid("vault create: owner user ID not found: " + std::to_string(v->owner_id));

                if (apiKey->user_id != ownerUser->id && !ownerUser->canManageAPIKeys())
                    return invalid("vault create: API key '" + apiKeyStr + "' does not belong to user ID " +
                                   std::to_string(v->owner_id));
            }

            s3Vault->api_key_id = apiKey->id;

            s3Vault->bucket = io->prompt("Enter S3 bucket name (required):");
            if (s3Vault->bucket.empty()) return invalid("vault create: S3 bucket name is required");

            auto strategyStr = io->prompt("Enter sync strategy (cache/sync/mirror) [cache] --help for details:", "cache");
            while (std::ranges::find(helpOptions.begin(), helpOptions.end(), stripLeadingDashes(strategyStr)) != helpOptions.end()) {
                io->print(SYNC_STRATEGY_HELP);
                strategyStr = io->prompt("Enter sync strategy (cache/sync/mirror) [cache]:", "cache");
            }
            const auto rsync = std::make_shared<RSync>();
            rsync->strategy = strategyFromString(strategyStr);

            auto conflictStr = io->prompt("Enter on-sync-conflict policy (keep_local/keep_remote/ask) [ask] --help for details:", "ask");
            while (std::ranges::find(helpOptions.begin(), helpOptions.end(), stripLeadingDashes(conflictStr)) != helpOptions.end()) {
                io->print(REMOTE_CONFLICT_POLICY_HELP);
                conflictStr = io->prompt("Enter on-sync-conflict policy (keep_local/keep_remote/ask) [ask]:", "ask");
            }
            rsync->conflict_policy = rsConflictPolicyFromString(conflictStr);

            sync = rsync;

            s3Vault->encrypt_upstream = io->confirm("Enable upstream encryption? (yes/no) [yes]", false);
        }

        auto interval = io->prompt("Enter sync interval (e.g. 30s, 10m, 1h) [15m] --help for details:", "15m");
        while (std::ranges::find(helpOptions.begin(), helpOptions.end(), stripLeadingDashes(interval)) != helpOptions.end()) {
            io->print(SYNC_INTERVAL_HELP);
            interval = io->prompt("Enter sync interval (e.g. 30s, 10m, 1h) [15m]:", "15m");
        }
        sync->interval = parseSyncInterval(interval);

        return finish_vault_create(call, v, sync);

    } catch (const std::exception& e) {
        return handle_vault_create_failure(v, e.what());
    }
}

CommandResult commands::vault::handle_vault_create(const CommandCall& call) {
    std::shared_ptr<Vault> vault;

    try {
        if (!call.user->canCreateVaults())
            return invalid(
                "vault create: user ID " + std::to_string(call.user->id) +
                " does not have permission to create vaults");

        if (hasFlag(call, "interactive")) return handle_vault_create_interactive(call);
        if (call.positionals.empty()) return invalid("vault create: missing <name>");
        if (call.positionals.size() > 1) return invalid("vault create: too many arguments");

        const std::string name = call.positionals[0];

        const bool f_local = hasFlag(call, "local");
        const bool f_s3 = hasFlag(call, "s3");
        if (f_local && f_s3) return invalid("vault create: --local and --s3 are mutually exclusive");

        const auto descOpt = optVal(call, "desc");
        const auto quotaOpt = optVal(call, "quota");
        const auto ownerOpt = optVal(call, "owner");
        const auto syncStrategyOpt = optVal(call, "sync-strategy");
        const auto onSyncConflictOpt = optVal(call, "on-sync-conflict");

        if (!ownerOpt) return invalid("vault create: invalid owner");

        unsigned int ownerId = call.user->id;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault create: --owner <id> must be a positive integer");
            ownerId = *ownerIdOpt;
        }

        if (VaultQueries::vaultExists(name, ownerId))
            return invalid(
                "vault create: vault with name '" + name + "' already exists for user ID " + std::to_string(ownerId));

        std::shared_ptr<Sync> sync;

        if (f_local) {
            vault = std::make_shared<Vault>();
            const auto fsync = std::make_shared<FSync>();

            if (onSyncConflictOpt) fsync->conflict_policy = fsConflictPolicyFromString(*onSyncConflictOpt);
            else fsync->conflict_policy = FSync::ConflictPolicy::Overwrite; // Default

            sync = fsync;
        } else if (f_s3) {
            const auto s3Vault = std::make_shared<S3Vault>();

            const auto apiKeyOpt = optVal(call, "api-key");
            if (!apiKeyOpt || apiKeyOpt->empty())
                return invalid(
                    "vault create: missing required --api-key <name | id> for S3 vault");

            std::shared_ptr<api::APIKey> apiKey;
            if (const auto apiKeyIdOpt = parseInt(*apiKeyOpt)) apiKey = APIKeyQueries::getAPIKey(*apiKeyIdOpt);
            else apiKey = APIKeyQueries::getAPIKey(*apiKeyOpt);

            if (!apiKey) return invalid("vault create: API key not found: " + *apiKeyOpt);

            if (ownerId != call.user->id) {
                if (!call.user->canManageAPIKeys())
                    return invalid(
                        "vault create: user ID " + std::to_string(call.user->id) +
                        " does not have permission to assign API keys to other users vaults"
                        );

                const auto owner = UserQueries::getUserById(ownerId);

                if (ownerId != apiKey->user_id && !owner->canManageAPIKeys())
                    return invalid("vault create: API key '" + *apiKeyOpt + "' does not belong to user ID " +
                                   std::to_string(ownerId));
            }

            s3Vault->api_key_id = apiKey->id;

            const auto bucketOpt = optVal(call, "bucket");
            if (!bucketOpt || bucketOpt->empty())
                return invalid(
                    "vault create: missing required --bucket <name> for S3 vault");

            s3Vault->bucket = *bucketOpt;

            const auto rsync = std::make_shared<RSync>();
            if (syncStrategyOpt) rsync->strategy = strategyFromString(*syncStrategyOpt);
            else rsync->strategy = RSync::Strategy::Cache;

            if (onSyncConflictOpt) rsync->conflict_policy = rsConflictPolicyFromString(*onSyncConflictOpt);
            else rsync->conflict_policy = RSync::ConflictPolicy::KeepLocal;

            sync = rsync;
            vault = s3Vault;
        }

        vault->name = name;
        vault->description = descOpt ? *descOpt : "";
        vault->owner_id = ownerId;
        vault->type = f_local ? VaultType::Local : VaultType::S3;
        if (quotaOpt) vault->quota = parseSize(*quotaOpt);
        else vault->quota = 0; // 0 means unlimited

        return finish_vault_create(call, vault, sync);

    } catch (const std::exception& e) {
        return handle_vault_create_failure(vault, e.what());
    }
}
