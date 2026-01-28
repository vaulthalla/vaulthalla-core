#pragma once

#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>

namespace vh::auth {
class SessionManager;
}

namespace vh::websocket {
class WebSocketSession;

using json = nlohmann::json;

class WebSocketRouter {
  public:
    // Stored handler type: router owns msg lifecycle and wrapper sends WSResponse
    using Handler = std::function<void(json&&, WebSocketSession&)>;

    // "Raw" callables that return response data
    using RawWsHandler         = std::function<json(const json& req, WebSocketSession&)>;                // sees full req
    using RawPayloadHandler    = std::function<json(const json& payload, WebSocketSession&)>;            // payload only
    using RawHandlerWithToken  = std::function<json(const std::string& token, WebSocketSession&)>;       // token only
    using RawSessionOnly       = std::function<json(WebSocketSession&)>;                                 // session only
    using RawEmpty             = std::function<json()>;                                                  // nothing

    explicit WebSocketRouter();

    // Primary registration APIs (no duplication of cmd in wrappers)
    void registerWs(std::string cmd, RawWsHandler fn);
    void registerPayload(std::string cmd, RawPayloadHandler fn);
    void registerHandlerWithToken(std::string cmd, RawHandlerWithToken fn);
    void registerSessionOnlyHandler(std::string cmd, RawSessionOnly fn);
    void registerEmptyHandler(std::string cmd, RawEmpty fn);

    // Escape hatch: register an already-wrapped handler
    void registerHandler(std::string cmd, Handler h);

    void routeMessage(json&& msg, WebSocketSession& session);

    // ---------------------------
    // Member-function-pointer overloads (NO lambdas at call sites)
    // ---------------------------

    // Payload: json Obj::method(const json&, WebSocketSession&)
    template <class Obj>
    void registerPayload(std::string cmd, Obj* obj, json (Obj::*mf)(const json&, WebSocketSession&)) {
        registerPayload(std::move(cmd),
            [obj, mf](const json& payload, WebSocketSession& session) {
                return (obj->*mf)(payload, session);
            });
    }

    // Payload: json Obj::method(const json&, const WebSocketSession&)
    template <class Obj>
    void registerPayload(std::string cmd, Obj* obj, json (Obj::*mf)(const json&, const WebSocketSession&)) {
        registerPayload(std::move(cmd),
            [obj, mf](const json& payload, WebSocketSession& session) {
                return (obj->*mf)(payload, static_cast<const WebSocketSession&>(session));
            });
    }

    // Token: json Obj::method(const std::string&, WebSocketSession&)
    template <class Obj>
    void registerHandlerWithToken(std::string cmd, Obj* obj, json (Obj::*mf)(const std::string&, WebSocketSession&)) {
        registerHandlerWithToken(std::move(cmd),
            [obj, mf](const std::string& token, WebSocketSession& session) {
                return (obj->*mf)(token, session);
            });
    }

    // Token: json Obj::method(const std::string&, const WebSocketSession&)
    template <class Obj>
    void registerHandlerWithToken(std::string cmd, Obj* obj, json (Obj::*mf)(const std::string&, const WebSocketSession&)) {
        registerHandlerWithToken(std::move(cmd),
            [obj, mf](const std::string& token, WebSocketSession& session) {
                return (obj->*mf)(token, static_cast<const WebSocketSession&>(session));
            });
    }

    // Session-only: json Obj::method(WebSocketSession&)
    template <class Obj>
    void registerSessionOnlyHandler(std::string cmd, Obj* obj, json (Obj::*mf)(WebSocketSession&)) {
        registerSessionOnlyHandler(std::move(cmd),
            [obj, mf](WebSocketSession& session) {
                return (obj->*mf)(session);
            });
    }

    // Session-only: json Obj::method(const WebSocketSession&)
    template <class Obj>
    void registerSessionOnlyHandler(std::string cmd, Obj* obj, json (Obj::*mf)(const WebSocketSession&)) {
        registerSessionOnlyHandler(std::move(cmd),
            [obj, mf](WebSocketSession& session) {
                return (obj->*mf)(static_cast<const WebSocketSession&>(session));
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
    void registerWs(const std::string& cmd, json (*fn)(const json& req, WebSocketSession&)) {
        registerWs(cmd, RawWsHandler{[fn](const json& req, WebSocketSession& session) {
            return fn(req, session);
        }});
    }

    // Payload: json fn(const json& payload, WebSocketSession&)
    void registerPayload(const std::string& cmd, json (*fn)(const json& payload, WebSocketSession&)) {
        registerPayload(cmd, RawPayloadHandler{[fn](const json& payload, WebSocketSession& session) {
            return fn(payload, session);
        }});
    }

    // Payload with const session: json fn(const json& payload, const WebSocketSession&)
    void registerPayload(const std::string& cmd, json (*fn)(const json& payload, const WebSocketSession&)) {
        registerPayload(cmd, RawPayloadHandler{[fn](const json& payload, WebSocketSession& session) {
            return fn(payload, static_cast<const WebSocketSession&>(session));
        }});
    }

    // Token: json fn(const std::string& token, WebSocketSession&)
    void registerHandlerWithToken(const std::string& cmd, json (*fn)(const std::string& token, WebSocketSession&)) {
        registerHandlerWithToken(cmd, RawHandlerWithToken{[fn](const std::string& token, WebSocketSession& session) {
            return fn(token, session);
        }});
    }

    // Token with const session: json fn(const std::string& token, const WebSocketSession&)
    void registerHandlerWithToken(const std::string& cmd, json (*fn)(const std::string& token, const WebSocketSession&)) {
        registerHandlerWithToken(cmd, RawHandlerWithToken{[fn](const std::string& token, WebSocketSession& session) {
            return fn(token, static_cast<const WebSocketSession&>(session));
        }});
    }

    // Session-only: json fn(WebSocketSession&)
    void registerSessionOnlyHandler(const std::string& cmd, json (*fn)(WebSocketSession&)) {
        registerSessionOnlyHandler(cmd, RawSessionOnly{[fn](WebSocketSession& session) {
            return fn(session);
        }});
    }

    // Session-only const: json fn(const WebSocketSession&)
    void registerSessionOnlyHandler(const std::string& cmd, json (*fn)(const WebSocketSession&)) {
        registerSessionOnlyHandler(cmd, RawSessionOnly{[fn](WebSocketSession& session) {
            return fn(static_cast<const WebSocketSession&>(session));
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
    std::shared_ptr<auth::SessionManager> sessionManager_;
};

}
