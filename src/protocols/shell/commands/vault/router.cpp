#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "VaultUsage.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <memory>

using namespace vh::shell;
using namespace vh::shell::commands;
using namespace vh::shell::commands::vault;
using namespace vh::types;
using namespace vh::services;

static CommandResult handle_vault(const CommandCall& call) {
    if (call.positionals.empty() || hasKey(call, "help") || hasKey(call, "h"))
        return ok(VaultUsage::all().str());

    const std::string_view sub = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (sub == "sync") return handle_sync(subcall);
    if (sub == "create" || sub == "new") return handle_vault_create(subcall);
    if (sub == "delete" || sub == "rm") return handle_vault_delete(subcall);
    if (sub == "info" || sub == "get") return handle_vault_info(subcall);
    if (sub == "update" || sub == "set") return handle_vault_update(subcall);
    if (sub == "role" || sub == "r") return handle_vault_role(subcall);
    if (sub == "keys") return handle_vault_keys(subcall);

    return ok(VaultUsage::all().str());
}

static CommandResult handle_vaults(const CommandCall& call) {
    if (hasKey(call, "help") || hasKey(call, "h")) return ok(VaultUsage::vaults_list().str());
    return handle_vaults_list(call);
}

void vault::registerCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand(VaultUsage::vault(), handle_vault);
    r->registerCommand(VaultUsage::vaults_list(), handle_vaults);
}
