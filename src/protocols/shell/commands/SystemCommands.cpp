#include "protocols/shell/commands/SystemCommands.hpp"
#include "protocols/shell/Router.hpp"

namespace vh::shell {

void registerSystemCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("help", "Show help info",
        [&r](auto&) { r->listCommands(); },
        {"-h", "--help", "?"});
}

}
