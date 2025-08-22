#pragma once

#include "protocols/shell/CommandUsage.hpp"

namespace vh::shell {

struct ShellUsage {
    [[nodiscard]] static CommandBook all();
};

}
