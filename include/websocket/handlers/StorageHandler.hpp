#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::storage {
    class StorageManager;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class StorageHandler {
    public:
        explicit StorageHandler(const std::shared_ptr<vh::storage::StorageManager>& storageManager);

        // API commands
        void handleAddS3APIKey(const json& msg, WebSocketSession& session);
        void handleRemoveAPIKey(const json& msg, WebSocketSession& session);
        void handleListS3APIKeys(const json& msg, WebSocketSession& session);
        void handleGetS3APIKey(const json& msg, WebSocketSession& session);

        // Local Disk Vault commands
        void handleInitLocalDisk(const json& msg, WebSocketSession& session);
        void handleRemoveLocalDiskVault(const json& msg, WebSocketSession& session);
        void handleGetLocalDiskVault(const json& msg, WebSocketSession& session);

        // S3 Vault commands
        void handleInitS3(const json& msg, WebSocketSession& session);
        void handleRemoveS3Vault(const json& msg, WebSocketSession& session);
        void handleListS3Vaults(const json& msg, WebSocketSession& session);
        void handleGetS3Vault(const json& msg, WebSocketSession& session);

        // Volume management
        void handleAddVolume(const json& msg, WebSocketSession& session);
        void handleRemoveVolume(const json& msg, WebSocketSession& session);
        void handleListVolumes(const json& msg, WebSocketSession& session);
        void handleGetVolume(const json& msg, WebSocketSession& session);


    private:
        std::shared_ptr<vh::storage::StorageManager> storageManager_;
    };

} // namespace vh::websocket
