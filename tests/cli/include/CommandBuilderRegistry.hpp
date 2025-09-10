#pragma once

#include "CommandBuilder.hpp"
#include "EntityType.hpp"

#include <memory>

namespace vh::test::cli {

template <typename T = void>
struct CommandBuilder;

struct UserCommandBuilder;
struct VaultCommandBuilder;
struct GroupCommandBuilder;
struct UserRoleCommandBuilder;
struct VaultRoleCommandBuilder;
class TestUsageManager;
enum class EntityType;

class CommandBuilderRegistry {
public:
    std::shared_ptr<UserCommandBuilder> userBuilder;
    std::shared_ptr<VaultCommandBuilder> vaultBuilder;
    std::shared_ptr<GroupCommandBuilder> groupBuilder;
    std::shared_ptr<UserRoleCommandBuilder> userRoleBuilder;
    std::shared_ptr<VaultRoleCommandBuilder> vaultRoleBuilder;


    CommandBuilderRegistry(const CommandBuilderRegistry&) = delete;
    CommandBuilderRegistry& operator=(const CommandBuilderRegistry&) = delete;

    static CommandBuilderRegistry& instance() {
        static CommandBuilderRegistry instance;
        return instance;
    }

    static void init(const std::shared_ptr<TestUsageManager>& usage) {
        auto& registry = instance();
        registry.userBuilder = std::make_shared<UserCommandBuilder>(usage);
        registry.vaultBuilder = std::make_shared<VaultCommandBuilder>(usage);
        registry.groupBuilder = std::make_shared<GroupCommandBuilder>(usage);
        registry.userRoleBuilder = std::make_shared<UserRoleCommandBuilder>(usage);
        registry.vaultRoleBuilder = std::make_shared<VaultRoleCommandBuilder>(usage);
    }

    template <typename T>
    std::shared_ptr<CommandBuilder<T>> getBuilder(const EntityType& type) const {
        switch (type) {
        case EntityType::USER: return userBuilder;
        case EntityType::VAULT: return vaultBuilder;
        case EntityType::GROUP: return groupBuilder;
        case EntityType::USER_ROLE: return userRoleBuilder;
        case EntityType::VAULT_ROLE: return vaultRoleBuilder;
        default: throw std::runtime_error("CommandBuilderRegistry: unsupported entity type for command builder");
        }
    }

private:
    CommandBuilderRegistry() = default;  // private ctor
};

}
