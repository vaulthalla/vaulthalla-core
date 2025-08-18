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
#include "services/LogRegistry.hpp"

#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>

using namespace vh::websocket;
using namespace vh::types;
using namespace vh::database;
using namespace vh::storage;
using namespace vh::logging;
using namespace vh::services;
using json = nlohmann::json;

StorageHandler::StorageHandler(const std::shared_ptr<StorageManager>& storageManager)
    : storageManager_(storageManager), apiKeyManager_(ServiceDepsRegistry::instance().apiKeyManager) {
}

void StorageHandler::handleAddAPIKey(const json& msg, WebSocketSession& session) const {
    try {
        const auto& payload = msg.at("payload");
        const auto userID = payload.at("user_id").get<unsigned int>();
        const auto name = payload.at("name").get<std::string>();
        const auto provider = api::s3_provider_from_string(payload.at("provider").get<std::string>());
        const auto accessKey = payload.at("access_key").get<std::string>();
        const auto secretKey = payload.at("secret_access_key").get<std::string>();
        const auto region = payload.at("region").get<std::string>();
        const auto endpoint = payload.at("endpoint").get<std::string>();

        auto key = std::make_shared<api::APIKey>(userID, name, provider, accessKey, secretKey, region, endpoint);
        apiKeyManager_->addAPIKey(key);

        const json response = {{"command", "storage.apiKey.add.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"}};

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Added API key for user ID: {}", userID);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleAddAPIKey error: {}", e.what());

        const json response = {{"command", "storage.apiKey.add.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleRemoveAPIKey(const json& msg, WebSocketSession& session) const {
    try {
        const auto keyId = msg.at("payload").at("id").get<unsigned int>();
        const auto user = session.getAuthenticatedUser();
        apiKeyManager_->removeAPIKey(keyId, user->id);

        const json response = {{"command", "storage.apiKey.remove.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"}};

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Removed API key with ID: {} for user ID: {}", keyId, user->id);

    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleRemoveAPIKey error: {}", e.what());

        const json response = {{"command", "storage.apiKey.remove.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleListAPIKeys(const json& msg, WebSocketSession& session) const {
    try {
        const auto keys = apiKeyManager_->listAPIKeys();

        const json data = {{"keys", json(keys).dump(4)}};

        const json response = {{"command", "storage.apiKey.list.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        LogRegistry::storage()->debug("[StorageHandler] Listed all API keys successfully.");
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleListAPIKeys error: " << e.what() << std::endl;

        const json response = {{"command", "storage.apiKey.list.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleListUserAPIKeys(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user) throw std::runtime_error("User not authenticated");
        const auto keys = apiKeyManager_->listUserAPIKeys(user->id);

        const json data{{"keys", json(keys).dump(4)}};

        const json response = {{"command", "storage.apiKey.list.user.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        LogRegistry::storage()->debug("[StorageHandler] Listed API keys for user ID: {}", user->id);
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleListUserAPIKeys error: " << e.what() << std::endl;

        const json response = {{"command", "storage.apiKey.list.user.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleGetAPIKey(const json& msg, WebSocketSession& session) const {
    try {
        unsigned int keyId = msg.at("payload").at("id").get<unsigned int>();
        const auto user = session.getAuthenticatedUser();
        auto key = apiKeyManager_->getAPIKey(keyId, user->id);

        json data = {{"api_key", key}};

        const json response = {{"command", "storage.apiKey.get.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        LogRegistry::storage()->debug("[StorageHandler] Fetched API key with ID: {} for user ID: {}", keyId, user->id);
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleGetAPIKey error: " << e.what() << std::endl;

        const json response = {{"command", "storage.apiKey.get.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleAddVault(const json& msg, WebSocketSession& session) const {
    try {
        const json& payload = msg.at("payload");
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

        vault = storageManager_->addVault(vault, sync);

        const json data = {{"vault", *vault}};

        const json response = {{"command", "storage.vault.add.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Added vault with ID: {} and type: {}", vault->id, type);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleAddVault error: {}", e.what());

        const json response = {{"command", "storage.vault.add.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleUpdateVault(const json& msg, WebSocketSession& session) const {
    try {
        const json& payload = msg.at("payload");
        const auto vault = std::make_shared<Vault>(payload);

        storageManager_->updateVault(vault);

        const json data = {{"vault", *vault}};

        const json response = {{"command", "storage.vault.update.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Updated vault with ID: {}", vault->id);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleUpdateVault error: {}", e.what());

        const json response = {{"command", "storage.vault.update.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleRemoveVault(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();

        const auto vaultId = msg.at("payload").at("id").get<unsigned int>();
        const auto vault = storageManager_->getVault(vaultId);

        if (!user->isAdmin() && (!user->canManageVaults() || user->id != vault->owner_id))
            throw std::runtime_error("User does not have permission to delete vaults.");

        storageManager_->removeVault(vaultId);

        const json response = {{"command", "storage.vault.remove.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"}};

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Removed vault with ID: {}", vaultId);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleRemoveVault error: {}", e.what());

        const json response = {{"command", "storage.vault.remove.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleGetVault(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        const auto vaultId = msg.at("payload").at("id").get<unsigned int>();
        const auto vault = storageManager_->getVault(vaultId);
        if (!vault) throw std::runtime_error("Vault not found with ID: " + std::to_string(vaultId));

        json data = {};

        if (vault->type == VaultType::S3) {
            const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
            data["vault"] = *s3Vault;
        } else data["vault"] = *vault;

        if (vault->owner_id == user->id) data["vault"]["owner"] = user->name;
        else data["vault"]["owner"] = VaultQueries::getVaultOwnersName(vaultId);

        const json response = {{"command", "storage.vault.get.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Fetched vault with ID: {}", vaultId);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleGetVault error: {}", e.what());

        const json response = {{"command", "storage.vault.get.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

void StorageHandler::handleListVaults(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        std::vector<std::shared_ptr<Vault>> vaults;
        if (user->isAdmin()) vaults = VaultQueries::listVaults();
        else vaults = VaultQueries::listUserVaults(user->id);

        const json data = {{"vaults", vaults}};

        const json response = {
            {"command", "storage.vault.list.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "ok"},
            {"data", data}
        };

        session.send(response);

        LogRegistry::storage()->debug("[StorageHandler] Listed {} vaults for user ID: {}", vaults.size(), user->id);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleListVaults error: {}", e.what());

        const json response = {
            {"command", "storage.vault.list.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "error"},
            {"error", e.what()}
        };

        session.send(response);
    }
}

void StorageHandler::handleSyncVault(const json& msg, WebSocketSession& session) const {
    try {
        const auto user = session.getAuthenticatedUser();
        const auto vaultId = msg.at("payload").at("id").get<unsigned int>();

        ServiceDepsRegistry::instance().syncController->runNow(vaultId);

        const json response = {
            {"command", "storage.vault.sync.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "ok"}
        };

        session.send(response);

        LogRegistry::storage()->info("[StorageHandler] Triggered sync for vault ID: {}", vaultId);
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[StorageHandler] handleSyncVault error: {}", e.what());

        const json response = {
            {"command", "storage.vault.sync.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "error"},
            {"error", e.what()}
        };

        session.send(response);
    }
}
