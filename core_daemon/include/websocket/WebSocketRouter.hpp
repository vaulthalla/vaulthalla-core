#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace vh::auth {
class SessionManager;
}

namespace vh::websocket {
class WebSocketSession;

using json = nlohmann::json;

class WebSocketRouter {
  public:
    using HandlerFunc = std::function<void(const json& msg, WebSocketSession& session)>;

    explicit WebSocketRouter(const std::shared_ptr<vh::auth::SessionManager>& sessionManager);

    void registerHandler(const std::string& command, HandlerFunc handler);

    void routeMessage(const json& msg, WebSocketSession& session);

  private:
    std::unordered_map<std::string, HandlerFunc> handlers_;
    std::shared_ptr<vh::auth::SessionManager> sessionManager_;
};

} // namespace vh::websocket
