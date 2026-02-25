#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/util/argsHelpers.hpp"
#include "runtime/Deps.hpp"

#include "database/queries/VaultQueries.hpp"
#include "database/queries/WaiverQueries.hpp"
#include "database/queries/SyncQueries.hpp"

#include "storage/Manager.hpp"
#include "storage/s3/S3Controller.hpp"

#include "vault/model/Vault.hpp"
#include "identities/model/User.hpp"

#include "config/ConfigRegistry.hpp"
#include "CommandUsage.hpp"

#include <string>
#include <vector>
#include <memory>

using namespace vh;
using namespace vh::protocols::shell;
using namespace vh::protocols::shell::commands::vault;
using namespace vh::vault::model;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::cloud;

CommandResult commands::vault::handle_vault_update(const CommandCall& call) {
    constexpr const auto* ERR = "vault update";

    const auto usage = resolveUsage({"vault", "update"});
    validatePositionals(call, usage);

    const auto vLkp = resolveVault(call, call.positionals[0], usage, ERR);
    if (!vLkp || !vLkp.ptr) return invalid(vLkp.error);
    const auto vault = vLkp.ptr;

    if (!call.user->canManageVaults() && vault->owner_id != call.user->id)
        return invalid("vault update: you do not have permission to update this vault");

    assignDescIfAvailable(call, usage, vault);
    assignQuotaIfAvailable(call, usage, vault);
    assignOwnerIfAvailable(call, usage, vault);

    const auto sync = SyncQueries::getSync(vault->id);
    parseSync(call, usage, vault, sync);
    parseS3API(call, usage, vault, vault->owner_id, false);

    const auto [okToProceed, waiver] = handle_encryption_waiver({call, vault, true});
    if (!okToProceed) return invalid("vault create: user did not accept encryption waiver");
    if (waiver) WaiverQueries::addWaiver(waiver);

    VaultQueries::upsertVault(vault, sync);

    return ok("Successfully updated vault!\n" + to_string(vault));
}


CommandResult commands::vault::handle_vault_delete(const CommandCall& call) {
    constexpr const auto* ERR = "vault delete";

    const auto usage = resolveUsage({"vault", "delete"});
    validatePositionals(call, usage);

    const auto vLkp = resolveVault(call, call.positionals[0], usage, ERR);
    if (!vLkp || !vLkp.ptr) return invalid(vLkp.error);
    const auto vault = vLkp.ptr;

    if (!call.user->canManageVaults()) {
        if (call.user->id != vault->owner_id) return invalid(
            "vault delete: you do not have permission to delete this vault");

        if (!call.user->canDeleteVaultData(vault->id)) return invalid(
            "vault delete: you do not have permission to delete this vault's data");
    }

    runtime::Deps::get().storageManager->removeVault(vault->id);

    return ok("Successfully deleted vault '" + vault->name + "' (ID: " + std::to_string(vault->id) + ")\n");
}
