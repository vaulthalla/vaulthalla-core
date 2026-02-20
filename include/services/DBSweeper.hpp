#pragma once

#include "services/AsyncService.hpp"

#include <chrono>

namespace vh::shell { class Router; }

namespace vh::services {

class DBSweeper final : public AsyncService {
public:
    DBSweeper();
    ~DBSweeper() override = default;

protected:
    void runLoop() override;

private:
    std::chrono::minutes sweep_interval_;
};

}
