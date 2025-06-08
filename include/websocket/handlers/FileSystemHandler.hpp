#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include "security/AccessControl.hpp"
#include "security/PermissionManager.hpp"
#include "storage/StorageManager.hpp"
#include "services/ServiceManager.hpp"

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class FileSystemHandler {
    public:
        explicit FileSystemHandler(const std::shared_ptr<vh::services::ServiceManager>& serviceManager);

        void handleListDir(const json& msg, WebSocketSession& session);
        void handleReadFile(const json& msg, WebSocketSession& session);
        void handleWriteFile(const json& msg, WebSocketSession& session);
        void handleDeleteFile(const json& msg, WebSocketSession& session);

    private:
        std::shared_ptr<vh::storage::StorageManager> storageManager_;
        std::shared_ptr<vh::security::AccessControl> accessControl_;
        std::shared_ptr<vh::security::PermissionManager> permissionManager_;

        static void validateAuth(WebSocketSession& session);
        void enforcePermission(WebSocketSession& session,
                               const std::string& mountName,
                               const std::string& path,
                               const std::string& requiredPermission);
    };

} // namespace vh::websocket
