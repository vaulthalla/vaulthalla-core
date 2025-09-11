#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>

namespace vh::types {
struct User;
struct APIKey;
struct Vault;
struct Role;
struct UserRole;
struct VaultRole;
struct Group;
}

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

    std::vector<std::shared_ptr<types::User>> users;
    std::vector<std::shared_ptr<types::APIKey>> api_keys;
    std::vector<std::shared_ptr<types::Vault>> vaults;
    std::vector<std::shared_ptr<types::UserRole>> userRoles;
    std::vector<std::shared_ptr<types::VaultRole>> vaultRoles;
    std::vector<std::shared_ptr<types::Group>> groups;
    std::shared_ptr<shell::UsageManager> usage;
    std::unordered_map<std::string, std::shared_ptr<shell::CommandUsage>> commands;

    CLITestContext();

    [[nodiscard]] std::shared_ptr<types::User> pickRandomUser() const;
    [[nodiscard]] std::shared_ptr<types::Vault> pickVaultOwnedBy(const std::shared_ptr<types::User>& user) const;
    [[nodiscard]] std::shared_ptr<types::UserRole> randomUserRole() const;
    [[nodiscard]] std::shared_ptr<types::VaultRole> randomVaultRole() const;
    [[nodiscard]] static std::string getCommandName(const EntityType& type, const std::string& action);
    [[nodiscard]] std::shared_ptr<shell::CommandUsage> getCommand(const EntityType& type, const std::string& action) const;
};

}
