#include "protocols/shell/commands/apiKeys.hpp"
#include "protocols/shell/Router.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "types/APIKey.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "keys/APIKeyManager.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::services;
using namespace vh::keys;
using namespace vh::types::api;

static std::vector<std::string> getProviderOptions() {
    return {
        "aws", "cloudflare-r2", "wasabi", "backblaze-b2",
        "digitalocean", "minio", "ceph", "storj", "other"
    };
}

static std::string usage_provider() {
    std::string options = "provider options: [";
    for (const auto& opt : getProviderOptions()) options += opt + " | ";
    options.pop_back(); // remove last space
    options.pop_back(); // remove last '|'
    options += "]";
    return options;
}

static CommandResult usage_api_keys_root() {
    return {
        0,
        "Usage:\n"
        "  api-keys create --name <name> --access <accessKey> --secret <secret> --region <region=auto> "
        "--endpoint <endpoint> --provider <provider>\n"
        "  api-keys create -n <name> -a <accessKey> -s <secret> -r <region=auto> -e <endpoint> -p <provider>\n"
        "    " + usage_provider() + "\n"
        "  api-keys delete <id>\n"
        "  api-keys info <id>\n"
        "  api-keys list [--json]\n",
        ""
    };
}

static CommandResult handleListAPIKeys(const CommandCall& call) {
    const bool json = hasFlag(call, "json");

    const auto user = call.user;
    if (!user) return invalid("You must be logged in to list API keys.");

    std::vector<std::shared_ptr<APIKey>> keys;
    if (user->canAccessAnyAPIKey()) keys = APIKeyQueries::listAPIKeys();
    else keys = APIKeyQueries::listAPIKeys(user->id);

    if (json) {
        auto out = nlohmann::json(keys).dump(4);
        out.push_back('\n');
        return ok(out);
    }

    return ok(to_string(keys));
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
            errors.push_back(usage_provider());
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
        key->secret_access_key = *secretOpt; // This will be encrypted later
        key->region = *regionOpt;
        key->endpoint = *endpointOpt;
        key->provider = s3_provider_from_string(*providerOpt);

        key->id = ServiceDepsRegistry::instance().apiKeyManager->addAPIKey(key);

        return ok("Successfully created API key!\n" + to_string(key));
    } catch (const std::exception& e) {
        return invalid("API key creation failed: " + std::string(e.what()));
    }
}

static CommandResult handleDeleteAPIKey(const CommandCall& call) {
    const auto id = call.positionals.empty() ? "" : call.positionals[0];
    if (id.empty()) return invalid("Usage: api-key delete <id>");

    const auto key = APIKeyQueries::getAPIKey(std::stoi(id));
    if (!key) return invalid("API key not found: " + id);

    if (!call.user->canAccessAnyAPIKey() && key->user_id != call.user->id)
        return invalid("You do not have permission to delete this API key.");

    ServiceDepsRegistry::instance().apiKeyManager->removeAPIKey(key->id, key->user_id);

    return ok("API key deleted successfully: " + std::to_string(key->id) + "\n");
}

static CommandResult handleAPIKeyInfo(const CommandCall& call) {
    try {
        const auto id = call.positionals.empty() ? "" : call.positionals[0];
        if (id.empty()) return invalid("Usage: api-key info <name | id>");

        const auto key = APIKeyQueries::getAPIKey(std::stoi(id));
        if (!key) return invalid("API key not found: " + id);

        if (!call.user->canAccessAnyAPIKey() && key->user_id != call.user->id)
            return invalid("You do not have permission to access this API key.");

        return ok(to_string(key));
    } catch (const std::exception& e) {
        return invalid("Failed to retrieve API key info: " + std::string(e.what()));
    }
}


void vh::shell::registerAPIKeyCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("api-keys", "List API Keys",
                       [](const CommandCall& call) -> CommandResult {
                           if (!call.user) return invalid("You must be logged in to list API keys.");
                           return handleListAPIKeys(call);
                       }, {});

    r->registerCommand("api-key", "Manage a single API key",
                       [](const CommandCall& call) -> CommandResult {
                           if (call.positionals.empty()) return usage_api_keys_root();

                           const std::string_view sub = call.positionals[0];
                           CommandCall subcall = call;
                           subcall.positionals.erase(subcall.positionals.begin());

                           if (sub == "create" || sub == "new") return handleCreateAPIKey(subcall);
                           if (sub == "delete" || sub == "rm") return handleDeleteAPIKey(subcall);
                           if (sub == "info" || sub == "get") return handleAPIKeyInfo(subcall);
                           if (sub == "list") return handleListAPIKeys(subcall);
                           return usage_api_keys_root();
                       }, {"api", "ak"});
}
