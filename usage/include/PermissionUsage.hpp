#pragma once

#include "CommandUsage.hpp"

namespace vh::shell {

struct PermissionUsage {
    [[nodiscard]] static CommandBook all();

    [[nodiscard]] static CommandUsage permissions();

    [[nodiscard]] static std::string usage_vault_permissions();
    [[nodiscard]] static std::string usage_user_permissions();
};

}
