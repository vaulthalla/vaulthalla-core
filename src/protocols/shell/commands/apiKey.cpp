#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "types/APIKey.hpp"
#include "types/User.hpp"
#include "util/shellArgsHelpers.hpp"
#include "crypto/APIKeyManager.hpp"
#include "storage/cloud/s3/S3Controller.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"
#include "usage/include/usages.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::services;
using namespace vh::crypto;
using namespace vh::types::api;
using namespace vh::cloud;

static S3Provider s3_provider_from_shell_input(const std::string& str) {
    if (str == "aws") return S3Provider::AWS;
    if (str == "cloudflare-r2") return S3Provider::CloudflareR2;
    if (str == "wasabi") return S3Provider::Wasabi;
    if (str == "backblaze-b2") return S3Provider::BackblazeB2;
    if (str == "digitalocean") return S3Provider::DigitalOcean;
    if (str == "minio") return S3Provider::MinIO;
    if (str == "ceph") return S3Provider::Ceph;
    if (str == "storj") return S3Provider::Storj;
    if (str == "other") return S3Provider::Other;
    throw std::invalid_argument("Invalid provider: " + str);
}

static CommandResult handleListAPIKeys(const CommandCall& call) {
    try {
        const auto params = parseListQuery(call);

        std::vector<std::shared_ptr<APIKey>> keys;
        if (call.user->canManageAPIKeys()) keys = APIKeyQueries::listAPIKeys(params);
        else keys = APIKeyQueries::listAPIKeys(call.user->id, params);

        if (hasFlag(call, "json")) {
            auto out = nlohmann::json(keys).dump(4);
            out.push_back('\n');
            return ok(out);
        }

        return ok(to_string(keys));
    } catch (const std::exception& e) {
        return invalid("Failed to list API keys: " + std::string(e.what()));
    }
}

static CommandResult handleCreateAPIKey(const CommandCall& call) {
    try {
        const auto nameOpt = optVal(call, "name");
        const auto accessKeyOpt = optVal(call, "access");
        const auto secretOpt = optVal(call, "secret");
        const auto regionOpt = optVal(call, "region");
        const auto endpointOpt = optVal(call, "endpoint");
        const auto providerOpt = optVal(call, "provider");

        std::vector<std::string> errors;
        if (!nameOpt || nameOpt->empty()) errors.emplace_back("Missing required option: --name");
        if (!accessKeyOpt || accessKeyOpt->empty()) errors.emplace_back("Missing required option: --access");
        if (!secretOpt || secretOpt->empty()) errors.emplace_back("Missing required option: --secret");
        if (!regionOpt || regionOpt->empty()) errors.emplace_back("Missing required option: --region");
        if (!endpointOpt || endpointOpt->empty()) errors.emplace_back("Missing required option: --endpoint");
        if (!providerOpt || providerOpt->empty()) {
            errors.emplace_back("Missing required option: --provider");
        }

        if (!errors.empty()) {
            std::string errorMsg = "API key creation failed:\n";
            for (const auto& err : errors) errorMsg += "  - " + err + "\n";
            return invalid(errorMsg);
        }

        auto key = std::make_shared<APIKey>();
        key->user_id = call.user->id;
        key->name = *nameOpt;
        key->access_key = *accessKeyOpt;
        key->secret_access_key = *secretOpt;
        key->region = *regionOpt;
        key->endpoint = *endpointOpt;
        key->provider = s3_provider_from_shell_input(*providerOpt);

        const auto [valid, validationErrors] = S3Controller(key, "").validateAPICredentials();

        if (!valid) return invalid("API key validation failed:\n" + validationErrors);

        key->id = ServiceDepsRegistry::instance().apiKeyManager->addAPIKey(key);

        return ok("Successfully created API key!\n" + to_string(key));
    } catch (const std::exception& e) {
        return invalid("API key creation failed: " + std::string(e.what()));
    }
}

static CommandResult handleDeleteAPIKey(const CommandCall& call) {
    try {
        const auto id = call.positionals.empty() ? "" : call.positionals[0];
        if (id.empty()) return invalid("Usage: api-key delete <id>");

        const auto key = APIKeyQueries::getAPIKey(std::stoi(id));
        if (!key) return invalid("API key not found: " + id);

        if (!call.user->canManageAPIKeys() && key->user_id != call.user->id)
            return invalid("You do not have permission to delete this API key.");

        ServiceDepsRegistry::instance().apiKeyManager->removeAPIKey(key->id, key->user_id);

        return ok("API key deleted successfully: " + std::to_string(key->id) + "\n");
    } catch (const std::exception& e) {
        return invalid("Failed to delete API key: " + std::string(e.what()));
    }
}

static CommandResult handleAPIKeyInfo(const CommandCall& call) {
    try {
        const auto id = call.positionals.empty() ? "" : call.positionals[0];
        if (id.empty()) return invalid("Usage: api-key info <name | id>");

        const auto key = APIKeyQueries::getAPIKey(std::stoi(id));
        if (!key) return invalid("API key not found: " + id);

        if (!call.user->canManageAPIKeys() && key->user_id != call.user->id)
            return invalid("You do not have permission to access this API key.");

        return ok(to_string(key));
    } catch (const std::exception& e) {
        return invalid("Failed to retrieve API key info: " + std::string(e.what()));
    }
}

static bool isAPIKeyMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"api-key", cmd}, input);
}

static CommandResult handle_key(const CommandCall& call) {
    if (call.positionals.empty()) return usage(call.constructFullArgs());

    const auto [sub, subcall] = descend(call);

    if (isAPIKeyMatch("list", sub)) return handleListAPIKeys(subcall);
    if (isAPIKeyMatch("create", sub)) return handleCreateAPIKey(subcall);
    if (isAPIKeyMatch("delete", sub)) return handleDeleteAPIKey(subcall);
    if (isAPIKeyMatch("info", sub)) return handleAPIKeyInfo(subcall);

    return invalid(call.constructFullArgs(), "Unknown api-key subcommand: '" + std::string(sub) + "'");
}

static CommandResult handle_keys(const CommandCall& call) {
    return handleListAPIKeys(call);
}

void commands::registerAPIKeyCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    r->registerCommand(usageManager->resolve("api-key"), handle_key);
}
