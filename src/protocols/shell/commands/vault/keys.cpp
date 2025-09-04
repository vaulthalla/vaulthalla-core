#include "protocols/shell/commands/vault.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/VaultKeyQueries.hpp"

#include "logging/LogRegistry.hpp"
#include "services/SyncController.hpp"

#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/cloud/s3/S3Controller.hpp"

#include "crypto/VaultEncryptionManager.hpp"
#include "crypto/GPGEncryptor.hpp"

#include "types/Vault.hpp"
#include "types/APIKey.hpp"
#include "types/User.hpp"

#include "config/ConfigRegistry.hpp"

#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <fstream>

using namespace vh::shell::commands::vault;
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
    constexpr const auto* ERR = "vault keys export";

    const auto vaultArg = call.positionals[0];

    const auto engLkp = resolveEngine(call, vaultArg, ERR);
    if (!engLkp || !engLkp.ptr) return invalid(engLkp.error);
    const auto engine = engLkp.ptr;

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
    constexpr const auto* ERR = "vault keys inspect";

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

    const auto engLkp = resolveEngine(call, vaultArg, ERR);
    if (!engLkp || !engLkp.ptr) return invalid(engLkp.error);
    const auto engine = engLkp.ptr;

    const auto vaultKey = VaultKeyQueries::getVaultKey(engine->vault->id);

    return ok(api::generate_json_key_info_object(engine->vault, vaultKey, call.user->name).dump(4));
}


static CommandResult handle_rotate_vault_keys(const CommandCall& call) {
    constexpr const auto* ERR = "vault keys rotate";

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

    const auto syncNow = hasFlag(call, "now");
    const auto rotateKey = [&syncNow](const std::shared_ptr<StorageEngine>& engine) {
        engine->encryptionManager->prepare_key_rotation();
        if (syncNow) ServiceDepsRegistry::instance().syncController->runNow(engine->vault->id);
    };

    const auto vaultArg = call.positionals[0];

    if (vaultArg == "all") {
        for (const auto& engine : ServiceDepsRegistry::instance().storageManager->getEngines())
            rotateKey(engine);

        return ok("Vault keys for all vaults have been rotated successfully.\n"
            "If you have --now flag set, the sync will be triggered immediately.");
    }

    const auto engLkp = resolveEngine(call, vaultArg, ERR);
    if (!engLkp || !engLkp.ptr) return invalid(engLkp.error);
    const auto engine = engLkp.ptr;

    rotateKey(engine);

    return ok("Vault key for '" + engine->vault->name + "' (ID: " + std::to_string(engine->vault->id) +
              ") has been rotated successfully.\n"
              "If you have --now flag set, the sync will be triggered immediately.");
}


CommandResult commands::vault::handle_vault_keys(const CommandCall& call) {
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
