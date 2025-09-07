#pragma once

#include "CommandUsage.hpp"
#include "CommandBook.hpp"

namespace vh::shell {

namespace aku { [[nodiscard]] std::string usage_cloud_provider(); }

namespace permissions {
    [[nodiscard]] std::string usage_vault_permissions();
    [[nodiscard]] std::string usage_user_permissions();
}

namespace vault { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace user { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace group { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace secrets { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace aku { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace role { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace permissions { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace help { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }
namespace version { std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent); }

static constexpr std::vector<std::string> ADMIN_SHELL_PERMS = {
    "manage-encryption-keys",
    "manage-admins",
    "manage-users",
    "manage-groups",
    "manage-vaults",
    "manage-roles",
    "manage-api-keys",
    "audit-log-access",
    "create-vaults"
};

static constexpr std::vector<std::string> VAULT_SHELL_PERMS = {
    "manage-vault",
    "manage-access",
    "manage-tags",
    "manage-metadata",
    "manage-versions",
    "manage-file-locks",
    "share",
    "sync",
    "create",
    "download",
    "delete",
    "rename",
    "move",
    "list"
};

static std::vector<std::string> mergeVectors() {
    std::vector<std::string> all_perms = ADMIN_SHELL_PERMS;
    all_perms.insert(all_perms.end(), VAULT_SHELL_PERMS.begin(), VAULT_SHELL_PERMS.end());
    return all_perms;
}

static constexpr std::vector<std::string> ALL_SHELL_PERMS = mergeVectors();

}
