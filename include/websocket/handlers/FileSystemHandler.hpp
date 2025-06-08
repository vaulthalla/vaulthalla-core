#pragma once

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::security {
    class PermissionManager;
}

namespace vh::storage {
    class StorageManager;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class FileSystemHandler {
    public:
        explicit FileSystemHandler(std::shared_ptr<vh::storage::StorageManager> storageManager);
        FileSystemHandler(std::shared_ptr<vh::storage::StorageManager> storageManager,
                          std::shared_ptr<vh::security::PermissionManager> permissionManager);

        void handleListDir(const json& msg, WebSocketSession& session);
        void handleReadFile(const json& msg, WebSocketSession& session);
        void handleWriteFile(const json& msg, WebSocketSession& session);
        void handleDeleteFile(const json& msg, WebSocketSession& session);

    private:
        std::shared_ptr<vh::storage::StorageManager> storageManager_;
        std::shared_ptr<vh::security::PermissionManager> permissionManager_;

        static void validateAuth(WebSocketSession& session);
        void enforcePermission(WebSocketSession& session,
                               const std::string& mountName,
                               const std::string& path,
                               const std::string& requiredPermission);
    };

} // namespace vh::websocket
