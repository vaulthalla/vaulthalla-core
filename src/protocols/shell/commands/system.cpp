#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"

#include <version.h>

namespace vh::shell {

void registerSystemCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("help", "Show help info",
        [&r](auto&) -> CommandResult { return {0, r->listCommands(), ""}; },
        {"-h", "--help", "?"});

    r->registerCommand("version", "Show version information",
        [&r](const CommandCall& call) -> CommandResult {
            return {0, "Vaulthalla v" + std::string(VH_VERSION), ""};
        }, {"-v", "--version"});
}

}
