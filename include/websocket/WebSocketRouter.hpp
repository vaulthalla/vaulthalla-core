#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

namespace vh::websocket {
    class WebSocketSession;

    using json = nlohmann::json;

    class WebSocketRouter {
    public:
        using HandlerFunc = std::function<void(const json& msg, WebSocketSession& session)>;

        void registerHandler(const std::string& command, HandlerFunc handler);

        void routeMessage(const json& msg, WebSocketSession& session);

    private:
        std::unordered_map<std::string, HandlerFunc> handlers_;
    };

} // namespace vh::websocket
