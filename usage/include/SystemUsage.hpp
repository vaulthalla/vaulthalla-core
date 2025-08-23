#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

class SystemUsage {
public:
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage help();
    [[nodiscard]] static CommandUsage version();
};

}
