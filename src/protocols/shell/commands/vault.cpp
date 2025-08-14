#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"

#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/DBQueryParams.hpp"
#include "types/APIKey.hpp"
#include "util/fsPath.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;

// ---------- pretty/usage ----------
static CommandResult usage_vault_root() {
    return {
        0,
        "Usage:\n"
        "  Local vault:\n"
        "    vault create <name> --local --mount <path>\n"
        "        [--on-sync-conflict <overwrite | keep_both | ask>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner-id <id>]\n"
        "\n"
        "  S3-backed vault:\n"
        "    vault create <name> --s3 --mount <path>\n"
        "         --api-key <name | id> --bucket <name>\n"
        "        [--sync-strategy <cache | sync | mirror>]\n"
        "        [--on-sync-conflict <keep_local | keep_remote | ask>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner-id <id>]\n"
        "\n"
        "  Update existing vault (backend inferred; all params optional):\n"
        "    vault (update | set) <id>\n"
        "        [--mount <path>] [--api-key <name | id>] [--bucket <name>]\n"
        "        [--sync-strategy <cache | sync | mirror>]\n"
        "        [--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner-id <id>]\n"
        "    vault (update | set) <name> <owner_id>\n"
        "        [--mount <path>] [--api-key <name | id>] [--bucket <name>]\n"
        "        [--sync-strategy <cache | sync | mirror>]\n"
        "        [--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner-id <id>]\n"
        "\n"
        "  Common commands:\n"
        "    vault delete <id>\n"
        "    vault delete <name> --owner-id <id>\n"
        "    vault info   <id>\n"
        "    vault info   <name> [--owner-id <id>]\n",
        ""
    };
}

static CommandResult usage_vaults_list() {
    return {
        0,
        "Usage:\n"
        "  vaults [--local|--s3] [--limit <n>]\n",
        ""
    };
}

static uintmax_t parseSize(const std::string& s) {
    switch (std::toupper(s.back())) {
    case 'T': return std::stoull(s.substr(0, s.size() - 1)) * 1024 * 1024 * 1024 * 1024; // TiB
    case 'G': return std::stoull(s.substr(0, s.size() - 1)) * 1024;                      // GiB
    case 'M': return std::stoull(s.substr(0, s.size() - 1)) * 1024 * 1024;               // MiB
    default: return std::stoull(s);                                                      // Assume bytes if no suffix
    }
}

// ---------- handlers ----------
static CommandResult handle_vault_create(const CommandCall& call) {
    try {
        if (call.positionals.empty()) return invalid("vault create: missing <name>");
        if (call.positionals.size() > 1) return invalid("vault create: too many arguments");

        const std::string name = call.positionals[0];

        const bool f_local = hasFlag(call, "local");
        const bool f_s3 = hasFlag(call, "s3");
        if (f_local && f_s3) return invalid("vault create: --local and --s3 are mutually exclusive");

        const auto mountOpt = optVal(call, "mount");
        const auto descOpt = optVal(call, "desc");
        const auto quotaOpt = optVal(call, "quota");
        const auto ownerIdOpt = optVal(call, "owner-id");
        const auto syncStrategyOpt = optVal(call, "sync-strategy");
        const auto onSyncConflictOpt = optVal(call, "on-sync-conflict");

        if (!mountOpt || mountOpt->empty()) return invalid("vault create: missing required --mount <path>");
        const auto mount = makeAbsolute(*mountOpt);

        if (mount.string().starts_with("/mnt") ||
            mount.string().starts_with(ConfigRegistry::get().fuse.root_mount_path.string()))
            return invalid("vault create: mount path cannot start with /mnt or the configured root mount path");

        if (FSEntryQueries::exists(mount)) return invalid("vault create: mount path already exists: " + mount.string());

        unsigned int ownerId = call.user->id;
        if (ownerIdOpt) {
            const auto parsed = parseInt(*ownerIdOpt);
            if (parsed && *parsed > 0) ownerId = *parsed;
        }

        if (VaultQueries::vaultExists(name, ownerId)) return invalid(
            "vault create: vault with name '" + name + "' already exists for user ID " + std::to_string(ownerId));

        std::shared_ptr<Vault> vault;
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
            if (!apiKeyOpt || apiKeyOpt->empty()) return invalid(
                "vault create: missing required --api-key <name | id> for S3 vault");

            std::shared_ptr<api::APIKey> apiKey;
            const auto apiKeyIdOpt = parseInt(*apiKeyOpt);
            if (apiKeyIdOpt) apiKey = APIKeyQueries::getAPIKey(*apiKeyIdOpt);
            else apiKey = APIKeyQueries::getAPIKey(*apiKeyOpt);

            if (!apiKey) return invalid("vault create: API key not found: " + *apiKeyOpt);

            if (ownerId != call.user->id) {
                if (!call.user->canAccessAnyAPIKey())
                    return invalid(
                        "vault create: user ID " + std::to_string(call.user->id) +
                        " does not have permission to assign API keys to other users vaults"
                        );

                const auto owner = UserQueries::getUserById(ownerId);

                if (ownerId != apiKey->user_id && !owner->canAccessAnyAPIKey())
                    return invalid("vault create: API key '" + *apiKeyOpt + "' does not belong to user ID " +
                                   std::to_string(ownerId));
            }

            s3Vault->api_key_id = apiKey->id;

            const auto bucketOpt = optVal(call, "bucket");
            if (!bucketOpt || bucketOpt->empty()) return invalid(
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
        vault->mount_point = mount;
        vault->description = descOpt ? *descOpt : "";
        vault->owner_id = ownerId;
        vault->type = f_local ? VaultType::Local : VaultType::S3;
        if (quotaOpt) vault->quota = parseSize(*quotaOpt);
        else vault->quota = 0; // 0 means unlimited

        vault = ServiceDepsRegistry::instance().storageManager->addVault(vault, sync);

        return ok("Successfully created new vault!\n" + to_string(vault));
    } catch (const std::exception& e) {
        return invalid("vault create: " + std::string(e.what()));
    }
}

static CommandResult handle_vault_delete(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault delete: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault delete: too many arguments");

    const std::string name = call.positionals[0];
    const auto idOpt = parseInt(name);

    std::shared_ptr<Vault> vault;

    if (idOpt) vault = VaultQueries::getVault(*idOpt);
    else {
        const auto ownerIdOpt = optVal(call, "owner-id");
        if (!ownerIdOpt) return invalid("vault delete: missing required --owner-id <id> for vault name deletion");
        const auto ownerId = parseInt(*ownerIdOpt);
        if (!ownerId || *ownerId <= 0) return invalid("vault delete: --owner-id must be a positive integer");

        vault = VaultQueries::getVault(name, *ownerId);
    }

    if (!vault) return invalid("vault delete: vault with ID " + std::to_string(*idOpt) + " not found");
    if (!call.user->canManageVaults() && vault->owner_id != call.user->id)
        return invalid("vault delete: you do not have permission to delete this vault");

    ServiceDepsRegistry::instance().storageManager->removeVault(*idOpt);

    return ok("Successfully deleted vault '" + vault->name + "' (ID: " + std::to_string(vault->id) + ")\n");
}

static CommandResult handle_vault_info(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault info: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault info: too many arguments");

    const std::string name = call.positionals[0];
    const auto idOpt = parseInt(name);

    std::shared_ptr<Vault> vault;
    if (idOpt) vault = VaultQueries::getVault(*idOpt);
    else {
        const auto ownerIdOpt = optVal(call, "owner-id");
        if (!ownerIdOpt) vault = VaultQueries::getVault(name, call.user->id);
        const auto ownerId = parseInt(*ownerIdOpt);
        vault = VaultQueries::getVault(name, *ownerId);
    }

    if (!vault) return invalid("vault info: vault with arg '" + name + "' not found");

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id)
        return invalid("vault info: you do not have permission to view this vault");

    return ok(to_string(vault));
}

