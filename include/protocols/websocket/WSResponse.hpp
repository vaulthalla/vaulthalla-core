#pragma once

#include <nlohmann/json.hpp>

namespace vh::websocket {

using json = nlohmann::json;

class WebSocketSession;

enum class WSStatus { OK, ERROR, UNAUTHORIZED, INTERNAL_ERROR };

struct WSResponse {
    std::string cmd;
    json req, data{};
    WSStatus status = WSStatus::OK;
    std::optional<std::string> error{};

    WSResponse(std::string&& cmd, json&& req, const WSStatus& status, json&& data = json{}, std::optional<std::string>&& error = std::nullopt);

    void operator()(WebSocketSession& session);

    static WSResponse SUCCESS(std::string&& cmd, json&& req, json&& data);

    static WSResponse ERROR(std::string&& cmd, json&& req, std::string&& error);

    static WSResponse UNAUTHORIZED(std::string&& cmd, json&& req);

    static WSResponse INTERNAL_ERROR(json&& req, std::string&& error = "Unknown");
};

std::string to_string(const WSStatus& status);

}
