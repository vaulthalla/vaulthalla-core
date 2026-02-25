#pragma once

#include "concurrency/AsyncService.hpp"

#include <chrono>

namespace vh::shell { class Router; }

namespace vh::database {

class Janitor final : public concurrency::AsyncService {
public:
    Janitor();
    ~Janitor() override = default;

protected:
    void runLoop() override;

private:
    std::chrono::minutes sweep_interval_;
};

}