static CommandResult handle_vaults_list(const CommandCall& call) {
    // Validate options
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

    const auto vaults = listAll
                            ? VaultQueries::listVaults(std::move(p))
                            : VaultQueries::listUserVaults(user->id, std::move(p));

    return ok(to_string(vaults));
}

static CommandResult handle_vault_update(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault update: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("vault update: too many arguments");

    const auto nameOrId = call.positionals[0];
    const auto idOpt = parseInt(nameOrId);

    std::shared_ptr<Vault> vault;
    if (idOpt) vault = VaultQueries::getVault(*idOpt);
    else {
        if (call.positionals.size() < 2)
            return invalid("vault update: missing required <owner-id> for vault name update");

        const auto ownerId = std::stoi(call.positionals[1]);
        vault = VaultQueries::getVault(nameOrId, ownerId);
    }

    if (!vault) return invalid("vault update: vault with ID " + std::to_string(*idOpt) + " not found");

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id)
        return invalid("vault update: you do not have permission to update this vault");

    const auto mountOpt = optVal(call, "mount");
    const auto descOpt = optVal(call, "desc");
    const auto quotaOpt = optVal(call, "quota");
    const auto ownerIdOpt = optVal(call, "owner-id");
    const auto syncStrategyOpt = optVal(call, "sync-strategy");
    const auto onSyncConflictOpt = optVal(call, "on-sync-conflict");
    const auto apiKeyOpt = optVal(call, "api-key");
    const auto bucketOpt = optVal(call, "bucket");

    if (vault->type == VaultType::Local && (apiKeyOpt || bucketOpt))
        return invalid("vault update: --api-key and --bucket are not valid for local vaults");

    if (mountOpt) {
        // TODO: handle mount migration
    }

    if (descOpt) vault->description = *descOpt;

    if (quotaOpt) {
        if (*quotaOpt == "unlimited") vault->quota = 0; // 0 means unlimited
        else vault->quota = parseSize(*quotaOpt);
    }

    if (ownerIdOpt) {
        const auto ownerId = parseInt(*ownerIdOpt);
        if (!ownerId || *ownerId <= 0) return invalid("vault update: --owner-id must be a positive integer");
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
        if (vault->type == VaultType::Local)
            return invalid("vault update: --api-key and --bucket are not valid for local vaults");

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

    VaultQueries::upsertVault(vault, sync);

    return ok("Successfully updated vault!\n" + to_string(vault));
}

void vh::shell::registerVaultCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("vault", "Manage a single vault",
                       [](const CommandCall& call) -> CommandResult {
                           if (call.positionals.empty()) return usage_vault_root();

                           const std::string_view sub = call.positionals[0];
                           CommandCall subcall = call;
                           // shift subcommand off positionals
                           subcall.positionals.erase(subcall.positionals.begin());

                           if (sub == "create" || sub == "new") return handle_vault_create(subcall);
                           if (sub == "delete" || sub == "rm") return handle_vault_delete(subcall);
                           if (sub == "info" || sub == "get") return handle_vault_info(subcall);
                           if (sub == "update" || sub == "set") return handle_vault_update(subcall);

                           return invalid(
                               "vault: unknown subcommand '" + std::string(sub) + "'. Use: create | delete | info");
                       }, {"v"});

    r->registerCommand("vaults", "List vaults",
                       [](const CommandCall& call) -> CommandResult {
                           if (hasKey(call, "help") || hasKey(call, "h")) return usage_vaults_list();
                           return handle_vaults_list(call);
                       }, {"ls"});
}