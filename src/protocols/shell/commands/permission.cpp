#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"
#include "usage/include/usages.hpp"

using namespace vh::shell;
using namespace vh::services;

static CommandResult handle_permission(const CommandCall& call) {
    if (call.positionals.empty() || call.positionals.size() > 1) return usage(call.constructFullArgs());
    const std::string_view sub = call.positionals[0];
    if (sub == "user") return ok(permissions::usage_user_permissions());
    if (sub == "vault") return ok(permissions::usage_vault_permissions());
    return invalid(call.constructFullArgs(), "Unknown permission subcommand: '" + std::string(sub) + "'");
}

void commands::registerPermissionCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    r->registerCommand(usageManager->resolve("permission"), handle_permission);
}
