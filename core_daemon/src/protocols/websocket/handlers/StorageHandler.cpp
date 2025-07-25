#include "protocols/websocket/handlers/StorageHandler.hpp"
#include "types/APIKey.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/Sync.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "storage/APIKeyManager.hpp"
#include "storage/StorageManager.hpp"
#include "protocols/websocket/WebSocketSession.hpp"

#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

namespace vh::websocket {

StorageHandler::StorageHandler(const std::shared_ptr<storage::StorageManager>& storageManager)
    : storageManager_(storageManager), apiKeyManager_(std::make_shared<keys::APIKeyManager>()) {
}

void StorageHandler::handleAddAPIKey(const json& msg, WebSocketSession& session) const {
    try {
        const auto& payload = msg.at("payload");
        const auto userID = payload.at("user_id").get<unsigned int>();
        const auto name = payload.at("name").get<std::string>();
        const auto type = payload.at("type").get<std::string>();
        const auto typeLower = boost::algorithm::to_lower_copy(type);

        std::shared_ptr<types::api::APIKey> key;

        if (typeLower == "s3") {
            types::api::S3Provider provider =
                types::api::s3_provider_from_string(payload.at("provider").get<std::string>());
            const auto accessKey = payload.at("access_key").get<std::string>();
            const auto secretKey = payload.at("secret_access_key").get<std::string>();
            const auto region = payload.at("region").get<std::string>();
            const auto endpoint = payload.at("endpoint").get<std::string>();

            key = std::make_shared<
                types::api::S3APIKey>(name, userID, provider, accessKey, secretKey, region, endpoint);
        } else throw std::runtime_error("Unsupported API key type: " + type);

        apiKeyManager_->addAPIKey(key);

        const json response = {{"command", "storage.apiKey.add.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"}};

        session.send(response);

        std::cout << "[StorageHandler] Added API key: " << name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleAddAPIKey error: " << e.what() << std::endl;

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

        std::cout << "[StorageHandler] Removed API key with ID: " << keyId << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleRemoveAPIKey error: " << e.what() << std::endl;

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

        const json data = {{"keys", types::api::to_json(keys).dump(4)}};

        const json response = {{"command", "storage.apiKey.list.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        std::cout << "[StorageHandler] Listed API keys for all users." << std::endl;
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

        const json data{{"keys", types::api::to_json(keys).dump(4)}};

        const json response = {{"command", "storage.apiKey.list.user.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        std::cout << "[StorageHandler] Listed API keys for user ID: " << user->id << std::endl;
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

        if (key->type != types::api::APIKeyType::S3)
            throw std::runtime_error("Unsupported API key type: " + types::api::to_string(key->type));

        json data = {{"api_key", key}};

        const json response = {{"command", "storage.apiKey.get.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        std::cout << "[StorageHandler] Fetched API key with ID: " << keyId << std::endl;
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

        std::shared_ptr<types::Vault> vault;
        std::shared_ptr<types::Sync> sync = nullptr;

        if (typeLower == "s3") {
            const auto apiKeyID = payload.at("api_key_id").get<unsigned int>();
            const std::string bucket = payload.at("bucket").get<std::string>();
            vault = std::make_shared<types::S3Vault>(name, apiKeyID, bucket);
            sync = std::make_shared<types::Sync>(payload);
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

        std::cout << "[StorageHandler] Mounted vault: " << name << " -> " << type << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleInitLocalDisk error: " << e.what() << std::endl;

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
        const auto vault = std::make_shared<types::Vault>(payload);

        storageManager_->updateVault(vault);

        const json data = {{"vault", *vault}};

        const json response = {{"command", "storage.vault.update.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        std::cout << "[StorageHandler] Updated vault with ID: " << vault->id << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleUpdateVault error: " << e.what() << std::endl;

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

        std::cout << "[StorageHandler] Removed local disk vault with ID: " << vaultId << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleRemoveLocalDiskVault error: " << e.what() << std::endl;

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

        if (vault->type == types::VaultType::S3) {
            const auto s3Vault = std::static_pointer_cast<types::S3Vault>(vault);
            data["vault"] = *s3Vault;
        } else data["vault"] = *vault;

        if (vault->owner_id == user->id) data["vault"]["owner"] = user->name;
        else data["vault"]["owner"] = database::VaultQueries::getVaultOwnersName(vaultId);

        const json response = {{"command", "storage.vault.get.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        std::cout << "[StorageHandler] Fetched local disk vault with ID: " << vaultId << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleGetLocalDiskVault error: " << e.what() << std::endl;

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
        const auto vaults = storageManager_->listVaults(user);

        nlohmann::json jVaults = nlohmann::json::array();

        for (const auto& vault : vaults) {
            nlohmann::json jVault = *vault;
            if (vault->owner_id == user->id) jVault["owner"] = user->name;
            else jVault["owner"] = database::VaultQueries::getVaultOwnersName(vault->id);
            jVaults.push_back(jVault);
        }

        const json data = {{"vaults", jVaults}};

        const json response = {
            {"command", "storage.vault.list.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "ok"},
            {"data", data}
        };

        session.send(response);

        std::cout << "[StorageHandler] Listed vaults." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleListVaults error: " << e.what() << std::endl;

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

        storageManager_->syncNow(vaultId);

        const json response = {
            {"command", "storage.vault.sync.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "ok"}
        };

        session.send(response);

        std::cout << "[StorageHandler] Triggered sync for vault ID: " << vaultId << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleSyncVault error: " << e.what() << std::endl;

        const json response = {
            {"command", "storage.vault.sync.response"},
            {"requestId", msg.at("requestId").get<std::string>()},
            {"status", "error"},
            {"error", e.what()}
        };

        session.send(response);
    }
}


} // namespace vh::websocket