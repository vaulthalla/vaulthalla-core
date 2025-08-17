#pragma once

#include "services/AsyncService.hpp"
#include <memory>
#include <string>

namespace vh::shell { class Router; }

namespace vh::services {

class CtlServerService final : public AsyncService {
public:
    CtlServerService();
    ~CtlServerService() override;

protected:
    void runLoop() override;
    void onStop(); // close listener to break accept()

private:
    std::shared_ptr<shell::Router> router_;
    std::string socketPath_;
    unsigned adminGid_;
    int listenFd_ = -1;

    void closeListener();
};

}
