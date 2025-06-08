#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::storage {
    class StorageEngine;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class StorageHandler {
    public:
        StorageHandler();

        // Init commands
        void handleInitLocal(const json& msg, WebSocketSession& session);
        void handleInitS3(const json& msg, WebSocketSession& session);
        void handleInitR2(const json& msg, WebSocketSession& session);

        // Mount commands
        void handleMount(const json& msg, WebSocketSession& session);
        void handleUnmount(const json& msg, WebSocketSession& session);

        // Access active mounts
        std::shared_ptr<vh::storage::StorageEngine> getMount(const std::string& mountName);

    private:
        std::unordered_map<std::string, std::shared_ptr<vh::storage::StorageEngine>> mounts_;
        std::mutex mountsMutex_;
    };

} // namespace vh::websocket
