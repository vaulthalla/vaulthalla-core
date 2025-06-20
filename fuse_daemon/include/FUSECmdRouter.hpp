#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>

namespace vh::types::fuse {
struct Command;
}

namespace vh::fuse::ipc {

class CommandRouter {
  public:
    explicit CommandRouter(const std::string& socketPath);

    ~CommandRouter();

    void start();

    void stop();

    void setCommandHandler(std::function<void(const types::fuse::Command&)> handler);

  private:
    std::string socketPath_;
    int serverFd_ = -1;
    bool running_ = false;
    std::thread listenerThread_;
    std::function<void(const types::fuse::Command&)> handler_;

    void listenLoop();
};

} // namespace vh::fuse::ipc
