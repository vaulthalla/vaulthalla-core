#pragma once

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::share {
    class LinkResolver;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class ShareHandler {
    public:
        explicit ShareHandler(const std::shared_ptr<vh::share::LinkResolver>& linkResolver);

        void handleCreateLink(const json& msg, WebSocketSession& session);
        void handleResolveLink(const json& msg, WebSocketSession& session);

    private:
        std::shared_ptr<vh::share::LinkResolver> linkResolver_;
    };

} // namespace vh::websocket
