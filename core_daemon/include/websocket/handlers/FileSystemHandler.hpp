#pragma once

#include "services/ServiceManager.hpp"
#include "storage/StorageManager.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

class FileSystemHandler {
  public:
    explicit FileSystemHandler(const std::shared_ptr<services::ServiceManager>& serviceManager);

    void handleListDir(const json& msg, WebSocketSession& session);
    void handleReadFile(const json& msg, WebSocketSession& session);
    void handleWriteFile(const json& msg, WebSocketSession& session);
    void handleDeleteFile(const json& msg, WebSocketSession& session);

  private:
    std::shared_ptr<storage::StorageManager> storageManager_;

    static void validateAuth(WebSocketSession& session);
    void enforcePermission(WebSocketSession& session, const std::string& mountName, const std::string& path,
                           const std::string& requiredPermission);
};

} // namespace vh::websocket
