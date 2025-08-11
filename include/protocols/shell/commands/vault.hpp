#pragma once

#include <memory>

namespace vh::shell {

class Router;

void registerVaultCommands(const std::shared_ptr<Router>& r);

}
