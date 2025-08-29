#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "database/Queries/VaultKeyQueries.hpp"
#include "database/Queries/WaiverQueries.hpp"

#include "services/LogRegistry.hpp"
#include "services/SyncController.hpp"

#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/cloud/s3/S3Controller.hpp"

#include "crypto/VaultEncryptionManager.hpp"
#include "crypto/GPGEncryptor.hpp"
#include "crypto/APIKeyManager.hpp"

#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/DBQueryParams.hpp"
#include "types/APIKey.hpp"
#include "types/Group.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "types/VaultRole.hpp"
#include "types/User.hpp"
#include "types/Waiver.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"

#include "config/ConfigRegistry.hpp"
#include "util/interval.hpp"
#include "VaultUsage.hpp"
#include "util/waiver.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <utility>

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


// ################################################################################
// #################### 🧱 Vault Lifecycle (create/delete) ########################
// ################################################################################

static std::shared_ptr<Waiver> create_encrypt_waiver(const CommandCall& call, const std::shared_ptr<S3Vault>& s3Vault) {
    auto waiver = std::make_shared<Waiver>();
    waiver->vault = s3Vault;
    waiver->user = call.user;
    waiver->apiKey = APIKeyQueries::getAPIKey(s3Vault->api_key_id);
    waiver->encrypt_upstream = s3Vault->encrypt_upstream;
    waiver->waiver_text = s3Vault->encrypt_upstream ?
        ENABLE_UPSTREAM_ENCRYPTION_WAIVER : DISABLE_UPSTREAM_ENCRYPTION_WAIVER;

    if (s3Vault->owner_id != call.user->id) {
        waiver->owner = UserQueries::getUserById(s3Vault->owner_id);
        if (!waiver->owner) throw std::runtime_error("Failed to load owner ID " + std::to_string(s3Vault->owner_id));

        if (waiver->owner->canManageVaults()) waiver->overridingRole = std::make_shared<Role>(*call.user->role);
        else {
            const auto role = call.user->getRole(s3Vault->id);
            if (!role || role->type != "vault") throw std::runtime_error(
                "User ID " + std::to_string(call.user->id) + " does not have a role for vault ID " + std::to_string(s3Vault->id));

            // validate role has vault management perms
            if (!role->canManageVault({})) throw std::runtime_error(
                "User ID " + std::to_string(call.user->id) + " does not have permission to manage vault ID " + std::to_string(s3Vault->id));

            waiver->overridingRole = std::make_shared<Role>(*role);
        }
    }

    return waiver;
}

static bool upstream_bucket_is_empty(const std::shared_ptr<S3Vault>& s3Vault) {
    const auto apiKey = ServiceDepsRegistry::instance().apiKeyManager->getAPIKey(s3Vault->api_key_id, s3Vault->owner_id);
    if (!apiKey) throw std::runtime_error("Failed to load API key ID " + std::to_string(s3Vault->api_key_id));
    if (apiKey->secret_access_key.empty()) throw std::runtime_error("API key ID " + std::to_string(s3Vault->api_key_id) + " has no secret access key");
    const S3Controller ctrl(apiKey, s3Vault->bucket);

    if (const auto [ok, msg] = ctrl.validateAPICredentials(); !ok)
        throw std::runtime_error("Failed to validate S3 credentials: " + msg);

    return ctrl.isBucketEmpty();
}

