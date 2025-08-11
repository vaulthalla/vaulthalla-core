#pragma once

#include <memory>

namespace vh::shell {

class Router;

void registerSystemCommands(const std::shared_ptr<Router>& r);

}
