#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "CommandUsage.hpp"

void vh::protocols::shell::commands::registerAllCommands(const std::shared_ptr<Router>& r) {
    vault::registerCommands(r);
    registerAPIKeyCommands(r);
    registerSystemCommands(r);
    registerUserCommands(r);
    registerGroupCommands(r);
    registerRoleCommands(r);
    registerPermissionCommands(r);
    registerSecretsCommands(r);
}