static bool requires_waiver(const CommandCall& call, const std::shared_ptr<S3Vault>& s3Vault, const bool isUpdate = false) {
    const bool hasEncryptFlag = hasFlag(call, "encrypt");
    const bool hasNoEncryptFlag = hasFlag(call, "no-encrypt");

    if (hasEncryptFlag && hasNoEncryptFlag)
        throw std::runtime_error("Cannot use --encrypt and --no-encrypt together");

    if (!hasEncryptFlag && !hasNoEncryptFlag) {
        if (isUpdate) return false;
        s3Vault->encrypt_upstream = true;
        return !upstream_bucket_is_empty(s3Vault);
    }

    if (hasEncryptFlag) {
        s3Vault->encrypt_upstream = true;
        if (isUpdate && s3Vault->encrypt_upstream) return false;
        if (hasFlag(call, "accept-overwrite-waiver")) return false;
        return !upstream_bucket_is_empty(s3Vault);
    }

    // hasNoEncryptFlag
    s3Vault->encrypt_upstream = false;
    if (isUpdate && !s3Vault->encrypt_upstream) return false;
    if (hasFlag(call, "accept_decryption_waiver")) return false;
    return !upstream_bucket_is_empty(s3Vault);
}

struct WaiverResult {
    bool okToProceed;
    std::shared_ptr<Waiver> waiver;
};

static WaiverResult handle_encryption_waiver(const CommandCall& call, const std::shared_ptr<Vault>& vault, const bool isUpdate = false) {
    if (!vault) throw std::invalid_argument("Invalid vault");
    if (vault->type != VaultType::S3) return {true, nullptr};

    const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
    if (!s3Vault) throw std::runtime_error("Failed to cast vault to S3Vault");

    if (!requires_waiver(call, s3Vault, isUpdate)) return {true, nullptr};

    const auto waiverText = s3Vault->encrypt_upstream ?
        ENABLE_UPSTREAM_ENCRYPTION_WAIVER : DISABLE_UPSTREAM_ENCRYPTION_WAIVER;

    if (const auto res = call.io->prompt(waiverText, "I DO NOT ACCEPT"); res == "I ACCEPT") {
        const auto waiver = create_encrypt_waiver(call, s3Vault);
        if (!waiver) throw std::runtime_error("Failed to create encryption waiver");
        return {true, waiver};
    }

    return {false, nullptr};
}

static CommandResult finish_vault_create(const CommandCall& call, std::shared_ptr<Vault>& v, const std::shared_ptr<Sync>& s) {
    const auto [okToProceed, waiver] = handle_encryption_waiver(call, v, false);
    if (!okToProceed) return invalid("vault create: user did not accept encryption waiver");

    v = ServiceDepsRegistry::instance().storageManager->addVault(v, s);
    if (waiver) WaiverQueries::addWaiver(waiver);

    return ok("\nSuccessfully created new vault!\n" + to_string(v));
}

static CommandResult handle_vault_create_failure(const CommandCall& call, const std::shared_ptr<Vault>& v, const std::string& err) {
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
                io->print(R"(
On-Sync-Conflict Policy Options:
  overwrite  - In case of conflict, overwrite the remote with the local version.
  keep_both  - In case of conflict, keep both versions by renaming the remote.
  ask        - Prompt the user to resolve conflicts during sync operations.
)");
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
                io->print(R"(
Sync Strategy Options:
  cache  - Local cache of S3 bucket. Changes are uploaded to S3 on demand.
           Downloads are served from cache if available, otherwise fetched from S3.
  sync   - Two-way sync between local and S3. Changes in either location are propagated
           to the other during sync operations.
  mirror - One-way mirror of local to S3. Local changes are uploaded to S3,
           but changes in S3 are not downloaded locally.

)");
                strategyStr = io->prompt("Enter sync strategy (cache/sync/mirror) [cache]:", "cache");
            }
            const auto rsync = std::make_shared<RSync>();
            rsync->strategy = strategyFromString(strategyStr);

            auto conflictStr = io->prompt("Enter on-sync-conflict policy (keep_local/keep_remote/ask) [ask] --help for details:", "ask");
            while (std::ranges::find(helpOptions.begin(), helpOptions.end(), stripLeadingDashes(conflictStr)) != helpOptions.end()) {
                io->print(R"(
On-Sync-Conflict Policy Options:
  keep_local  - In case of conflict, keep the local version and overwrite the remote.
  keep_remote - In case of conflict, keep the remote version and overwrite the local.
  ask         - Prompt the user to resolve conflicts during sync operations.

)");
                conflictStr = io->prompt("Enter on-sync-conflict policy (keep_local/keep_remote/ask) [ask]:", "ask");
            }
            rsync->conflict_policy = rsConflictPolicyFromString(conflictStr);

            sync = rsync;

            s3Vault->encrypt_upstream = io->confirm("Enable upstream encryption? (yes/no) [yes]", false);
        }

        auto interval = io->prompt("Enter sync interval (e.g. 30s, 10m, 1h) [15m] --help for details:", "15m");
        while (std::ranges::find(helpOptions.begin(), helpOptions.end(), stripLeadingDashes(interval)) != helpOptions.end()) {
            io->print(R"(
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
)");
            interval = io->prompt("Enter sync interval (e.g. 30s, 10m, 1h) [15m]:", "15m");
        }
        sync->interval = parseSyncInterval(interval);

        return finish_vault_create(call, v, sync);

    } catch (const std::exception& e) {
        return handle_vault_create_failure(call, v, e.what());
    }
}

