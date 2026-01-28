#pragma once

#include <memory>

namespace vh::websocket {

class WebSocketRouter;

class WebSocketHandler {
  public:
    explicit WebSocketHandler(const std::shared_ptr<WebSocketRouter>& router);

    void registerAllHandlers() const;

  private:
    std::shared_ptr<WebSocketRouter> router_;

    void registerAuthHandlers() const;
    void registerFileSystemHandlers() const;
    void registerStorageHandlers() const;
    void registerAPIKeyHandlers() const;
    void registerRoleHandlers() const;
    void registerPermissionsHandlers() const;
    void registerSettingsHandlers() const;
    void registerGroupHandlers() const;
    void registerStatHandlers() const;
};

}
