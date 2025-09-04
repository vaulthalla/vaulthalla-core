#pragma once

#include "services/AsyncService.hpp"

namespace vh::services {

struct LogRotationService final : AsyncService {
    LogRotationService();
    ~LogRotationService() override;

    void runLoop() override;
};

}
