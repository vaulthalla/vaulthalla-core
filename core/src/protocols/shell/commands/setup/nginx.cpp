#include "protocols/shell/commands/helpers.hpp"
#include "protocols/shell/commands/router.hpp"
#include "protocols/shell/util/argsHelpers.hpp"

#include <string>

namespace vh::protocols::shell::commands::setup {

CommandResult handleNginx(const CommandCall& call) {
    const auto usage = resolveUsage({"setup", "nginx"});
    validatePositionals(call, usage);
    return invalid("setup nginx: handled by the local lifecycle utility. Re-run this command via the local 'vh' CLI binary.");
}

}
