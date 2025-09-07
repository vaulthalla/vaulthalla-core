#pragma once

#include "UsageManager.hpp"

namespace vh::shell {
    class CommandUsage;
}

namespace vh::test {

class TestUsageManager : public shell::UsageManager {
public:
    // may phase this one out
    [[nodiscard]] std::shared_ptr<shell::CommandUsage> getFilteredTestUsage() const;


};

}
