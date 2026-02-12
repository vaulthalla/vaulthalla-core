#include "protocols/websocket/handlers/APIKeyHandler.hpp"
#include "types/vault/APIKey.hpp"
#include "types/entities/User.hpp"
#include "types/vault/Vault.hpp"
#include "types/sync/Sync.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "crypto/APIKeyManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"

#include <nlohmann/json.hpp>

using namespace vh::websocket;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::logging;
using namespace vh::services;
using json = nlohmann::json;

json APIKeyHandler::add(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (!user) throw std::invalid_argument("Invalid user name");

    const auto userID = payload.at("user_id").get<unsigned int>();
    if (user->id != userID) throw std::invalid_argument("Invalid user ID");

    const auto name = payload.at("name").get<std::string>();
    const auto provider = api::s3_provider_from_string(payload.at("provider").get<std::string>());
    const auto accessKey = payload.at("access_key").get<std::string>();
    const auto secretKey = payload.at("secret_access_key").get<std::string>();
    const auto region = payload.at("region").get<std::string>();
    const auto endpoint = payload.at("endpoint").get<std::string>();

    auto key = std::make_shared<api::APIKey>(userID, name, provider, accessKey, secretKey, region, endpoint);
    ServiceDepsRegistry::instance().apiKeyManager->addAPIKey(key);

    return {};
}

json APIKeyHandler::remove(const json& payload, const WebSocketSession& session) {
    const auto keyId = payload.at("id").get<unsigned int>();
    const auto user = session.getAuthenticatedUser();
    ServiceDepsRegistry::instance().apiKeyManager->removeAPIKey(keyId, user->id);
    return {};
}

json APIKeyHandler::list(const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();

    const auto keys = user->canManageAPIKeys() ?
        ServiceDepsRegistry::instance().apiKeyManager->listAPIKeys() :
        ServiceDepsRegistry::instance().apiKeyManager->listUserAPIKeys(user->id);

    return {{"keys", json(keys).dump(4)}};
}

json APIKeyHandler::get(const json& payload, const WebSocketSession& session) {
    const unsigned int keyId = payload.at("id").get<unsigned int>();
    const auto user = session.getAuthenticatedUser();
    return {{"api_key", ServiceDepsRegistry::instance().apiKeyManager->getAPIKey(keyId, user->id)}};
}
