#include "CLITestContext.hpp"
#include "TestUsageManager.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "EntityType.hpp"
#include "generators.hpp"

#include <stdexcept>

using namespace vh::test::cli;
using namespace vh::types;

CLITestContext::CLITestContext()
    : stage(std::make_shared<TestStage>(kDefaultTestStages[0])),
      usage(std::make_shared<TestUsageManager>()) {
    const std::array<std::string, 3> entities = {"user", "vault", "group"};
    const std::array<std::string, 5> actions = {"create", "update", "delete", "list", "info"};

    for (const auto& entity : entities) {
        for (const auto& action : actions) {
            const auto cmdName = entity + " " + action;
            const auto cmdUsage = usage->resolve({entity, action});
            if (!cmdUsage) throw std::runtime_error("EntityFactory: unknown command usage: " + cmdName);
            commands[cmdName] = cmdUsage;
        }
    }
}

std::string CLITestContext::getCommandName(const EntityType& type, const std::string& action) {
    switch (type) {
        case EntityType::USER:  return "user " + action;
        case EntityType::VAULT: return "vault " + action;
        case EntityType::GROUP: return "group " + action;
        default: throw std::runtime_error("EntityFactory: unsupported entity type for command name");
    }
}

std::shared_ptr<vh::shell::CommandUsage> CLITestContext::getCommand(const EntityType& type, const std::string& action) const {
    return commands.at(getCommandName(type, action));
}

std::shared_ptr<User> CLITestContext::pickRandomUser() const {
    if (users.empty()) throw std::runtime_error("CLITestContext: no users available to pick from");
    return users[generateRandomIndex(users.size())];
}

std::shared_ptr<Vault> CLITestContext::pickVaultOwnedBy(const std::shared_ptr<User>& user) const {
    if (vaults.empty()) throw std::runtime_error("CLITestContext: no vaults available to pick from");
    std::vector<std::shared_ptr<Vault>> owned;
    for (const auto& v : vaults) if (v->owner_id == user->id) owned.push_back(v);
    if (owned.empty()) throw std::runtime_error("CLITestContext: user owns no vaults to pick from");
    return owned[generateRandomIndex(owned.size())];
}

std::shared_ptr<UserRole> CLITestContext::randomUserRole() const {
    if (userRoles.empty()) throw std::runtime_error("CLITestContext: no user roles available to pick from");
    return userRoles[generateRandomIndex(userRoles.size())];
}

std::shared_ptr<VaultRole> CLITestContext::randomVaultRole() const {
    if (vaultRoles.empty()) throw std::runtime_error("CLITestContext: no vault roles available to pick from");
    return vaultRoles[generateRandomIndex(vaultRoles.size())];
}
