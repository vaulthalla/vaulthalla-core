#pragma once

#include "services/AsyncService.hpp"

#include <memory>

namespace vh::logging {
    class LogRotator;
}

namespace vh::services {

class LogRotationService final : public AsyncService {
public:
    LogRotationService();
    ~LogRotationService() override;

    void runLoop() override;

private:
    std::unique_ptr<logging::LogRotator> appRot_, auditRot_;
};

}
