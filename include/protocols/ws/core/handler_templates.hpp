#pragma once

#include "protocols/ws/model/Response.hpp"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace vh::protocols::ws { class Session; }

namespace vh::protocols::ws::core {

template <class Fn>
auto makeWsHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, Session& session) mutable {
        try {
            json data = std::invoke(fn, msg, session);  // fn reads msg by ref
            model::Response::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            model::Response::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            model::Response::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
auto makePayloadHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, Session& session) mutable {
        try {
            const json& payload = msg.at("payload");      // ref into msg (safe during this call)
            json data = std::invoke(fn, payload, session); // handler returns response data
            model::Response::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            model::Response::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            model::Response::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
Router::Handler makeHandlerWithToken(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, Session& session) mutable {
        try {
            const auto& token = msg.at("token").get_ref<const std::string&>();

            json data = std::invoke(fn, token, session);

            model::Response::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            model::Response::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            model::Response::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
Router::Handler makeSessionOnlyHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
           (json&& msg, Session& session) mutable {
        try {
            json data = std::invoke(fn, session);
            model::Response::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            model::Response::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            model::Response::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

template <class Fn>
Router::Handler makeEmptyHandler(std::string cmd, Fn&& fn) {
    return [cmd = std::move(cmd), fn = std::forward<Fn>(fn)]
        (json&& msg, Session& session) mutable {
        try {
            json data = std::invoke(fn);
            model::Response::SUCCESS(std::string(cmd), std::move(msg), std::move(data))(session);
        } catch (const std::exception& e) {
            model::Response::ERROR(std::string(cmd), std::move(msg), e.what())(session);
        } catch (...) {
            model::Response::ERROR(std::string(cmd), std::move(msg), "Unknown error")(session);
        }
    };
}

// ---------------------------
// helpers to bind member functions cleanly
// ---------------------------
template <class Obj>
auto bindMember(Obj* obj, void (Obj::*mf)(json&&, Session&) const) {
    return [obj, mf](json&& msg, Session& session) {
        (obj->*mf)(std::move(msg), session);
    };
}

template <class Obj>
auto bindPayloadMember(Obj* obj, void (Obj::*mf)(const json&, json&&, Session&) const) {
    return [obj, mf](const json& payload, json&& msg, Session& session) {
        (obj->*mf)(payload, std::move(msg), session);
    };
}

}
