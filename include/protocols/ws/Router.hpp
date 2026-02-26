#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>

namespace vh::protocols::ws {
class Session;

using json = nlohmann::json;

class Router {
  public:
    // Stored handler type: router owns msg lifecycle and wrapper sends WSResponse
    using Handler = std::function<void(json&&, Session&)>;

    // "Raw" callables that return response data
    using RawWsHandler         = std::function<json(const json& req, Session&)>;                // sees full req
    using RawPayloadHandler    = std::function<json(const json& payload, Session&)>;            // payload only
    using RawHandlerWithToken  = std::function<json(const std::string& token, Session&)>;       // token only
    using RawSessionOnly       = std::function<json(Session&)>;                                 // session only
    using RawEmpty             = std::function<json()>;                                                  // nothing

    // Primary registration APIs (no duplication of cmd in wrappers)
    void registerWs(const std::string& cmd, RawWsHandler fn);
    void registerPayload(const std::string& cmd, RawPayloadHandler fn);
    void registerHandlerWithToken(const std::string& cmd, RawHandlerWithToken fn);
    void registerSessionOnlyHandler(const std::string& cmd, RawSessionOnly fn);
    void registerEmptyHandler(const std::string& cmd, RawEmpty fn);

    // Escape hatch: register an already-wrapped handler
    void registerHandler(const std::string& cmd, Handler h);

    void routeMessage(json&& msg, Session& session);

    // ---------------------------
    // Member-function-pointer overloads (NO lambdas at call sites)
    // ---------------------------

    // Payload: json Obj::method(const json&, WebSocketSession&)
    template <class Obj>
    void registerPayload(std::string cmd, Obj* obj, json (Obj::*mf)(const json&, Session&)) {
        registerPayload(std::move(cmd),
            [obj, mf](const json& payload, Session& session) {
                return (obj->*mf)(payload, session);
            });
    }

    // Payload: json Obj::method(const json&, const WebSocketSession&)
    template <class Obj>
    void registerPayload(std::string cmd, Obj* obj, json (Obj::*mf)(const json&, const Session&)) {
        registerPayload(std::move(cmd),
            [obj, mf](const json& payload, Session& session) {
                return (obj->*mf)(payload, static_cast<const Session&>(session));
            });
    }

    // Token: json Obj::method(const std::string&, WebSocketSession&)
    template <class Obj>
    void registerHandlerWithToken(std::string cmd, Obj* obj, json (Obj::*mf)(const std::string&, Session&)) {
        registerHandlerWithToken(std::move(cmd),
            [obj, mf](const std::string& token, Session& session) {
                return (obj->*mf)(token, session);
            });
    }

    // Token: json Obj::method(const std::string&, const WebSocketSession&)
    template <class Obj>
    void registerHandlerWithToken(std::string cmd, Obj* obj, json (Obj::*mf)(const std::string&, const Session&)) {
        registerHandlerWithToken(std::move(cmd),
            [obj, mf](const std::string& token, Session& session) {
                return (obj->*mf)(token, static_cast<const Session&>(session));
            });
    }

    // Session-only: json Obj::method(WebSocketSession&)
    template <class Obj>
    void registerSessionOnlyHandler(std::string cmd, Obj* obj, json (Obj::*mf)(Session&)) {
        registerSessionOnlyHandler(std::move(cmd),
            [obj, mf](Session& session) {
                return (obj->*mf)(session);
            });
    }

    // Session-only: json Obj::method(const WebSocketSession&)
    template <class Obj>
    void registerSessionOnlyHandler(std::string cmd, Obj* obj, json (Obj::*mf)(const Session&)) {
        registerSessionOnlyHandler(std::move(cmd),
            [obj, mf](Session& session) {
                return (obj->*mf)(static_cast<const Session&>(session));
            });
    }

    // Empty: json Obj::method()
    template <class Obj>
    void registerEmptyHandler(std::string cmd, Obj* obj, json (Obj::*mf)()) {
        registerEmptyHandler(std::move(cmd),
            [obj, mf]() {
                return (obj->*mf)();
            });
    }

    // Empty: json Obj::method() const
    template <class Obj>
    void registerEmptyHandler(std::string cmd, const Obj* obj, json (Obj::*mf)() const) {
        registerEmptyHandler(std::move(cmd),
            [obj, mf]() {
                return (obj->*mf)();
            });
    }

        // ---------------------------
    // Overloads for static / free function handlers
    // (i.e., json (*)(...), or static json Class::fn(...)
    // ---------------------------

    // Full request (if you ever use it): json fn(const json& req, WebSocketSession&)
    void registerWs(const std::string& cmd, json (*fn)(const json& req, Session&)) {
        registerWs(cmd, RawWsHandler{[fn](const json& req, Session& session) {
            return fn(req, session);
        }});
    }

    // Payload: json fn(const json& payload, WebSocketSession&)
    void registerPayload(const std::string& cmd, json (*fn)(const json& payload, Session&)) {
        registerPayload(cmd, RawPayloadHandler{[fn](const json& payload, Session& session) {
            return fn(payload, session);
        }});
    }

    // Payload with const session: json fn(const json& payload, const WebSocketSession&)
    void registerPayload(const std::string& cmd, json (*fn)(const json& payload, const Session&)) {
        registerPayload(cmd, RawPayloadHandler{[fn](const json& payload, Session& session) {
            return fn(payload, static_cast<const Session&>(session));
        }});
    }

    // Token: json fn(const std::string& token, WebSocketSession&)
    void registerHandlerWithToken(const std::string& cmd, json (*fn)(const std::string& token, Session&)) {
        registerHandlerWithToken(cmd, RawHandlerWithToken{[fn](const std::string& token, Session& session) {
            return fn(token, session);
        }});
    }

    // Token with const session: json fn(const std::string& token, const WebSocketSession&)
    void registerHandlerWithToken(const std::string& cmd, json (*fn)(const std::string& token, const Session&)) {
        registerHandlerWithToken(cmd, RawHandlerWithToken{[fn](const std::string& token, Session& session) {
            return fn(token, static_cast<const Session&>(session));
        }});
    }

    // Session-only: json fn(WebSocketSession&)
    void registerSessionOnlyHandler(const std::string& cmd, json (*fn)(Session&)) {
        registerSessionOnlyHandler(cmd, RawSessionOnly{[fn](Session& session) {
            return fn(session);
        }});
    }

    // Session-only const: json fn(const WebSocketSession&)
    void registerSessionOnlyHandler(const std::string& cmd, json (*fn)(const Session&)) {
        registerSessionOnlyHandler(cmd, RawSessionOnly{[fn](Session& session) {
            return fn(static_cast<const Session&>(session));
        }});
    }

    // Empty: json fn()
    void registerEmptyHandler(const std::string& cmd, json (*fn)()) {
        registerEmptyHandler(cmd, RawEmpty{[fn]() {
            return fn();
        }});
    }


  private:
    std::unordered_map<std::string, Handler> handlers_;
};

}
