#pragma once

#include "protocols/shell/types.hpp"

#include <memory>

namespace vh::shell {
class Router;
}

namespace vh::types {
struct Vault;
struct S3Vault;
struct Sync;
struct User;
struct Waiver;
}

namespace vh::storage {
struct StorageEngine;
}

namespace vh::shell::commands::vault {

struct WaiverContext {
    const CommandCall& call;
    std::shared_ptr<types::Vault> vault;
    bool isUpdate = false;
};

struct WaiverResult {
    bool okToProceed;
    std::shared_ptr<types::Waiver> waiver;
};

// router.cpp
void registerCommands(const std::shared_ptr<Router>& r);

// waiver.cpp
WaiverResult handle_encryption_waiver(const WaiverContext& ctx);

// create.cpp
CommandResult handle_vault_create(const CommandCall& call);

// lifecycle.cpp
CommandResult handle_vault_update(const CommandCall& call);
CommandResult handle_vault_delete(const CommandCall& call);

// listinfo.cpp
CommandResult handle_vault_info(const CommandCall& call);
CommandResult handle_vaults_list(const CommandCall& call);

// role.cpp
CommandResult handle_vault_role(const CommandCall& call);

// keys.cpp
CommandResult handle_vault_keys(const CommandCall& call);

// sync.cpp
CommandResult handle_sync(const CommandCall& call);

}
