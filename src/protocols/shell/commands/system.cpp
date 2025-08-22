#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/usage/SystemUsage.hpp"
#include "protocols/shell/usage/ShellUsage.hpp"

#include <version.h>

namespace vh::shell {

static CommandResult handle_help(const CommandCall& call) {
    return ok(ShellUsage::all().filterTopLevelOnly().toText());
}

static CommandResult handle_version(const CommandCall& call) {
    return {0, "Vaulthalla v" + std::string(VH_VERSION), ""};
}

void registerSystemCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand(SystemUsage::help(), handle_help);
    r->registerCommand(SystemUsage::version(), handle_version);
}

}
