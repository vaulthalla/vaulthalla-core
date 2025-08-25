#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

struct SecretsUsage {
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage secrets();
    [[nodiscard]] static CommandUsage secrets_set();
    [[nodiscard]] static CommandUsage secrets_export();

    [[nodiscard]] static CommandUsage buildBaseUsage_();
};

}
