#include "FUSECmdRouter.hpp"
#include "types/fuse/Command.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

using namespace vh::fuse::ipc;

CommandRouter::CommandRouter(const std::string& socketPath)
    : socketPath_(socketPath) {
}

CommandRouter::~CommandRouter() {
    stop();
    if (serverFd_ != -1) close(serverFd_);
    unlink(socketPath_.c_str());
}

void CommandRouter::start() {
    sockaddr_un addr{};
    serverFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        perror("socket");
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    unlink(socketPath_.c_str());
    if (bind(serverFd_, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(serverFd_);
        return;
    }

    if (listen(serverFd_, 5) == -1) {
        perror("listen");
        close(serverFd_);
        return;
    }

    running_ = true;
    listenerThread_ = std::thread(&CommandRouter::listenLoop, this);
    std::cout << "[FUSE-IPC] Listening on " << socketPath_ << std::endl;
}

void CommandRouter::stop() {
    running_ = false;
    if (listenerThread_.joinable()) listenerThread_.join();
}

void CommandRouter::setCommandHandler(std::function<void(const types::fuse::Command&)> handler) {
    handler_ = std::move(handler);
}

void CommandRouter::listenLoop() {
    while (running_) {
        int clientFd = accept(serverFd_, nullptr, nullptr);
        if (clientFd == -1) {
            if (running_) perror("accept");
            continue;
        }

        char buffer[1024] = {};
        ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            std::string jsonStr(buffer);
            if (auto cmd = std::make_unique<types::fuse::Command>(types::fuse::Command::fromJson(jsonStr))) {
                if (handler_) handler_(*cmd);
            } else {
                std::cerr << "[FUSE-IPC] Failed to parse command: " << jsonStr << std::endl;
            }
        }

        close(clientFd);
    }
}