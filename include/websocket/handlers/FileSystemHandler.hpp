#pragma once

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;
    class StorageHandler;
    namespace vh::security {
        class PermissionManager;
    }

    class FileSystemHandler {
    public:
        explicit FileSystemHandler(std::shared_ptr<StorageHandler> storageHandler);
        FileSystemHandler(std::shared_ptr<StorageHandler> storageHandler,
                          std::shared_ptr<vh::security::PermissionManager> permissionManager);

        void handleListDir(const json& msg, WebSocketSession& session);
        void handleReadFile(const json& msg, WebSocketSession& session);
        void handleWriteFile(const json& msg, WebSocketSession& session);
        void handleDeleteFile(const json& msg, WebSocketSession& session);

    private:
        std::shared_ptr<StorageHandler> storageHandler_;
        std::shared_ptr<vh::security::PermissionManager> permissionManager_;

        void validateAuth(WebSocketSession& session);
        void enforcePermission(WebSocketSession& session,
                               const std::string& mountName,
                               const std::string& path,
                               const std::string& requiredPermission);
    };

} // namespace vh::websocket
