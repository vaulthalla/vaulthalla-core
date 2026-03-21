#pragma once

#include "protocols/shell/types.hpp"

#include <memory>
#include <regex>

namespace vh::protocols::shell {
    class Router;
}

namespace vh::protocols::shell::commands::roles {
    void registerCommands(const std::shared_ptr<Router> &r);

    CommandResult handle_admin_roles(const CommandCall &call);
    CommandResult handle_vault_roles(const CommandCall &call);
}
