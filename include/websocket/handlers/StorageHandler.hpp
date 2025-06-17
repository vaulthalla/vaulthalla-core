#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::storage {
    class StorageManager;
}

namespace vh::keys {
    class APIKeyManager;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class StorageHandler {
    public:
        explicit StorageHandler(const std::shared_ptr<vh::storage::StorageManager>& storageManager);

        // API commands
        void handleAddAPIKey(const json& msg, WebSocketSession& session);
        void handleRemoveAPIKey(const json& msg, WebSocketSession& session);
        void handleListAPIKeys(const json& msg, WebSocketSession& session);
        void handleListUserAPIKeys(const json& msg, WebSocketSession& session);
        void handleGetAPIKey(const json& msg, WebSocketSession& session);

        // All Vault commands
        void handleListVaults(const json& msg, WebSocketSession& session);
        void handleAddVault(const json& msg, WebSocketSession& session);
        void handleRemoveVault(const json& msg, WebSocketSession& session);
        void handleGetVault(const json& msg, WebSocketSession& session);

        // Volume management
        void handleAddVolume(const json& msg, WebSocketSession& session);
        void handleRemoveVolume(const json& msg, WebSocketSession& session);
        void handleListUserVolumes(const json& msg, WebSocketSession& session);
        void handleListVaultVolumes(const json& msg, WebSocketSession& session);
        void handleListVolumes(const json& msg, WebSocketSession& session);
        void handleGetVolume(const json& msg, WebSocketSession& session);


    private:
        std::shared_ptr<vh::storage::StorageManager> storageManager_;
        std::shared_ptr<vh::keys::APIKeyManager> apiKeyManager_;
    };

} // namespace vh::websocket
