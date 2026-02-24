#pragma once

#include "concurrency/AsyncService.hpp"

#include <chrono>

namespace vh::shell { class Router; }

namespace vh::services {

class DBSweeper final : public concurrency::AsyncService {
public:
    DBSweeper();
    ~DBSweeper() override = default;

protected:
    void runLoop() override;

private:
    std::chrono::minutes sweep_interval_;
};

}
