#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/commands/helpers.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/util/argsHelpers.hpp"
#include "runtime/Deps.hpp"
#include "usage/include/UsageManager.hpp"
#include "identities/User.hpp"

#include <string>
#include <string_view>

namespace vh::protocols::shell::commands {

namespace {

constexpr auto* kLifecycleHint =
    "handled by the local lifecycle utility. Re-run this command via the local 'vh' CLI binary.";

bool isTeardownMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"teardown", cmd}, input);
}

CommandResult handleTeardownNginx(const CommandCall& call) {
    const auto usage = resolveUsage({"teardown", "nginx"});
    validatePositionals(call, usage);
    if (!call.user->isSuperAdmin())
        return invalid("teardown nginx: requires super_admin role");
    return invalid("teardown nginx: " + std::string(kLifecycleHint));
}

CommandResult handleTeardownDb(const CommandCall& call) {
    const auto usage = resolveUsage({"teardown", "db"});
    validatePositionals(call, usage);
    if (!call.user->isSuperAdmin())
        return invalid("teardown db: requires super_admin role");
    return invalid("teardown db: " + std::string(kLifecycleHint));
}

CommandResult handleTeardown(const CommandCall& call) {
    if (call.positionals.empty() || hasKey(call, "help") || hasKey(call, "h"))
        return usage(call.constructFullArgs());

    const auto [sub, subcall] = descend(call);
    if (isTeardownMatch("nginx", sub)) return handleTeardownNginx(subcall);
    if (isTeardownMatch("db", sub)) return handleTeardownDb(subcall);

    return invalid(call.constructFullArgs(), "Unknown teardown subcommand: '" + std::string(sub) + "'");
}

}

void registerTeardownCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = runtime::Deps::get().shellUsageManager;
    r->registerCommand(usageManager->resolve("teardown"), handleTeardown);
}

}
