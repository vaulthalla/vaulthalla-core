#pragma once

#include <memory>

namespace vh::shell {

class Router;

void registerUserCommands(const std::shared_ptr<Router>& r);

}
