#pragma once

#include <memory>
#include <nlohmann/json.hpp>

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
    void handleRemoveVault(const json& msg, WebSocketSession& session) const;
    void handleGetVault(const json& msg, WebSocketSession& session) const;

    // Volume management
    void handleAddVolume(const json& msg, WebSocketSession& session) const;
    void handleRemoveVolume(const json& msg, WebSocketSession& session) const;
    void handleListUserVolumes(const json& msg, WebSocketSession& session);
    void handleListVaultVolumes(const json& msg, WebSocketSession& session);
    void handleListVolumes(const json& msg, WebSocketSession& session);
    void handleGetVolume(const json& msg, WebSocketSession& session) const;

  private:
    std::shared_ptr<storage::StorageManager> storageManager_;
    std::shared_ptr<keys::APIKeyManager> apiKeyManager_;
};

} // namespace vh::websocket
