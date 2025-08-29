#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "types/APIKey.hpp"
#include "types/User.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "crypto/APIKeyManager.hpp"
#include "storage/cloud/s3/S3Controller.hpp"
#include "APIKeyUsage.hpp"

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
        const bool json = hasFlag(call, "json");

        const auto user = call.user;
        if (!user) return invalid("You must be logged in to list API keys.");

        std::vector<std::shared_ptr<APIKey>> keys;
        if (user->canManageAPIKeys()) keys = APIKeyQueries::listAPIKeys();
        else keys = APIKeyQueries::listAPIKeys(user->id);

        if (json) {
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
            errors.push_back(APIKeyUsage::usage_provider());
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

static CommandResult handle_key(const CommandCall& call) {
    if (call.positionals.empty()) return ok(APIKeyUsage::all().str());

    const std::string_view sub = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (sub == "create" || sub == "new") return handleCreateAPIKey(subcall);
    if (sub == "delete" || sub == "rm") return handleDeleteAPIKey(subcall);
    if (sub == "info" || sub == "get") return handleAPIKeyInfo(subcall);
    if (sub == "list") return handleListAPIKeys(subcall);
    return ok(APIKeyUsage::all().str());
}

static CommandResult handle_keys(const CommandCall& call) {
    return handleListAPIKeys(call);
}

void vh::shell::registerAPIKeyCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand(APIKeyUsage::apikeys_list(), handle_keys);
    r->registerCommand(APIKeyUsage::apikey(), handle_key);
}
