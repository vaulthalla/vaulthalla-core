#pragma once

#include "concurrency/AsyncService.hpp"

#include <memory>

namespace vh::logging {
    class LogRotator;
}

namespace vh::services {

class LogRotationService final : public concurrency::AsyncService {
public:
    LogRotationService();
    ~LogRotationService() override;

    void runLoop() override;

private:
    std::unique_ptr<logging::LogRotator> appRot_, auditRot_;
};

}
