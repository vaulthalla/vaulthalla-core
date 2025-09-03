#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"

#include "services/ServiceDepsRegistry.hpp"
#include "services/LogRegistry.hpp"
#include "database/Queries/VaultQueries.hpp"

#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"

#include "types/Vault.hpp"
#include "types/DBQueryParams.hpp"
#include "types/VaultRole.hpp"
#include "types/User.hpp"

#include "config/ConfigRegistry.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>
#include <memory>
#include <utility>

using namespace vh::shell::commands::vault;
using namespace vh::shell::commands;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;
using namespace vh::crypto;
using namespace vh::util;
using namespace vh::logging;
using namespace vh::cloud;

CommandResult commands::vault::handle_vault_info(const CommandCall& call) {
    constexpr const auto* ERR = "vault info";

    if (call.positionals.empty()) return invalid("vault info: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault info: too many arguments");

    const std::string vaultArg = call.positionals[0];

    const auto vLkp = resolveVault(call, vaultArg, ERR);
    if (!vLkp || !vLkp.ptr) return invalid(vLkp.error);
    const auto vault = vLkp.ptr;

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id) return invalid(
        "vault info: you do not have permission to view this vault");

    if (!call.user->isAdmin() && !call.user->canListVaultData(vault->id)) return invalid(
        "vault info: you do not have permission to view this vault's data");

    return ok(to_string(vault));
}

CommandResult commands::vault::handle_vaults_list(const CommandCall& call) {
    const bool f_local = hasFlag(call, "local");
    const bool f_s3 = hasFlag(call, "s3");
    if (f_local && f_s3) return invalid("vaults: --local and --s3 are mutually exclusive");
    if (!call.positionals.empty()) return invalid("vaults: unexpected positional arguments");

    int limit = 100;
    if (const auto lim = optVal(call, "limit")) {
        if (lim->empty()) return invalid("vaults: --limit requires a value");
        const auto parsed = parseInt(*lim);
        if (!parsed || *parsed <= 0) return invalid("vaults: --limit must be a positive integer");
        limit = *parsed;
    }

    const auto user = call.user;
    const bool listAll = user->isAdmin() || user->canManageVaults();

    DBQueryParams p{
        .sortBy = "created_at",
        .order = SortOrder::DESC,
        .limit = limit,
        .offset = 0, // TODO: handle pagination if needed
    };

    auto vaults = listAll
                      ? VaultQueries::listVaults(std::move(p))
                      : VaultQueries::listUserVaults(user->id, std::move(p));

    if (!listAll) {
        for (const auto& r : call.user->roles) {
            if (r->canList({})) {
                const auto vault = ServiceDepsRegistry::instance().storageManager->getEngine(r->vault_id)->vault;
                if (!vault) continue;
                if (vault->owner_id == user->id) continue; // Already added
                vaults.push_back(vault);
            }
        }
    }

    return ok(to_string(vaults));
}
