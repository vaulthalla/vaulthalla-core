#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>

namespace vh::identities::model { struct User; struct Group; }
namespace vh::rbac::model { struct Role; struct UserRole; struct VaultRole; }
namespace vh::vault::model { struct Vault; struct APIKey; }

namespace vh::shell {
class CommandUsage;
class UsageManager;
}

namespace vh::test::cli {

struct CLITestContext;
enum class EntityType;

struct CLITestContext {
    static constexpr std::array<std::string_view, 4> ENTITIES = {"user", "vault", "group", "role"};
    static constexpr std::array<std::string_view, 5> ACTIONS = {"create", "update", "delete", "list", "info"};

    std::vector<std::shared_ptr<identities::model::User>> users;
    std::vector<std::shared_ptr<vault::model::APIKey>> api_keys;
    std::vector<std::shared_ptr<vault::model::Vault>> vaults;
    std::vector<std::shared_ptr<rbac::model::UserRole>> userRoles;
    std::vector<std::shared_ptr<rbac::model::VaultRole>> vaultRoles;
    std::vector<std::shared_ptr<identities::model::Group>> groups;
    std::shared_ptr<shell::UsageManager> usage;
    std::unordered_map<std::string, std::shared_ptr<shell::CommandUsage>> commands;

    CLITestContext();

    [[nodiscard]] std::shared_ptr<identities::model::User> pickRandomUser() const;
    [[nodiscard]] std::shared_ptr<identities::model::Group> pickRandomGroup() const;
    [[nodiscard]] std::shared_ptr<vault::model::Vault> pickRandomVault() const;
    [[nodiscard]] std::shared_ptr<vault::model::Vault> pickVaultOwnedBy(const std::shared_ptr<identities::model::User>& user) const;
    [[nodiscard]] std::shared_ptr<rbac::model::UserRole> randomUserRole() const;
    [[nodiscard]] std::shared_ptr<rbac::model::VaultRole> randomVaultRole() const;
    [[nodiscard]] static std::string getCommandName(const EntityType& type, const std::string& action);
    [[nodiscard]] std::shared_ptr<shell::CommandUsage> getCommand(const EntityType& type, const std::string& action) const;
};

}
