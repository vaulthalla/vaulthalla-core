#pragma once

#include <memory>

namespace vh::shell {

class Router;

void registerAPIKeyCommands(const std::shared_ptr<Router>& r);

}