static CommandResult handle_vault_create(const CommandCall& call) {
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
            const auto apiKeyIdOpt = parseInt(*apiKeyOpt);
            if (apiKeyIdOpt) apiKey = APIKeyQueries::getAPIKey(*apiKeyIdOpt);
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
        return handle_vault_create_failure(call, vault, e.what());
    }
}


static CommandResult handle_vault_update(const CommandCall& call) {
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

    const auto [okToProceed, waiver] = handle_encryption_waiver(call, vault, true);
    if (!okToProceed) return invalid("vault create: user did not accept encryption waiver");
    if (waiver) WaiverQueries::addWaiver(waiver);

    VaultQueries::upsertVault(vault, sync);

    return ok("Successfully updated vault!\n" + to_string(vault));
}


static CommandResult handle_vault_delete(const CommandCall& call) {
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


// ################################################################################
// #################### 📦 Vault Info and Listing Commands ########################
// ################################################################################

static CommandResult handle_vault_info(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault info: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault info: too many arguments");

    const std::string name = call.positionals[0];
    const auto idOpt = parseInt(name);

    std::shared_ptr<Vault> vault;
    if (idOpt) vault = VaultQueries::getVault(*idOpt);
    else {
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault info: missing required --owner <id | name> for vault name lookup");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) vault = VaultQueries::getVault(name, *ownerIdOpt);
        else vault = VaultQueries::getVault(name, call.user->id);
    }

    if (!vault) return invalid("vault info: vault with arg '" + name + "' not found");

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id) return invalid(
        "vault info: you do not have permission to view this vault");

    if (!call.user->isAdmin() && !call.user->canListVaultData(vault->id)) return invalid(
        "vault info: you do not have permission to view this vault's data");

    return ok(to_string(vault));
}


static CommandResult handle_vaults_list(const CommandCall& call) {
    const bool f_local = hasFlag(call, "local");
    const bool f_s3 = hasFlag(call, "s3");
    if (f_local && f_s3) return invalid("vaults: --local and --s3 are mutually exclusive");
    if (!call.positionals.empty()) return invalid("vaults: unexpected positional arguments");

    int limit = 100;
    if (auto lim = optVal(call, "limit")) {
        if (lim->empty()) return invalid("vaults: --limit requires a value");
        auto parsed = parseInt(*lim);
        if (!parsed || *parsed <= 0) return invalid("vaults: --limit must be a positive integer");
        limit = *parsed;
    }

    const auto user = call.user;
    const bool listAll = user->isAdmin() || user->canManageVaults();

    DBQueryParams p{
        .sortBy = "created_at",
        .order = SortOrder::DESC,
        .limit = limit,
        .offset = 0, // TODO: handle pagination if needed
    };

    auto vaults = listAll
                      ? VaultQueries::listVaults(std::move(p))
                      : VaultQueries::listUserVaults(user->id, std::move(p));

    if (!listAll) {
        for (const auto& r : call.user->roles) {
            if (r->canList({})) {
                const auto vault = ServiceDepsRegistry::instance().storageManager->getEngine(r->vault_id)->vault;
                if (!vault) continue;
                if (vault->owner_id == user->id) continue; // Already added
                vaults.push_back(vault);
            }
        }
    }

    return ok(to_string(vaults));
}


