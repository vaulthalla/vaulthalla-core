#pragma once

#include "protocols/shell/CommandUsage.hpp"

namespace vh::shell {

struct PermissionUsage {
    [[nodiscard]] static CommandUsage permissions();

    [[nodiscard]] static std::string usage_vault_permissions();
    [[nodiscard]] static std::string usage_user_permissions();
};

}
