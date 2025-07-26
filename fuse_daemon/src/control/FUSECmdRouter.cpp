#include "control/FUSECmdRouter.hpp"
#include "types/FUSECommand.hpp"
#include <fcntl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

using namespace vh::fuse::ipc;

CommandRouter::CommandRouter(const std::string& socketPath) : socketPath_(socketPath) {}

CommandRouter::~CommandRouter() {
    stop();
    if (serverFd_ != -1) close(serverFd_);
    unlink(socketPath_.c_str());
}

void CommandRouter::start() {
    sockaddr_un addr{};
    serverFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
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
        serverFd_ = -1;
        return;
    }

    if (listen(serverFd_, 5) == -1) {
        perror("listen");
        close(serverFd_);
        serverFd_ = -1;
        return;
    }

    running_.store(true);
    listenerThread_ = std::thread(&CommandRouter::listenLoop, this);
    std::cout << "[FUSE-IPC] Listening on " << socketPath_ << std::endl;
}

void CommandRouter::stop() {
    if (!running_.exchange(false)) return;  // only stop once
    unblockAccept(); // force accept() to unblock
    if (listenerThread_.joinable()) listenerThread_.join();
}

void CommandRouter::unblockAccept() {
    // Connect to our own socket to unblock accept()
    int dummy = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dummy >= 0) {
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
        connect(dummy, (sockaddr*)&addr, sizeof(addr));
        close(dummy);
    }
}

void CommandRouter::setCommandHandler(std::function<void(const types::fuse::FUSECommand&)> handler) {
    handler_ = std::move(handler);
}

void CommandRouter::listenLoop() {
    while (running_.load()) {
        int clientFd = accept(serverFd_, nullptr, nullptr);
        if (clientFd == -1) {
            if (running_) perror("[FUSE-IPC] accept");
            continue;
        }

        char buffer[1024] = {};
        ssize_t bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            try {
                std::string jsonStr(buffer);
                auto cmd = types::fuse::FUSECommand::fromJson(jsonStr);
                if (handler_) handler_(cmd);
            } catch (const std::exception& ex) {
                std::cerr << "[FUSE-IPC] Failed to parse command: " << ex.what() << "\n";
            }
        }

        close(clientFd);
    }
}
