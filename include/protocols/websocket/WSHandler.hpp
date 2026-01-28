#pragma once

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <nlohmann/json.hpp>

#include "protocols/websocket/WSResponse.hpp"

using json = nlohmann::json;

namespace vh::websocket {

struct WebSocketSession;

template <class Fn>
auto makeWsHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, WebSocketSession& session) mutable {
        try {
            json data = std::invoke(fn, msg, session);  // fn reads msg by ref
            WSResponse::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
auto makePayloadHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, WebSocketSession& session) mutable {
        try {
            const json& payload = msg.at("payload");      // ref into msg (safe during this call)
            json data = std::invoke(fn, payload, session); // handler returns response data
            WSResponse::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
WebSocketRouter::Handler makeHandlerWithToken(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, WebSocketSession& session) mutable {
        try {
            const auto& token = msg.at("token").get_ref<const std::string&>();

            json data = std::invoke(fn, token, session);

            WSResponse::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
WebSocketRouter::Handler makeSessionOnlyHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, WebSocketSession& session) mutable {
        try {
            json data = std::invoke(fn, session);
            WSResponse::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
WebSocketRouter::Handler makeEmptyHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
        (json&& msg, WebSocketSession& session) mutable {
        try {
            json data = std::invoke(fn);
            WSResponse::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            WSResponse::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

// ---------------------------
// helpers to bind member functions cleanly
// ---------------------------
template <class Obj>
auto bindMember(Obj* obj, void (Obj::*mf)(json&&, WebSocketSession&) const) {
    return [obj, mf](json&& msg, WebSocketSession& session) {
        (obj->*mf)(std::move(msg), session);
    };
}

template <class Obj>
auto bindPayloadMember(Obj* obj, void (Obj::*mf)(const json&, json&&, WebSocketSession&) const) {
    return [obj, mf](const json& payload, json&& msg, WebSocketSession& session) {
        (obj->*mf)(payload, std::move(msg), session);
    };
}

} // namespace ws
