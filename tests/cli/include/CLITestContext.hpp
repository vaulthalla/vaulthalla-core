#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
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
}

namespace vh::test::cli {

struct CLITestContext;
struct TestUsageManager;
enum class EntityType;

struct TestStage {
    std::string name;
    std::function<void(CLITestContext&)> run;
};

static constexpr std::array<TestStage, 3> kDefaultTestStages = {{
    {"phase0: seed baseline", [](CLITestContext& ctx) {}},
    {"phase1: generate entities", [](CLITestContext& ctx) {}},
    {"phase2: run tests", [](CLITestContext& ctx) {}},
}};

struct CLITestContext {
    CLITestContext();

    std::shared_ptr<TestStage> stage;
    std::vector<std::shared_ptr<types::User>> users;
    std::vector<std::shared_ptr<types::APIKey>> api_keys;
    std::vector<std::shared_ptr<types::Vault>> vaults;
    std::vector<std::shared_ptr<types::UserRole>> userRoles;
    std::vector<std::shared_ptr<types::VaultRole>> vaultRoles;
    std::vector<std::shared_ptr<types::Group>> groups;
    std::shared_ptr<TestUsageManager> usage;
    std::unordered_map<std::string, std::shared_ptr<shell::CommandUsage>> commands;

    [[nodiscard]] std::shared_ptr<types::User> pickRandomUser() const;
    [[nodiscard]] std::shared_ptr<types::Vault> pickVaultOwnedBy(const std::shared_ptr<types::User>& user) const;
    [[nodiscard]] std::shared_ptr<types::UserRole> randomUserRole() const;
    [[nodiscard]] std::shared_ptr<types::VaultRole> randomVaultRole() const;
    [[nodiscard]] static std::string getCommandName(const EntityType& type, const std::string& action);
    [[nodiscard]] std::shared_ptr<shell::CommandUsage> getCommand(const EntityType& type, const std::string& action) const;
};

}
