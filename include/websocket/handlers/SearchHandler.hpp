#pragma once

#include "index/SearchIndex.hpp"
#include "websocket/WebSocketSession.hpp"
#include <memory>
#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class SearchHandler {
  public:
    explicit SearchHandler(const std::shared_ptr<vh::index::SearchIndex>& searchIndex);

    void handleSearch(const json& msg, WebSocketSession& session);

  private:
    std::shared_ptr<vh::index::SearchIndex> searchIndex_;
};

} // namespace vh::websocket
