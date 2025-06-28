#include "websocket/handlers/SearchHandler.hpp"
#include "types/db/User.hpp"
#include <iostream>

namespace vh::websocket {

SearchHandler::SearchHandler(const std::shared_ptr<index::SearchIndex>& searchIndex) : searchIndex_(searchIndex) {
    if (!searchIndex_) throw std::invalid_argument("SearchIndex cannot be null");
}

void SearchHandler::handleSearch(const json& msg, WebSocketSession& session) {
    try {
        const auto user = session.getAuthenticatedUser();
        if (!user) {
            throw std::runtime_error("Unauthorized");
        }

        std::string query = msg.at("query").get<std::string>();

        // Future: multi-mount search, filter by mountName
        const auto results = searchIndex_->search(query);

        json resultPaths = json::array();
        for (const auto& path : results) {
            resultPaths.push_back(path.string());
        }

        const json response = {{"command", "index.search.response"},
                         {"status", "ok"},
                         {"query", query},
                         {"results", resultPaths}};

        session.send(response);

        std::cout << "[SearchHandler] User '" << user->email << "' performed search: '" << query << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[SearchHandler] handleSearch error: " << e.what() << std::endl;

        const json response = {{"command", "index.search.response"}, {"status", "error"}, {"error", e.what()}};

        session.send(response);
    }
}

} // namespace vh::websocket
