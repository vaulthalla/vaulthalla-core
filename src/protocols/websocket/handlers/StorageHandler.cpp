#include "protocols/websocket/handlers/StorageHandler.hpp"
#include "types/APIKey.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/Sync.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "crypto/APIKeyManager.hpp"
#include "storage/StorageManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "services/SyncController.hpp"

#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>

using namespace vh::websocket;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::logging;
using namespace vh::services;
using json = nlohmann::json;

json StorageHandler::addAPIKey(const json& payload, const WebSocketSession& session) {
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

json StorageHandler::removeAPIKey(const json& payload, const WebSocketSession& session) {
    const auto keyId = payload.at("id").get<unsigned int>();
    const auto user = session.getAuthenticatedUser();
    ServiceDepsRegistry::instance().apiKeyManager->removeAPIKey(keyId, user->id);
    return {};
}

json StorageHandler::listAPIKeys(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();

    const auto listAll = payload.value("all", false);
    const auto keys = user->canManageAPIKeys() && listAll ?
        ServiceDepsRegistry::instance().apiKeyManager->listAPIKeys() :
        ServiceDepsRegistry::instance().apiKeyManager->listUserAPIKeys(user->id);

    return {{"keys", json(keys).dump(4)}};
}

json StorageHandler::getAPIKey(const json& payload, const WebSocketSession& session) {
    const unsigned int keyId = payload.at("id").get<unsigned int>();
    const auto user = session.getAuthenticatedUser();
    return {{"api_key", ServiceDepsRegistry::instance().apiKeyManager->getAPIKey(keyId, user->id)}};
}

json StorageHandler::addVault(const json& payload, const WebSocketSession& session) {
    const std::string name = payload.at("name").get<std::string>();
    const std::string type = payload.at("type").get<std::string>();
    const std::string typeLower = boost::algorithm::to_lower_copy(type);
    const std::string mountPoint = payload.at("mount_point").get<std::string>();

    std::shared_ptr<Vault> vault;
    std::shared_ptr<Sync> sync = nullptr;

    if (typeLower == "s3") {
        const auto apiKeyID = payload.at("api_key_id").get<unsigned int>();
        const std::string bucket = payload.at("bucket").get<std::string>();
        vault = std::make_shared<S3Vault>(name, apiKeyID, bucket);
        sync = std::make_shared<Sync>(payload);
    }

    vault->name = name;
    vault->mount_point = mountPoint;
    vault->owner_id = session.getAuthenticatedUser()->id;

    vault = ServiceDepsRegistry::instance().storageManager->addVault(vault, sync);

    return {{"vault", *vault}};
}

json StorageHandler::updateVault(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();

    const auto vault = std::make_shared<Vault>(payload);
    if (!user->canManageVault(vault->id)) throw std::runtime_error("User does not have permission to update this vault.");

    ServiceDepsRegistry::instance().storageManager->updateVault(vault);
    return {{"vault", *vault}};
}

json StorageHandler::removeVault(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    const auto vaultId = payload.at("id").get<unsigned int>();
    const auto vault = ServiceDepsRegistry::instance().storageManager->getVault(vaultId);

    if (!user->isAdmin() && (!user->canManageVaults() || !user->canManageVault(vaultId)))
        throw std::runtime_error("User does not have permission to delete vaults.");

    ServiceDepsRegistry::instance().storageManager->removeVault(vaultId);
    return {};
}

json StorageHandler::getVault(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    const auto vaultId = payload.at("id").get<unsigned int>();
    if (!user->canManageVault(vaultId)) throw std::runtime_error("User does not have permission to get vaults.");

    const auto vault = ServiceDepsRegistry::instance().storageManager->getVault(vaultId);
    if (!vault) throw std::runtime_error("Vault not found with ID: " + std::to_string(vaultId));

    json data = {};

    if (vault->type == VaultType::S3) {
        const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
        data["vault"] = *s3Vault;
    } else data["vault"] = *vault;

    if (vault->owner_id == user->id) data["vault"]["owner"] = user->name;
    else data["vault"]["owner"] = VaultQueries::getVaultOwnersName(vaultId);

    return data;
}

json StorageHandler::listVaults(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    if (user->canManageVaults() && payload.value("all", false))
        return {{"vaults", VaultQueries::listVaults()}};
    return {{"vaults", VaultQueries::listUserVaults(user->id)}};
}

json StorageHandler::syncVault(const json& payload, const WebSocketSession& session) {
    const auto user = session.getAuthenticatedUser();
    const auto vaultId = payload.at("id").get<unsigned int>();
    if (!user->canSyncVaultData(vaultId)) throw std::runtime_error("User does not have permission to sync vaults.");
    ServiceDepsRegistry::instance().syncController->runNow(vaultId);
    return {};
}