// ################################################################################
// #################### 🔑 Vault Role Assignment Commands ########################
// ################################################################################

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


static CommandResult handle_vault_role_override(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault override: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault override: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);
    const auto roleIdOpt = parseInt(roleArg);

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

    std::shared_ptr<Role> role;
}


static CommandResult handle_vault_role(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault role: missing subcommand");
    if (call.positionals.size() > 1) return invalid("vault role: too many arguments");

    const auto subcommand = call.positionals[0];
    auto subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (subcommand == "assign") return handle_vault_role_assign(subcall);
    if (subcommand == "override") return handle_vault_role_override(subcall);
    return invalid("vault role: unknown subcommand: " + subcommand);
}


// ################################################################################
// #################### 🔑 Vault Key Management Commands ########################
// ################################################################################

static CommandResult handle_key_encrypt_and_response(const CommandCall& call,
                                                     const nlohmann::json& output) {
    const auto outputOpt = optVal(call, "output");

    if (const auto recipientOpt = optVal(call, "recipient")) {
        if (recipientOpt->empty()) return invalid("vault keys export: --recipient requires a value");
        if (!outputOpt) return invalid("vault keys export: --recipient requires --output to specify the output file");

        try {
            GPGEncryptor::encryptToFile(output, *recipientOpt, *outputOpt);
            return ok("Vault key successfully encrypted and saved to " + *outputOpt);
        } catch (const std::exception& e) {
            return invalid("vault keys export: failed to encrypt vault key: " + std::string(e.what()));
        }
    }

    if (outputOpt) {
        LogRegistry::audit()->warn(
            "[shell::handle_key_encrypt_and_response] No recipient specified, saving unencrypted key(s) to " + *
            outputOpt);
        try {
            std::ofstream outFile(*outputOpt);
            if (!outFile) return invalid("vault keys export: failed to open output file " + *outputOpt);
            outFile << output.dump(4);
            outFile.close();
            return {0, "Vault key(s) successfully saved to " + *outputOpt,
                    "\nWARNING: No recipient specified, key(s) are unencrypted.\n"
                    "\nConsider using --recipient with a GPG fingerprint to encrypt the key(s) before saving."};
        } catch (const std::exception& e) {
            return invalid("vault keys export: failed to write to output file: " + std::string(e.what()));
        }
    }

    LogRegistry::audit()->warn(
        "[shell::handle_key_encrypt_and_response] No recipient specified, returning unencrypted key(s)");
    return {0, output.dump(4),
            "\nWARNING: No recipient specified, key(s) are unencrypted.\n"
            "\nConsider using --recipient with a GPG fingerprint along with --output\nto securely encrypt the key(s) to an output file."};
}


static CommandResult export_one_key(const CommandCall& call) {
    const auto vaultArg = call.positionals[0];
    std::shared_ptr<StorageEngine> engine;

    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) return invalid("vault keys export: vault ID must be a positive integer");
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(*vaultIdOpt);
    } else {
        const auto ownerOpt = optVal(call, "owner");
        unsigned int ownerId;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault assign: --owner <id> must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault assign: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault assign: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        const auto vault = VaultQueries::getVault(vaultArg, ownerId);
        if (!vault) return invalid(
            "vault keys export: vault with name '" + vaultArg + "' not found for user ID " + std::to_string(ownerId));
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    }

    if (!engine) return invalid("vault keys export: no storage engine found for vault '" + vaultArg + "'");
    const auto context = fmt::format("User: {} -> {}", call.user->name, __func__);
    const auto& key = engine->encryptionManager->get_key(context);
    const auto vaultKey = VaultKeyQueries::getVaultKey(engine->vault->id);

    const auto out = api::generate_json_key_object(engine->vault, key, vaultKey, call.user->name);
    return handle_key_encrypt_and_response(call, out);
}


