#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"

#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/DBQueryParams.hpp"
#include "types/APIKey.hpp"
#include "types/Group.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "types/VaultRole.hpp"
#include "storage/StorageEngine.hpp"
#include "keys/VaultEncryptionManager.hpp"
#include "crypto/encrypt.hpp"
#include "keys/GPGEncryptor.hpp"
#include "logging/LogRegistry.hpp"

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
using namespace vh::crypto;
using namespace vh::util;
using namespace vh::logging;

static CommandResult usage_vault_root(const bool isSuperAdmin = false) {
    const std::string encryption_section =
        "  === Encryption Key Management ===\n"
        "    vault keys export  <vault_id | name | all>  [--recipient <fingerprint>] [--output <file>] [--owner <id | name>]\n"
        "    vault keys rotate  <vault_id | name | all>  [--owner <id | name>]\n"
        "    vault keys inspect <vault_id | name>        [--owner <id | name>]\n";

    const std::string general_usage_section =
        "Usage:\n"
        "\n"
        "  === Vault Creation ===\n"
        "    vault create <name> --local\n"
        "        [--on-sync-conflict <overwrite | keep_both | ask>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner <id | name>]\n"
        "\n"
        "    vault create <name> --s3\n"
        "        --api-key <name | id> --bucket <name>\n"
        "        [--sync-strategy <cache | sync | mirror>]\n"
        "        [--on-sync-conflict <keep_local | keep_remote | ask>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner <id | name>]\n"
        "\n"
        "  === Vault Update ===\n"
        "    vault (update | set) <id>\n"
        "        [--api-key <name | id>] [--bucket <name>]\n"
        "        [--sync-strategy <cache | sync | mirror>]\n"
        "        [--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner <id | name>]\n"
        "\n"
        "    vault (update | set) <name> <owner>\n"
        "        [--api-key <name | id>] [--bucket <name>]\n"
        "        [--sync-strategy <cache | sync | mirror>]\n"
        "        [--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>]\n"
        "        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]\n"
        "        [--owner <id | name>]\n"
        "\n"
        "  === Common Commands ===\n"
        "    vault delete <id>\n"
        "    vault delete <name> --owner <id | name>\n"
        "    vault info   <id>\n"
        "    vault info   <name> [--owner <id | name>]\n"
        "    vault assign <id> <role_id> --[uid | gid | user | group] <id | name>\n"
        "    vault assign <name> <role_id> --[uid | gid | user | group] <id | name> --owner <id | name>\n"
        "\n"
        "  Use 'vh vault <command> --help' for more information on any subcommand.\n";

    if (isSuperAdmin)
        return {0, general_usage_section + "\n" + encryption_section, ""};
    return {0, general_usage_section, ""};
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

static CommandResult handle_vault_create(const CommandCall& call) {
    try {
        if (call.positionals.empty()) return invalid("vault create: missing <name>");
        if (call.positionals.size() > 1) return invalid("vault create: too many arguments");

        const std::string name = call.positionals[0];

        const bool f_local = hasFlag(call, "local");
        const bool f_s3 = hasFlag(call, "s3");
        if (f_local && f_s3) return invalid("vault create: --local and --s3 are mutually exclusive");

        const auto descOpt = optVal(call, "desc");
        const auto quotaOpt = optVal(call, "quota");
        const auto ownerIdOpt = optVal(call, "owner-id");
        const auto syncStrategyOpt = optVal(call, "sync-strategy");
        const auto onSyncConflictOpt = optVal(call, "on-sync-conflict");

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
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault delete: missing required --owner <id | name> for vault name lookup");

        if (const auto ownerIdOpt = parseInt(*ownerOpt)) vault = VaultQueries::getVault(name, *ownerIdOpt);
        else if (const auto owner = UserQueries::getUserByName(*ownerOpt))
            vault = VaultQueries::getVault(name, call.user->id);
        else return invalid("vault delete: owner '" + *ownerOpt + "' not found");
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
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault info: missing required --owner <id | name> for vault name lookup");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) vault = VaultQueries::getVault(name, *ownerIdOpt);
        else vault = VaultQueries::getVault(name, call.user->id);
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

    const auto descOpt = optVal(call, "desc");
    const auto quotaOpt = optVal(call, "quota");
    const auto ownerIdOpt = optVal(call, "owner-id");
    const auto syncStrategyOpt = optVal(call, "sync-strategy");
    const auto onSyncConflictOpt = optVal(call, "on-sync-conflict");
    const auto apiKeyOpt = optVal(call, "api-key");
    const auto bucketOpt = optVal(call, "bucket");

    if (vault->type == VaultType::Local && (apiKeyOpt || bucketOpt))
        return invalid("vault update: --api-key and --bucket are not valid for local vaults");

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
            if (*ownerIdOpt <= 0) return invalid("vault assign: --owner-id must be a positive integer");
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
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id))
            return invalid("vault assign: you do not have permission to assign roles to this vault");

        if (!call.user->canManageRoles())
            return invalid("vault assign: you do not have permission to manage roles");
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

static nlohmann::json generate_json_key_object(const std::shared_ptr<Vault>& v,
                                               const std::vector<uint8_t>& key,
                                               const std::string& exportedBy) {
    return {
            {"vault_id", v->id},
            {"vault_name", v->name},
            {"key", b64_encode(key)},
            {"key_info",
                {
                    {"type", "AES-256-GCM"},
                    {"key_size", key.size()},
                    {"iv_size", 12},
                    {"tag_size", 16},
                    {"iv_gen", "libsodium::randombytes_buf"},
                    {"cipher_impl", "libsodium::crypto_aead_aes256gcm"},
                    {"hardware_accel", "AES-NI (runtime checked)"},
                    {"aad", false},
                    {"files_hashed_with", "libsodium"},
                    {"sealed_by", "Vaulthalla v0.9.0"},
                    {"exported_by", exportedBy},
                    {"exported_at", timestampToString(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()))}
                }
            }
    };
}

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
        LogRegistry::audit()->warn("[shell::handle_key_encrypt_and_response] No recipient specified, saving unencrypted key(s) to " + *outputOpt);
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

    LogRegistry::audit()->warn("[shell::handle_key_encrypt_and_response] No recipient specified, returning unencrypted key(s)");
    return {0, output.dump(4),
        "\nWARNING: No recipient specified, key(s) are unencrypted.\n"
        "\nConsider using --recipient with a GPG fingerprint along with --output\nto securely encrypt the key(s) to an output file."};
}

