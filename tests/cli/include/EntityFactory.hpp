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
#include <unordered_map>
#include <memory>
#include <optional>
#include <chrono>
#include <stdexcept>
#include <array>
#include <cstdlib>

namespace vh::test::cli {

enum class EntityType {
    USER,
    VAULT,
    GROUP,
    USER_ROLE,
    VAULT_ROLE
};

class EntityFactory {
public:
    EntityFactory(const std::shared_ptr<shell::Router>& router,
                  const std::shared_ptr<TestUsageManager>& usage,
                  const std::shared_ptr<CLITestContext>& ctx)
        : router_(std::move(router)), usage_(std::move(usage)) {
        if (!router_) throw std::runtime_error("EntityFactory: router is null");
        if (!usage_) throw std::runtime_error("EntityFactory: usage manager is null");

        std::array<std::string, 3> entities = {"user", "vault", "group"};
        std::array<std::string, 5> actions = {"create", "update", "delete", "list", "info"};

        for (const auto& entity : entities) {
            for (const auto& action : actions) {
                const auto cmdName = entity + " " + action;
                const auto cmdUsage = usage_->resolve({entity, action});
                if (!cmdUsage) throw std::runtime_error("EntityFactory: unknown command usage: " + cmdName);
                commands_[cmdName] = cmdUsage;
            }
        }
    }

    static std::string getCommandName(const EntityType& type, const std::string& action) {
        switch (type) {
            case EntityType::USER:  return "user " + action;
            case EntityType::VAULT: return "vault " + action;
            case EntityType::GROUP: return "group " + action;
            default: throw std::runtime_error("EntityFactory: unsupported entity type for command name");
        }
    }

    static std::string generateNow(args::Gen g,
                               const std::string_view token = "default",
                               const std::string_view usage = "example") {
        args::Rng rng{static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count())};
        args::GenContext ctx{std::string(token), std::string(usage)};
        return args::to_string_value(g->generate(rng, ctx));
    }

    static std::optional<std::string> generateName() {
        return generateNow(args::ArgGenerator::Join({
            args::ArgGenerator::Constant(std::string("user_")),
            args::ArgGenerator::RandomString(6, 10, "abcdefghijklmnopqrstuvwxyz0123456789")
        }, ""), "name", "user/create");
    }

    static std::optional<std::string> generateEmail() {
        return generateNow(args::ArgGenerator::Join({
            args::ArgGenerator::RandomString(6, 10, "abcdefghijklmnopqrstuvwxyz0123456789"),
            args::ArgGenerator::Constant(std::string("@example.org"))
        }, ""), "email", "user/create");
    }

    template <typename T>
    std::shared_ptr<T> create(const EntityType& type, const std::shared_ptr<types::User>& owner = nullptr) {
        const auto cmd = commands_.at(getCommandName(type, "create"));

        if (type == EntityType::USER) {
            const auto user = std::make_shared<types::User>();
            user->name = generateName().value_or("user_" + std::to_string(rand() % 10000));
            if (rand() < 0.5) user->email = generateEmail();
            user->role = ctx_->randomUserRole();

            auto builder = CommandBuilder(cmd, {"user", "create"})
                .withEntity(user)
                .withProvider()

            return std::static_pointer_cast<T>(user);
        }

        if (type == EntityType::VAULT) {
            const auto vault = std::make_shared<types::Vault>();
            vault->name = generateName().value_or("vault_" + std::to_string(rand() % 10000));
            vault->quota = args::ArgGenerator::OneOf({"unlimited", "100MB", "1G", "10G", "100G", "1T"}).generate();
            if (owner) vault->owner_id = owner->id;
            return std::static_pointer_cast<T>(vault);
        }
    }

    void seedBaseline(const std::shared_ptr<CLITestContext>& ctx);

private:
    std::shared_ptr<shell::Router> router_;
    std::shared_ptr<TestUsageManager> usage_;
    std::shared_ptr<CLITestContext> ctx_;
    std::unordered_map<std::string, std::shared_ptr<shell::CommandUsage>> commands_;
};

}
