#pragma once

#include "ArgsGenerator.hpp"
#include "protocols/shell/types.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "protocols/shell/Router.hpp"
#include "TestUsageManager.hpp"
#include "CLITestContext.hpp"
#include "CLITestRunner.hpp"
#include "CommandUsage.hpp"
#include "CommandBuilder.hpp"

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <stdexcept>

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

    void create(const EntityType& type) const {
        const auto cmd = ctx_->getCommand(type, "create");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for creation");

        const auto admin = database::UserQueries::getUserByName("admin");
        const std::unique_ptr<shell::SocketIO> io;

        if (type == EntityType::USER) {
            const auto user = factory_->create<types::User>(EntityType::USER);
            const auto command = CommandBuilder(cmd).build();
            const auto stdout_text = parseOutput(router_->executeLine(command, admin, io.get()));
            user->id = extractId(stdout_text);
            ctx_->users.push_back(user);
            return;
        }

        const auto owner = ctx_->users.empty() ? admin : ctx_->pickRandomUser();

        if (type == EntityType::VAULT) {
            const auto vault = factory_->create<types::Vault>(EntityType::VAULT, owner);
            const auto command = CommandBuilder(cmd).withEntity(*owner).build();
            const auto stdout_text = parseOutput(router_->executeLine(command, admin, io.get()));
            vault->id = extractId(stdout_text);
            ctx_->vaults.push_back(vault);
            return;
        }

        if (type == EntityType::GROUP) {
            const auto group = factory_->create<types::Group>(EntityType::GROUP);
            const auto command = CommandBuilder(cmd).build();
            const auto stdout_text = parseOutput(router_->executeLine(command, admin, io.get()));
            group->id = extractId(stdout_text);
            ctx_->groups.push_back(group);
            return;
        }

        if (type == EntityType::USER_ROLE) {
            const auto role = factory_->create<types::UserRole>(EntityType::USER_ROLE);
            const auto command = CommandBuilder(cmd).build();
            const auto stdout_text = parseOutput(router_->executeLine(command, admin, io.get()));
            role->id = extractId(stdout_text);
            ctx_->userRoles.push_back(role);
            return;
        }

        if (type == EntityType::VAULT_ROLE) {
            const auto role = factory_->create<types::VaultRole>(EntityType::VAULT_ROLE);
            const auto command = CommandBuilder(cmd).build();
            const auto stdout_text = parseOutput(router_->executeLine(command, admin, io.get()));
            role->id = extractId(stdout_text);
            ctx_->vaultRoles.push_back(role);
            return;
        }

        throw std::runtime_error("EntityFactory: unsupported entity type for creation");
    }

    void seedBaseline(const SeedContext& cfg) const {
        for (unsigned int i = 0; i < cfg.numUsers; ++i) create(EntityType::USER);
        for (unsigned int i = 0; i < cfg.numGroups; ++i) create(EntityType::GROUP);
        for (unsigned int i = 0; i < cfg.numVaults; ++i) create(EntityType::VAULT);
        for (unsigned int i = 0; i < cfg.numUserRoles; ++i) create(EntityType::USER_ROLE);
        for (unsigned int i = 0; i < cfg.numVaultRoles; ++i) create(EntityType::VAULT_ROLE);
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

    static std::string parseOutput(const shell::CommandResult& res) {
        if (res.exit_code != 0)
            throw std::runtime_error("EntityFactory: command failed with exit code " + std::to_string(res.exit_code) + ": " + res.stderr);
        return res.stdout_text;
    }
};

}
