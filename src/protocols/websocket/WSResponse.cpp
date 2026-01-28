#include "protocols/websocket/WSResponse.hpp"
#include "protocols/websocket/WebSocketSession.hpp"

#include <format>

using namespace vh::websocket;

WSResponse::WSResponse(std::string&& cmd, json&& req, const WSStatus& status, json&& data, std::optional<std::string>&& error)
    : cmd(std::move(cmd)), req(std::move(req)), data(std::move(data)), status(status), error(std::move(error)) {}

void WSResponse::operator()(WebSocketSession& session) {
    json response = {
        {"command", std::format("{}.response", cmd)},
        {"status", to_string(status)},
        {"requestId", req.at("requestId").get<std::string>()},
    };

    if (!data.empty()) response["data"] = data;
    if (error) response["error"] = error;

    session.send(response);
}

WSResponse WSResponse::SUCCESS(std::string&& cmd, json&& req, json&& data) {
    return WSResponse(std::move(cmd), std::move(req), WSStatus::OK, std::move(data));
}

WSResponse WSResponse::ERROR(std::string&& cmd, json&& req, std::string&& error) {
    return {std::move(cmd), std::move(req), WSStatus::ERROR, {}, error};
}

WSResponse WSResponse::UNAUTHORIZED(std::string&& cmd, json&& req) {
    return {std::move(cmd), std::move(req), WSStatus::UNAUTHORIZED, {},
        "You must be authenticated to perform this action."};
}

WSResponse WSResponse::INTERNAL_ERROR(json&& req, std::string&& error) {
    return {std::move("Unknown"), std::move(req), WSStatus::INTERNAL_ERROR, {},
        "An internal error occurred while processing your request: " + error};
}

std::string vh::websocket::to_string(const WSStatus& status) {
    switch (status) {
        case WSStatus::OK: return "OK";
        case WSStatus::ERROR: return "ERROR";
        case WSStatus::UNAUTHORIZED: return "UNAUTHORIZED";
        default: return "UNKNOWN";
    }
}
