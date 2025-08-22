#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "protocols/shell/usage/PermissionUsage.hpp"

using namespace vh::shell;

static CommandResult handle_permission(const CommandCall& call) {
    if (call.positionals.empty()) return ok(PermissionUsage::usage_user_permissions() + "\n" + PermissionUsage::usage_vault_permissions());
    if (call.positionals.size() > 1) return invalid("permissions: too many arguments\n\n" + PermissionUsage::permissions().str());
    const std::string_view sub = call.positionals[0];
    if (sub == "user") return ok(PermissionUsage::usage_user_permissions());
    if (sub == "vault") return ok(PermissionUsage::usage_vault_permissions());
    return invalid("permissions: unrecognized argument '" + std::string(sub) + "'\n\n" + PermissionUsage::permissions().str());
}

void vh::shell::registerPermissionCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand(PermissionUsage::permissions(), handle_permission);
}
