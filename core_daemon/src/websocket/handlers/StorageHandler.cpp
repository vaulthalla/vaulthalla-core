#include "websocket/handlers/StorageHandler.hpp"
#include "types/APIKey.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "keys/APIKeyManager.hpp"
#include "storage/StorageManager.hpp"
#include "websocket/WebSocketSession.hpp"
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

        json data;

        if (key->type == types::api::APIKeyType::S3) {
            auto s3Key = std::dynamic_pointer_cast<types::api::S3APIKey>(key);
            data = {{"key",
                     {{"id", s3Key->id},
                      {"user_id", s3Key->user_id},
                      {"type", to_string(s3Key->type)},
                      {"name", s3Key->name},
                      {"created_at", util::timestampToString(s3Key->created_at)},
                      {"provider", to_string(s3Key->provider)},
                      {"access_key", s3Key->access_key},
                      {"secret_access_key", s3Key->secret_access_key},
                      {"region", s3Key->region},
                      {"endpoint", s3Key->endpoint}}}};
        } else {
            throw std::runtime_error("Unsupported API key type: " + to_string(key->type));
        }

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
        if (database::VaultQueries::localDiskVaultExists())
            throw std::runtime_error(
                "Local disk vault already exists. Only one local disk vault is allowed.");

        const json& payload = msg.at("payload");
        const std::string name = payload.at("name").get<std::string>();
        const std::string type = payload.at("type").get<std::string>();
        const std::string typeLower = boost::algorithm::to_lower_copy(type);

        std::unique_ptr<types::Vault> vault;

        if (typeLower == "local") {
            const std::string mountPoint = payload.at("mount_point").get<std::string>();
            vault = std::make_unique<types::LocalDiskVault>(name, mountPoint);
        } else if (typeLower == "s3") {
            unsigned short apiKeyID = payload.at("api_key_id").get<unsigned short>();
            const std::string bucket = payload.at("bucket").get<std::string>();
            vault = std::make_unique<types::S3Vault>(name, apiKeyID, bucket);
        } else throw std::runtime_error("Unsupported vault type: " + typeLower);

        storageManager_->addVault(std::move(vault));

        const json data = {{"id", vault->id},
                           {"name", vault->name},
                           {"type", to_string(vault->type)},
                           {"isActive", vault->is_active},
                           {"createdAt", vault->created_at}};

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

void StorageHandler::handleRemoveVault(const json& msg, WebSocketSession& session) const {
    try {
        unsigned int vaultId = msg.at("payload").at("id").get<unsigned int>();
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
        unsigned int vaultId = msg.at("payload").at("id").get<unsigned int>();
        auto vault = storageManager_->getVault(vaultId);

        const json data = {{"vault", json(*vault)}};

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
        const auto vaults = storageManager_->listVaults(session.getAuthenticatedUser());

        const json data = {{"vaults", types::to_json(vaults).dump(4)}};

        const json response = {{"command", "storage.vault.list.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "ok"},
                               {"data", data}};

        session.send(response);

        std::cout << "[StorageHandler] Listed S3 vaults." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageHandler] handleListS3Vaults error: " << e.what() << std::endl;

        const json response = {{"command", "storage.vault.list.response"},
                               {"requestId", msg.at("requestId").get<std::string>()},
                               {"status", "error"},
                               {"error", e.what()}};

        session.send(response);
    }
}

} // namespace vh::websocket