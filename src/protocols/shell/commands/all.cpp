#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"

using namespace vh::shell;

void commands::registerAllCommands(const std::shared_ptr<Router>& r) {
    vault::registerCommands(r);
    registerAPIKeyCommands(r);
    registerSystemCommands(r);
    registerUserCommands(r);
    registerGroupCommands(r);
    registerRoleCommands(r);
    registerPermissionCommands(r);
    registerSecretsCommands(r);
}
