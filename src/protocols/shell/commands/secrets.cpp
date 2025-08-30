#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "usage/include/SecretsUsage.hpp"
#include "util/shellArgsHelpers.hpp"
#include "types/User.hpp"
#include "crypto/TPMKeyProvider.hpp"
#include "util/files.hpp"
#include "crypto/InternalSecretManager.hpp"
#include <nlohmann/json.hpp>
#include "crypto/GPGEncryptor.hpp"
#include "services/LogRegistry.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::crypto;
using namespace vh::util;
using namespace vh::logging;

static std::vector<uint8_t> trimSecret(const std::vector<uint8_t>& secret) {
    auto start = secret.begin();
    while (start != secret.end() && std::isspace(*start)) ++start;
    auto end = secret.end();
    do { --end; } while (end != start && std::isspace(*end));
    return {start, end + 1};
}

static CommandResult handle_secrets_set(const CommandCall& call) {
    if (call.positionals.size() != 1) return invalid("secrets set: missing <secret>\n\n" + SecretsUsage::secrets_set().str());

    const auto secretArg = call.positionals[0];

    const auto fileOpt = optVal(call, "file");
    if (!fileOpt) return invalid("secrets set: missing required --file <path> (or use default /run/vaulthalla/<secret>)");
    if (!std::filesystem::exists(*fileOpt)) return invalid("secrets set: file does not exist: " + *fileOpt);

    const auto secret = trimSecret(readFileToVector(*fileOpt));

    if (secretArg == "db-password") {
        TPMKeyProvider tpm("postgres");
        tpm.init();
        tpm.updateMasterKey(secret);
        return ok("Successfully updated database password secret (sealed with TPM)");
    }

    if (secretArg == "jwt-secret") {
        const InternalSecretManager ism;
        ism.setJWTSecret({secret.begin(), secret.end()});
        return ok("Successfully updated JWT secret");
    }

    return invalid("secrets set: unknown secret '" + std::string(secretArg) + "'. Valid secrets are: db-password, jwt-secret");
}

static CommandResult handle_secret_encrypt_and_response(const CommandCall& call,
                                                     const nlohmann::json& output) {
    const auto outputOpt = optVal(call, "output");

    if (const auto recipientOpt = optVal(call, "recipient")) {
        if (recipientOpt->empty()) return invalid("secrets export: --recipient requires a value");
        if (!outputOpt) return invalid("secrets export: --recipient requires --output to specify the output file");

        try {
            GPGEncryptor::encryptToFile(output, *recipientOpt, *outputOpt);
            return ok("Secret successfully encrypted and saved to " + *outputOpt);
        } catch (const std::exception& e) {
            return invalid("secrets export: failed to encrypt secret: " + std::string(e.what()));
        }
    }

    if (outputOpt) {
        LogRegistry::audit()->warn(
            "[shell::handle_secret_encrypt_and_response] No recipient specified, saving unencrypted key(s) to " + *outputOpt);
        try {
            std::ofstream outFile(*outputOpt);
            if (!outFile) return invalid("secrets export: failed to open output file " + *outputOpt);
            outFile << output.dump(4);
            outFile.close();
            return {0, "secret(s) successfully saved to " + *outputOpt,
                    "\nWARNING: No recipient specified, key(s) are unencrypted.\n"
                    "\nConsider using --recipient with a GPG fingerprint to encrypt the key(s) before saving."};
        } catch (const std::exception& e) {
            return invalid("secrets export: failed to write to output file: " + std::string(e.what()));
        }
    }

    LogRegistry::audit()->warn(
        "[shell::handle_secret_encrypt_and_response] No recipient specified, returning unencrypted key(s)");
    return {0, output.dump(4),
            "\nWARNING: No recipient specified, key(s) are unencrypted.\n"
            "\nConsider using --recipient with a GPG fingerprint along with --output\nto securely encrypt the key(s) to an output file."};
}

static nlohmann::json generateSecretOutput(const std::string& name, const std::vector<uint8_t>& secret) {
    return {
        {"name", name},
        {"secret", std::string(secret.begin(), secret.end())}
    };
}

static nlohmann::json generateSecretOutput(const std::string& name, const std::string& secret) {
    return {
        {"name", name},
        {"secret", secret}
    };
}

static nlohmann::json getDBPassword() {
    TPMKeyProvider tpm("postgres");
    tpm.init();
    return generateSecretOutput("db-password", tpm.getMasterKey());
}

static nlohmann::json getJWTSecret() {
    const InternalSecretManager ism;
    return generateSecretOutput("jwt-secret", ism.jwtSecret());
}

static CommandResult handle_secrets_export(const CommandCall& call) {
    if (call.positionals.size() != 1) return invalid("secrets export: missing <secret>\n\n" + SecretsUsage::secrets_export().str());

    const auto secretArg = call.positionals[0];

    nlohmann::json out = nlohmann::json::array();

    if (secretArg == "db-password" || secretArg == "all") out.push_back(getDBPassword());
    if (secretArg == "jwt-secret" || secretArg == "all") out.push_back(getJWTSecret());
    if (out.empty()) return invalid("secrets export: unknown secret '" + std::string(secretArg) + "'. Valid secrets are: db-password, jwt-secret, all");
    return handle_secret_encrypt_and_response(call, out);
}

static CommandResult handle_secrets(const CommandCall& call) {
    if (!call.user->isSuperAdmin() && !call.user->canManageEncryptionKeys())
        return invalid("secrets: only super admins or users with ManageEncryptionKeys permission can manage secrets");

    if (call.positionals.empty() || hasKey(call, "help") || hasKey(call, "h")) return ok(SecretsUsage::all().str());

    const std::string_view sub = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (sub == "set" || sub == "update") return handle_secrets_set(subcall);
    if (sub == "export" || sub == "get" || sub == "show") return handle_secrets_export(subcall);

    return ok(SecretsUsage::all().str());
}

void commands::registerSecretsCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand(SecretsUsage::secrets(), handle_secrets);
}
