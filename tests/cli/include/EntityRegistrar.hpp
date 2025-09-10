#pragma once

#include "protocols/shell/types.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "protocols/shell/Router.hpp"
#include "CLITestContext.hpp"
#include "CommandBuilder.hpp"

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <stdexcept>
#include <iostream>

#include "EntityFactory.hpp"
#include "database/Queries/UserQueries.hpp"

namespace vh::test::cli {

struct SeedContext {
    unsigned int numUsers = 10;
    unsigned int numVaults = 15;
    unsigned int numGroups = 5;
    unsigned int numUserRoles = 7;
    unsigned int numVaultRoles = 7;
};

class EntityRegistrar {
public:
    EntityRegistrar(const std::shared_ptr<shell::Router>& router,
                  const std::shared_ptr<CLITestContext>& ctx)
        : factory_(std::make_shared<EntityFactory>(ctx)),
          router_(router), ctx_(ctx) {
        if (!router_) throw std::runtime_error("EntityFactory: router is null");
    }

        [[nodiscard]] EntityResult create(const EntityType& type) const {
        const auto cmd = ctx_->getCommand(type, "create");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for creation");

        const auto admin = database::UserQueries::getUserByName("admin");
        int fd;
        const auto io = std::make_unique<shell::SocketIO>(fd);

        if (type == EntityType::USER) {
            const auto user = factory_->create(EntityType::USER);
            const auto command = CommandBuilder(cmd).build();
            return { router_->executeLine(command, admin, io.get()), user };
        }

        const auto owner = ctx_->users.empty() ? admin : ctx_->pickRandomUser();

        if (type == EntityType::VAULT) {
            const auto vault = factory_->create(EntityType::VAULT, owner);
            const auto command = CommandBuilder(cmd).withEntity(*owner).build();
            return { router_->executeLine(command, admin, io.get()), vault };
        }

        if (type == EntityType::GROUP) {
            const auto group = factory_->create(EntityType::GROUP);
            const auto command = CommandBuilder(cmd).build();
            return { router_->executeLine(command, admin, io.get()), group };
        }

        if (type == EntityType::USER_ROLE) {
            const auto role = factory_->create(EntityType::USER_ROLE);
            const auto command = CommandBuilder(cmd).build();
            std::cout << "Creating user role: " << command << std::endl;
            return { router_->executeLine(command, admin, io.get()), role };
        }

        if (type == EntityType::VAULT_ROLE) {
            const auto role = factory_->create(EntityType::VAULT_ROLE);
            const auto command = CommandBuilder(cmd).build();
            return { router_->executeLine(command, admin, io.get()), role };
        }

        throw std::runtime_error("EntityFactory: unsupported entity type for creation");
    }

    [[nodiscard]] std::vector<EntityResult> create(const EntityType& type, const std::size_t count) const {
        std::vector<EntityResult> results;
        for (std::size_t i = 0; i < count; ++i) results.push_back(create(type));
        return results;
    }

    [[nodiscard]] EntityResult remove(const EntityType& type, const std::shared_ptr<void>& entity) const {
        const auto cmd = ctx_->getCommand(type, "delete");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for deletion");

        const auto admin = database::UserQueries::getUserByName("admin");
        int fd;
        const auto io = std::make_unique<shell::SocketIO>(fd);

        if (type == EntityType::USER) {
            const auto user = std::static_pointer_cast<types::User>(entity);
            if (!user) throw std::runtime_error("EntityFactory: invalid user entity for deletion");
            const auto command = CommandBuilder(cmd).withEntity(*user).build();
            return { router_->executeLine(command, admin, io.get()), user };
        }

        if (type == EntityType::VAULT) {
            const auto vault = std::static_pointer_cast<types::Vault>(entity);
            if (!vault) throw std::runtime_error("EntityFactory: invalid vault entity for deletion");
            const auto command = CommandBuilder(cmd).withEntity(*vault).build();
            return { router_->executeLine(command, admin, io.get()), vault };
        }

        if (type == EntityType::GROUP) {
            const auto group = std::static_pointer_cast<types::Group>(entity);
            if (!group) throw std::runtime_error("EntityFactory: invalid group entity for deletion");
            const auto command = CommandBuilder(cmd).withEntity(*group).build();
            return { router_->executeLine(command, admin, io.get()), group };
        }

        if (type == EntityType::USER_ROLE) {
            const auto role = std::static_pointer_cast<types::UserRole>(entity);
            if (!role) throw std::runtime_error("EntityFactory: invalid user role entity for deletion");
            const auto command = CommandBuilder(cmd).withEntity(*role).build();
            return { router_->executeLine(command, admin, io.get()), role };
        }

        if (type == EntityType::VAULT_ROLE) {
            const auto role = std::static_pointer_cast<types::VaultRole>(entity);
            if (!role) throw std::runtime_error("EntityFactory: invalid vault role entity for deletion");
            const auto command = CommandBuilder(cmd).withEntity(*role).build();
            return { router_->executeLine(command, admin, io.get()), role };
        }

        throw std::runtime_error("EntityFactory: unsupported entity type for deletion");
    }

