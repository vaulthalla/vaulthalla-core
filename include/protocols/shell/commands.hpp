#pragma once

#include <memory>

namespace vh::shell {

class Router;

void registerAPIKeyCommands(const std::shared_ptr<Router>& r);
void registerSystemCommands(const std::shared_ptr<Router>& r);
void registerUserCommands(const std::shared_ptr<Router>& r);
void registerVaultCommands(const std::shared_ptr<Router>& r);
void registerGroupCommands(const std::shared_ptr<Router>& r);
void registerRoleCommands(const std::shared_ptr<Router>& r);
void registerPermissionCommands(const std::shared_ptr<Router>& r);
void registerSecretsCommands(const std::shared_ptr<Router>& r);

inline void registerAllCommands(const std::shared_ptr<Router>& r) {
    registerAPIKeyCommands(r);
    registerSystemCommands(r);
    registerUserCommands(r);
    registerVaultCommands(r);
    registerGroupCommands(r);
    registerRoleCommands(r);
    registerPermissionCommands(r);
    registerSecretsCommands(r);
}

}