static CommandResult export_all_keys(const CommandCall& call) {
    const auto engines = ServiceDepsRegistry::instance().storageManager->getEngines();
    if (engines.empty()) return invalid("vault keys export: no vaults found");

    nlohmann::json out = nlohmann::json::array();

    const auto context = fmt::format("User: {} -> {}", call.user->name, __func__);
    for (const auto& engine : engines) {
        const auto& key = engine->encryptionManager->get_key(context);
        const auto vaultKey = VaultKeyQueries::getVaultKey(engine->vault->id);
        out.push_back(api::generate_json_key_object(engine->vault, key, vaultKey, call.user->name));
    }

    return handle_key_encrypt_and_response(call, out);
}


static CommandResult handle_export_vault_keys(const CommandCall& call) {
    if (!call.user->isSuperAdmin()) {
        if (!call.user->canManageEncryptionKeys()) return invalid(
            "vault keys export: only super admins can export vault keys");

        LogRegistry::audit()->warn(
            "\n[shell::handle_export_vault_keys] User {} called to export vault keys without super admin privileges\n"
            "WARNING: It is extremely dangerous to assign this permission to non super-admin users, proceed at your own risk.\n",
            call.user->name);
    }
    if (call.positionals.empty()) return invalid("vault keys export: missing <vault_id | name | all> <output_file>");
    if (call.positionals.size() > 2) return invalid("vault keys export: too many arguments");
    if (call.positionals[0] == "all") return export_all_keys(call);
    return export_one_key(call);
}


static CommandResult handle_inspect_vault_key(const CommandCall& call) {
    if (!call.user->isSuperAdmin()) {
        if (!call.user->canManageEncryptionKeys()) return invalid(
            "vault keys inspect: only super admins can inspect vault keys");

        LogRegistry::audit()->warn(
            "\n[shell::handle_inspect_vault_key] User {} called to inspect vault keys without super admin privileges\n"
            "WARNING: It is extremely dangerous to assign this permission to non super-admin users, proceed at your own risk.\n",
            call.user->name);
    }

    if (call.positionals.size() != 1) return invalid(
        "vault keys inspect: expected exactly one argument <vault_id | name>");

    const auto vaultArg = call.positionals[0];
    if (vaultArg.empty()) return invalid("vault keys inspect: missing <vault_id | name>");

    std::shared_ptr<StorageEngine> engine;

    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) return invalid("vault keys inspect: vault ID must be a positive integer");
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(*vaultIdOpt);
    } else {
        const auto ownerOpt = optVal(call, "owner");
        unsigned int ownerId;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault keys inspect: --owner <id> must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault keys inspect: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault keys inspect: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        const auto vault = VaultQueries::getVault(vaultArg, ownerId);
        if (!vault) return invalid(
            "vault keys inspect: vault with name '" + vaultArg + "' not found for user ID " + std::to_string(ownerId));
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    }

    if (!engine) return invalid("vault keys inspect: no storage engine found for vault '" + vaultArg + "'");

    const auto vaultKey = VaultKeyQueries::getVaultKey(engine->vault->id);

    return ok(api::generate_json_key_info_object(engine->vault, vaultKey, call.user->name).dump(4));
}


