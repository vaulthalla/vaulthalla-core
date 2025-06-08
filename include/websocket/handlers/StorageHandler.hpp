#pragma once

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace vh::storage {
    class StorageEngine;
    class StorageManager;
}

namespace vh::websocket {

    using json = nlohmann::json;

    class WebSocketSession;

    class StorageHandler {
    public:
        StorageHandler(const std::shared_ptr<vh::storage::StorageManager>& storageManager);

        // Init commands
        void handleInitLocal(const json& msg, WebSocketSession& session);
        void handleInitS3(const json& msg, WebSocketSession& session);
        void handleInitR2(const json& msg, WebSocketSession& session);

        // Mount commands
        void handleMount(const json& msg, WebSocketSession& session);
        void handleUnmount(const json& msg, WebSocketSession& session);

    private:
        std::shared_ptr<vh::storage::StorageManager> storageManager_;
    };

} // namespace vh::websocket
