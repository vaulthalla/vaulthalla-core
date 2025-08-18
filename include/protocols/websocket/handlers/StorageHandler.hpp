#pragma once

#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::storage {
class StorageManager;
}

namespace vh::crypto {
class APIKeyManager;
}

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

class StorageHandler {
  public:
    explicit StorageHandler(const std::shared_ptr<storage::StorageManager>& storageManager);

    // API commands
    void handleAddAPIKey(const json& msg, WebSocketSession& session) const;
    void handleRemoveAPIKey(const json& msg, WebSocketSession& session) const;
    void handleListAPIKeys(const json& msg, WebSocketSession& session) const;
    void handleListUserAPIKeys(const json& msg, WebSocketSession& session) const;
    void handleGetAPIKey(const json& msg, WebSocketSession& session) const;

    // All Vault commands
    void handleListVaults(const json& msg, WebSocketSession& session) const;
    void handleAddVault(const json& msg, WebSocketSession& session) const;
    void handleUpdateVault(const json& msg, WebSocketSession& session) const;
    void handleRemoveVault(const json& msg, WebSocketSession& session) const;
    void handleGetVault(const json& msg, WebSocketSession& session) const;
    void handleSyncVault(const json& msg, WebSocketSession& session) const;

  private:
    std::shared_ptr<storage::StorageManager> storageManager_;
    std::shared_ptr<crypto::APIKeyManager> apiKeyManager_;
};

} // namespace vh::websocket