    void teardown() const {
        for (const auto& role : ctx_->userRoles) {
            const auto cmd = ctx_->getCommand(EntityType::USER_ROLE, "delete");
            if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for user role deletion");
            const auto command = CommandBuilder(cmd).withEntity(*role).build();
            const auto admin = database::UserQueries::getUserByName("admin");
            int fd;
            const auto io = std::make_unique<shell::SocketIO>(fd);
            router_->executeLine(command, admin, io.get());
        }

        for (const auto& role : ctx_->vaultRoles) {
            const auto cmd = ctx_->getCommand(EntityType::VAULT_ROLE, "delete");
            if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault role deletion");
            const auto command = CommandBuilder(cmd).withEntity(*role).build();
            const auto admin = database::UserQueries::getUserByName("admin");
            int fd;
            const auto io = std::make_unique<shell::SocketIO>(fd);
            router_->executeLine(command, admin, io.get());
        }

        for (const auto& user : ctx_->users) {
            const auto cmd = ctx_->getCommand(EntityType::USER, "delete");
            if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for user deletion");
            const auto command = CommandBuilder(cmd).withEntity(*user).build();
            const auto admin = database::UserQueries::getUserByName("admin");
            int fd;
            const auto io = std::make_unique<shell::SocketIO>(fd);
            router_->executeLine(command, admin, io.get());
        }

        for (const auto& vault : ctx_->vaults) {
            const auto cmd = ctx_->getCommand(EntityType::VAULT, "delete");
            if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault deletion");
            const auto command = CommandBuilder(cmd).withEntity(*vault).build();
            const auto admin = database::UserQueries::getUserByName("admin");
            int fd;
            const auto io = std::make_unique<shell::SocketIO>(fd);
            router_->executeLine(command, admin, io.get());
        }

        for (const auto& group : ctx_->groups) {
            const auto cmd = ctx_->getCommand(EntityType::GROUP, "delete");
            if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for group deletion");
            const auto command = CommandBuilder(cmd).withEntity(*group).build();
            const auto admin = database::UserQueries::getUserByName("admin");
            int fd;
            const auto io = std::make_unique<shell::SocketIO>(fd);
            router_->executeLine(command, admin, io.get());
        }

        ctx_->users.clear();
        ctx_->vaults.clear();
        ctx_->groups.clear();
        ctx_->userRoles.clear();
        ctx_->vaultRoles.clear();
    }

private:
    static constexpr std::string_view ID_REGEX = R"(ID:\s*(\d+))";
    std::shared_ptr<EntityFactory> factory_;
    std::shared_ptr<shell::Router> router_;
    std::shared_ptr<CLITestContext> ctx_;

    static std::regex toRegex(const std::string_view& pattern) {
        return std::regex(std::string(pattern), std::regex::icase);
    }

    static unsigned int extractId(const std::string& output) {
        std::smatch match;
        if (std::regex_search(output, match, toRegex(ID_REGEX)) && match.size() > 1)
            return std::stoi(match.str(1));
        throw std::runtime_error("EntityFactory: failed to parse ID from output");
    }
};

}
