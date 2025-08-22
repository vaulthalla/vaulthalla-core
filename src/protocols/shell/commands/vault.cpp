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

#include "services/LogRegistry.hpp"
#include "services/SyncController.hpp"

#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"

#include "crypto/VaultEncryptionManager.hpp"
#include "crypto/GPGEncryptor.hpp"

#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/DBQueryParams.hpp"
#include "types/APIKey.hpp"
#include "types/Group.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "types/VaultRole.hpp"

#include "config/ConfigRegistry.hpp"
#include "util/interval.hpp"
#include "protocols/shell/usage/VaultUsage.hpp"

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


// ################################################################################
// #################### 🧱 Vault Lifecycle (create/delete) ########################
// ################################################################################


static CommandResult handle_vault_create(const CommandCall& call) {
    try {
        if (!call.user->canCreateVaults())
            return invalid(
                "vault create: user ID " + std::to_string(call.user->id) +
                " does not have permission to create vaults");

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

        vault = ServiceDepsRegistry::instance().storageManager->addVault(vault, sync);

        return ok("Successfully created new vault!\n" + to_string(vault));
    } catch (const std::exception& e) {
        return invalid("vault create: " + std::string(e.what()));
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
    } else {
        for (const auto& v : vaults)
            if (!call.user->canListVaultData(v->id))
                std::ranges::remove_if(vaults, [&](const std::shared_ptr<Vault>& vault) {
                    return vault->id == v->id;
                });
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
        if (!ownerOpt) throw std::invalid_argument(
            "vault sync update: missing required --owner <id | name> for vault name lookup");
        unsigned int ownerId = call.user->id;
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) throw std::invalid_argument(
                "vault sync update: --owner <id> must be a positive integer");
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