static CommandResult handle_rotate_vault_keys(const CommandCall& call) {
    if (!call.user->isSuperAdmin()) {
        if (!call.user->canManageEncryptionKeys()) return invalid(
            "vault keys rotate: only super admins or users with manage encryption keys or vaults can rotate vault keys");

        LogRegistry::audit()->warn(
            "\n[shell::handle_rotate_vault_keys] User {} called to rotate vault keys without super admin privileges\n"
            "WARNING: It is extremely dangerous to assign this permission to non super-admin users, proceed at your own risk.\n",
            call.user->name);
    }

    if (call.positionals.size() != 1) return invalid(
        "vault keys rotate: expected exactly one argument <vault_id | name>");

    const auto vaultArg = call.positionals[0];
    if (vaultArg.empty()) return invalid("vault keys rotate: missing <vault_id | name>");

    const bool syncNow = hasFlag(call, "now");

    if (vaultArg == "all") {
        for (const auto& engine : ServiceDepsRegistry::instance().storageManager->getEngines()) {
            engine->encryptionManager->prepare_key_rotation();
            if (syncNow) ServiceDepsRegistry::instance().syncController->runNow(engine->vault->id);
        }

        return ok("Vault keys for all vaults have been rotated successfully.\n"
            "If you have --now flag set, the sync will be triggered immediately.");
    }

    std::shared_ptr<StorageEngine> engine;

    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) return invalid("vault keys rotate: vault ID must be a positive integer");
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(*vaultIdOpt);
    } else {
        const auto ownerOpt = optVal(call, "owner");
        unsigned int ownerId;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault keys rotate: --owner <id> must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault keys rotate: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault keys rotate: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        const auto vault = VaultQueries::getVault(vaultArg, ownerId);
        if (!vault) return invalid(
            "vault keys rotate: vault with name '" + vaultArg + "' not found for user ID " + std::to_string(ownerId));
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    }

    if (!engine) return invalid("vault keys rotate: no storage engine found for vault '" + vaultArg + "'");

    engine->encryptionManager->prepare_key_rotation();
    if (syncNow) ServiceDepsRegistry::instance().syncController->runNow(engine->vault->id);

    return ok("Vault key for '" + engine->vault->name + "' (ID: " + std::to_string(engine->vault->id) +
              ") has been rotated successfully.\n"
              "If you have --now flag set, the sync will be triggered immediately.");
}


static CommandResult handle_vault_keys(const CommandCall& call) {
    if (call.positionals.empty()) return invalid(
        "vault keys: missing <export | rotate | inspect> <vault_id | name | all> [--recipient <fingerprint>] [--output <file>] [--owner <id | name>]");

    const auto subcommand = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (subcommand == "export") return handle_export_vault_keys(subcall);
    if (subcommand == "rotate") return handle_rotate_vault_keys(subcall);
    if (subcommand == "inspect") return handle_inspect_vault_key(subcall);

    return invalid("vault keys: unknown subcommand '" + std::string(subcommand) + "'. Use: export | rotate | inspect");
}


// ################################################################################
// ######################### 🔄 Vault Sync Commands ###############################
// ################################################################################

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
        unsigned int ownerId = call.user->id;
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

static CommandResult handle_sync(const CommandCall& call) {
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


// ################################################################################
// ######################### 📦 Vault Command Registration ########################
// ################################################################################

static CommandResult handle_vault(const CommandCall& call) {
    if (call.positionals.empty() || hasKey(call, "help") || hasKey(call, "h")) return ok(VaultUsage::all().str());

    const std::string_view sub = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (sub == "sync") return handle_sync(subcall);
    if (sub == "create" || sub == "new") return handle_vault_create(subcall);
    if (sub == "delete" || sub == "rm") return handle_vault_delete(subcall);
    if (sub == "info" || sub == "get") return handle_vault_info(subcall);
    if (sub == "update" || sub == "set") return handle_vault_update(subcall);
    if (sub == "role" || sub == "r") return handle_vault_role(subcall);
    if (sub == "keys") return handle_vault_keys(subcall);

    return ok(VaultUsage::all().str());
}

static CommandResult handle_vaults(const CommandCall& call) {
    if (hasKey(call, "help") || hasKey(call, "h")) return ok(VaultUsage::vaults_list().str());
    return handle_vaults_list(call);
}

void vh::shell::registerVaultCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand(VaultUsage::vault(), handle_vault);
    r->registerCommand(VaultUsage::vaults_list(), handle_vaults);
}
