#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

struct ShellUsage {
    [[nodiscard]] static CommandBook all();
};

}
