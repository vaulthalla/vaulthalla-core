#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/util/argsHelpers.hpp"
#include "runtime/Deps.hpp"
#include "protocols/shell/usage/UsageManager.hpp"
#include "protocols/shell/usage/CommandUsage.hpp"
#include "protocols/shell/usage/usages.hpp"

using namespace vh;
using namespace vh::protocols::shell;

static CommandResult handle_permission(const CommandCall& call) {
    if (call.positionals.empty() || call.positionals.size() > 1) return getUsage(call.constructFullArgs());
    const std::string_view sub = call.positionals[0];
    if (sub == "user" || sub == "u") return ok(usage::permissions::usage_admin_permissions());
    if (sub == "vault" || sub == "v") return ok(usage::permissions::usage_vault_permissions());
    return invalid(call.constructFullArgs(), "Unknown permission subcommand: '" + std::string(sub) + "'");
}

void commands::registerPermissionCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = runtime::Deps::get().shellUsageManager;
    r->registerCommand(usageManager->resolve("permission"), handle_permission);
}