static CommandResult handle_export_vault_key(const CommandCall& call) {
    const auto vaultArg = call.positionals[0];
    std::shared_ptr<StorageEngine> engine;

    if (const auto vaultIdOpt = parseInt(vaultArg)) {
        if (*vaultIdOpt <= 0) return invalid("vault keys export: vault ID must be a positive integer");
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(*vaultIdOpt);
    } else {
        const auto ownerOpt = optVal(call, "owner");
        unsigned int ownerId;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault assign: --owner-id must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault assign: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault assign: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        const auto vault = VaultQueries::getVault(vaultArg, ownerId);
        if (!vault) return invalid("vault keys export: vault with name '" + vaultArg + "' not found for user ID " + std::to_string(ownerId));
        engine = ServiceDepsRegistry::instance().storageManager->getEngine(vault->id);
    }

    if (!engine) return invalid("vault keys export: no storage engine found for vault '" + vaultArg + "'");
    const auto context = fmt::format("User: {} -> {}", call.user->name, __func__);
    const auto& key = engine->encryptionManager->get_key(context);

    const auto out = generate_json_key_object(engine->vault, key, call.user->name);
    return handle_key_encrypt_and_response(call, out);
}

static CommandResult handle_export_all_vault_keys(const CommandCall& call) {
    const auto engines = ServiceDepsRegistry::instance().storageManager->getEngines();
    if (engines.empty()) return invalid("vault keys export: no vaults found");

    nlohmann::json out = nlohmann::json::array();

    const auto context = fmt::format("User: {} -> {}", call.user->name, __func__);
    for (const auto& engine : engines) {
        const auto& key = engine->encryptionManager->get_key(context);
        out.push_back(generate_json_key_object(engine->vault, key, call.user->name));
    }

    return handle_key_encrypt_and_response(call, out);
}

static CommandResult handle_export_vault_keys(const CommandCall& call) {
    if (!call.user->isSuperAdmin()) return invalid("vault keys export: only super admins can export vault keys");
    if (call.positionals.empty()) return invalid("vault keys export: missing <vault_id | name | all> <output_file>");
    if (call.positionals.size() > 2) return invalid("vault keys export: too many arguments");
    if (call.positionals[0] == "all") return handle_export_all_vault_keys(call);
    return handle_export_vault_key(call);
}

void vh::shell::registerVaultCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("vault", "Manage a single vault",
                       [](const CommandCall& call) -> CommandResult {
                           if (call.positionals.empty()) return usage_vault_root(call.user->isSuperAdmin());

                           const std::string_view sub = call.positionals[0];
                           CommandCall subcall = call;
                           // shift subcommand off positionals
                           subcall.positionals.erase(subcall.positionals.begin());

                           if (sub == "create" || sub == "new") return handle_vault_create(subcall);
                           if (sub == "delete" || sub == "rm") return handle_vault_delete(subcall);
                           if (sub == "info" || sub == "get") return handle_vault_info(subcall);
                           if (sub == "update" || sub == "set") return handle_vault_update(subcall);
                           if (sub == "assign" || sub == "role") return handle_vault_role_assign(subcall);
                           if (sub == "keys") {
                               if (call.positionals.size() == 1)
                                   return invalid("Usage: vault keys <export | rotate | inspect> <vault_id | name | all> [--recipient <fingerprint>] [--output <file>] [--owner <id | name>]");

                               const auto sub2 = subcall.positionals[0];
                               subcall.positionals.erase(subcall.positionals.begin());

                               if (sub2 == "export") return handle_export_vault_keys(subcall);
                               return invalid("vault keys: unknown subcommand '" + std::string(subcall.positionals[1]) +
                                   "'. Use: export | rotate | inspect");
                           }

                           return invalid(
                               "vault: unknown subcommand '" + std::string(sub) + "'. Use: create | delete | info");
                       }, {"v"});

    r->registerCommand("vaults", "List vaults",
                       [](const CommandCall& call) -> CommandResult {
                           if (hasKey(call, "help") || hasKey(call, "h")) return usage_vaults_list();
                           return handle_vaults_list(call);
                       }, {"ls"});
}