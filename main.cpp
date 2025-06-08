#include <iostream>
#include "websocket/WebSocketRouter.hpp"

int main() {
    auto router = std::make_shared<vh::websocket::WebSocketRouter>();

    auto fsHandler = std::make_shared<vh::websocket::FileSystemHandler>();
    auto storageHandler = std::make_shared<vh::websocket::StorageHandler>();

// Register FS routes
    router->registerHandler("fs.listDir", [fsHandler](const json& msg, WebSocketSession& session) {
        fsHandler->handleListDir(msg, session);
    });

    router->registerHandler("fs.readFile", [fsHandler](const json& msg, WebSocketSession& session) {
        fsHandler->handleReadFile(msg, session);
    });

    router->registerHandler("fs.writeFile", [fsHandler](const json& msg, WebSocketSession& session) {
        fsHandler->handleWriteFile(msg, session);
    });

    router->registerHandler("fs.deleteFile", [fsHandler](const json& msg, WebSocketSession& session) {
        fsHandler->handleDeleteFile(msg, session);
    });

// Register Storage routes
    router->registerHandler("storage.local.init", [storageHandler](const json& msg, WebSocketSession& session) {
        storageHandler->handleInitLocal(msg, session);
    });

    router->registerHandler("storage.s3.init", [storageHandler](const json& msg, WebSocketSession& session) {
        storageHandler->handleInitS3(msg, session);
    });

    router->registerHandler("storage.r2.init", [storageHandler](const json& msg, WebSocketSession& session) {
        storageHandler->handleInitR2(msg, session);
    });

// ... and so on for mount/unmount commands

// Now wire the WebSocketServer with this router
    auto server = std::make_shared<vh::websocket::WebSocketServer>(ioc, tcp::endpoint(tcp::v4(), 9002), *router);
    server->run();
}
