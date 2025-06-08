#pragma once

#include "websocket/WebSocketSession.hpp"
#include "index/SearchIndex.hpp"

#include <nlohmann/json.hpp>
#include <memory>

namespace vh::websocket {

    using json = nlohmann::json;

    class SearchHandler {
    public:
        explicit SearchHandler(std::shared_ptr<vh::index::SearchIndex> searchIndex);

        void handleSearch(const json& msg, WebSocketSession& session);

    private:
        std::shared_ptr<vh::index::SearchIndex> searchIndex_;
    };

} // namespace vh::websocket
