#pragma once

#include "protocols/shell/types.hpp"
#include "util/shellArgsHelpers.hpp"
#include "helpers.hpp"

#include <memory>
#include <optional>
#include <regex>
#include <string>

namespace vh::shell {
class Router;
class CommandUsage;
}

namespace vh::types {
struct Vault;
struct S3Vault;
struct Sync;
struct User;
struct Waiver;
struct VaultRole;
struct Role;
enum class VaultType;
enum class OverrideOpt;

namespace api {
struct APIKey;
}
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

// helpers.cpp
std::optional<unsigned int> parsePositiveUint(const std::string& s, const char* errLabel, std::string& errOut);

std::shared_ptr<types::User> resolveOwner(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage);
Lookup<types::User> resolveOwnerRequired(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::string& errPrefix);

Lookup<types::Vault> resolveVault(const CommandCall& call, const std::string& vaultArg, const std::shared_ptr<CommandUsage>& usage, const std::string& errPrefix);

Lookup<storage::StorageEngine> resolveEngine(const CommandCall& call, const std::string& vaultArg, const std::shared_ptr<CommandUsage>& usage, const std::string& errPrefix);

std::optional<std::string> checkOverridePermissions(const CommandCall& call,
                                                    const std::shared_ptr<types::Vault>& vault,
                                                    const std::string& errPrefix);

Lookup<types::VaultRole> resolveVRole(const std::string& roleArg,
                                      const std::shared_ptr<types::Vault>& vault,
                                      const Subject* subjectOrNull,
                                      const std::string& errPrefix);

PatternParse parsePatternOpt(const CommandCall& call, bool required, const std::string& errPrefix);

EnableParse parseEnableDisableOpt(const CommandCall& call, const std::string& errPrefix);

EffectParse parseEffectChangeOpt(const CommandCall& call, const std::string& errPrefix);

std::unique_ptr<types::VaultType> parseVaultType(const CommandCall& call);

void assignDescIfAvailable(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<types::Vault>& vault);

void assignQuotaIfAvailable(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<types::Vault>& vault);

void assignOwnerIfAvailable(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<types::Vault>& vault);

void parseSync(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<types::Vault>& vault, const std::shared_ptr<types::Sync>& sync);

void parseS3API(const CommandCall& call, const std::shared_ptr<CommandUsage>& usage, const std::shared_ptr<types::Vault>& vault, unsigned int ownerId, bool required = false);

}