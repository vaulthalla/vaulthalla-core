#pragma once

#include "ArgsGenerator.hpp"
#include "EntityType.hpp"
#include "generators.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "TestUsageManager.hpp"
#include "CLITestContext.hpp"
#include "permsUtil.hpp"
#include "CommandUsage.hpp"
#include "database/Queries/UserQueries.hpp"
#include "updateAliases.hpp"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <stdexcept>
#include <cstdlib>
#include <functional>

#include "protocols/shell/Router.hpp"

namespace vh::test::cli {

class UpdateHandler {
public:
    UpdateHandler(const std::shared_ptr<TestUsageManager>& usage,
                  const std::shared_ptr<shell::Router>& router,
                  const std::shared_ptr<CLITestContext>& ctx)
        : usage_(usage), ctx_(ctx), router_(router),
          userAliases_(ctx), groupAliases_(ctx), vaultRoleAliases_(ctx), userRoleAliases_(ctx), vaultAliases_(ctx) {
        if (!usage_) throw std::runtime_error("EntityFactory: usage manager is null");
    }

    void handleUserUpdate(const std::shared_ptr<types::User>& user, const std::string& field) const {
        const std::string usagePath = "user/update";
        if (userAliases_.isName(field)) user->name = generateName(usagePath);
        else if (userAliases_.isEmail(field)) user->email = generateEmail(usagePath);
        else if (userAliases_.isRole(field)) {
            if (ctx_->userRoles.empty()) throw std::runtime_error("EntityFactory: no user roles available to assign");
            user->role = ctx_->userRoles[generateRandomIndex(ctx_->userRoles.size())];
        } else throw std::runtime_error("EntityFactory: unsupported user field for update: " + field);
    }

    void handleGroupUpdate(const std::shared_ptr<types::Group>& group, const std::string& field) const {
        const std::string usagePath = "group/update";
        if (groupAliases_.isName(field)) group->name = generateName(usagePath);
        else throw std::runtime_error("EntityFactory: unsupported group field for update: " + field);
    }

    void handleVaultUpdate(const std::shared_ptr<types::Vault>& vault, const std::string& field) const {
        const std::string usagePath = "vault/update";
        if (vaultAliases_.isName(field)) vault->name = generateName(usagePath);
        else if (vaultAliases_.isQuota(field)) vault->setQuotaFromStr(generateQuotaStr(usagePath));
        else throw std::runtime_error("EntityFactory: unsupported vault field for update: " + field);
    }

    void handleUserRoleUpdate(const std::shared_ptr<types::UserRole>& role, const std::string& field) const {
        const std::string usagePath = "role/update";
        if (userRoleAliases_.isName(field)) role->name = generateRoleName(EntityType::USER_ROLE, usagePath);
        else if (userRoleAliases_.isDescription(field)) role->description = "Updated user role description";
        else if (userRoleAliases_.isPermissions(field)) role->permissions = generateBitmask(ADMIN_SHELL_PERMS.size());
        else throw std::runtime_error("EntityFactory: unsupported user role field for update: " + field);
    }

    void handleVaultRoleUpdate(const std::shared_ptr<types::VaultRole>& role, const std::string& field) const {
        const std::string usagePath = "role/update";
        if (vaultRoleAliases_.isName(field)) role->name = generateRoleName(EntityType::VAULT_ROLE, usagePath);
        else if (vaultRoleAliases_.isDescription(field)) role->description = "Updated vault role description";
        else if (vaultRoleAliases_.isPermissions(field)) role->permissions = generateBitmask(VAULT_SHELL_PERMS.size());
        else throw std::runtime_error("EntityFactory: unsupported vault role field for update: " + field);
    }

    std::string buildUserUpdate(const std::shared_ptr<types::User>& user, const std::unordered_set<std::string>& updatedFields) const {
        const auto cmd = ctx_->getCommand(EntityType::USER, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for user update");

        std::ostringstream oss;
        oss << "vh user update " << (generateRandomIndex(10000) < 5000 ? std::to_string(user->id) : user->name);
        for (const auto& field : updatedFields) {
            oss << ' ' << field;
            if (userAliases_.isName(field)) oss << ' ' << user->name;
            else if (userAliases_.isEmail(field)) oss << ' ' << *user->email;
            else if (userAliases_.isRole(field)) oss << ' ' << user->role->name;
        }

        return oss.str();
    }

    std::string buildGroupUpdate(const std::shared_ptr<types::Group>& group, const std::unordered_set<std::string>& updatedFields) const {
        const auto cmd = ctx_->getCommand(EntityType::GROUP, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for group update");

        std::ostringstream oss;
        oss << "vh group update " << (generateRandomIndex(10000) < 5000 ? std::to_string(group->id) : group->name);
        for (const auto& field : updatedFields) {
            oss << ' ' << field;
            if (groupAliases_.isName(field)) oss << ' ' << group->name;
        }

        return oss.str();
    }

    std::string buildUserRoleUpdate(const std::shared_ptr<types::UserRole>& role, const std::unordered_set<std::string>& updatedFields) const {
        const auto cmd = ctx_->getCommand(EntityType::USER_ROLE, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for user role update");

        std::ostringstream oss;
        oss << "vh role update " << (generateRandomIndex(10000) < 5000 ? std::to_string(role->id) : role->name);
        for (const auto& field : updatedFields) {
            oss << ' ' << field;
            if (userRoleAliases_.isName(field)) oss << ' ' << role->name;
            else if (userRoleAliases_.isDescription(field)) oss << ' ' << role->description;
            else if (userRoleAliases_.isPermissions(field)) oss << ' ' << std::to_string(role->permissions);
        }

        return oss.str();
    }

    std::string buildVaultRoleUpdate(const std::shared_ptr<types::VaultRole>& role, const std::unordered_set<std::string>& updatedFields) const {
        const auto cmd = ctx_->getCommand(EntityType::VAULT_ROLE, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault role update");

        std::ostringstream oss;
        oss << "vh role update " << (generateRandomIndex(10000) < 5000 ? std::to_string(role->id) : role->name);
        for (const auto& field : updatedFields) {
            oss << ' ' << field;
            if (vaultRoleAliases_.isName(field)) oss << ' ' << role->name;
            else if (vaultRoleAliases_.isDescription(field)) oss << ' ' << role->description;
            else if (vaultRoleAliases_.isPermissions(field)) oss << ' ' << std::to_string(role->permissions);
        }

        return oss.str();
    }

    std::string buildVaultUpdate(const std::shared_ptr<types::Vault>& vault, const std::unordered_set<std::string>& updatedFields) const {
        const auto cmd = ctx_->getCommand(EntityType::VAULT, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault update");

        std::ostringstream oss;
        oss << "vh vault update " << (generateRandomIndex(10000) < 5000 ? std::to_string(vault->id) : vault->name);
        for (const auto& field : updatedFields) {
            oss << ' ' << field;
            if (vaultAliases_.isName(field)) oss << ' ' << vault->name;
            else if (vaultAliases_.isQuota(field)) oss << ' ' << std::to_string(vault->quota);
            else if (vaultAliases_.isOwner(field)) oss << ' ' << std::to_string(vault->owner_id);
            // TODO: handle sync updates requires new object
        }

        return oss.str();
    }

    static std::unordered_set<std::string> handleUpdate(const std::function<void(const std::string& field)>& handler,
                                                        const std::shared_ptr<shell::CommandUsage>& cmd) {
        const auto numFieldsToUpdate = std::max(static_cast<size_t>(1), generateRandomIndex(cmd->optional.size()));

        std::unordered_set<std::string> updated;
        while (updated.size() < numFieldsToUpdate) {
            const auto opt = cmd->optional[generateRandomIndex(cmd->optional.size())];
            for (const auto& t : opt.option_tokens) {
                if (!updated.contains(t)) {
                    handler(t);
                    updated.insert(t);
                    break;
                }
            }
        }

        return updated;
    }

    shell::CommandResult commitUpdate(const std::function<std::string()>& builder) const {
        const auto admin = database::UserQueries::getUserByName("admin");
        int fd;
        const auto io = std::make_unique<shell::SocketIO>(fd);
        return router_->executeLine(builder(), admin, io.get());
    }

    shell::CommandResult update(const std::function<void(const std::string& field)>& handler,
                       const std::function<std::string(const std::unordered_set<std::string>& updated)>& baseBuilder,
                       const std::shared_ptr<shell::CommandUsage>& cmd) const {
        const auto updated = handleUpdate(handler, cmd);
        const auto builder = [baseBuilder, &updated]() { return baseBuilder(updated); };
        return commitUpdate(builder);
    }

    EntityResult update(const EntityType& type, const std::shared_ptr<void>& entity, const std::shared_ptr<types::User>& owner = nullptr) {
        const auto cmd = ctx_->getCommand(type, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for update");

        if (type == EntityType::USER) {
            const auto user = std::static_pointer_cast<types::User>(entity);
            const auto handler = [this, &user](const std::string& field) { handleUserUpdate(user, field); };
            const auto builder = [this, &user](const std::unordered_set<std::string>& updated) { return buildUserUpdate(user, updated); };
            return {update(handler, builder, cmd), user};
        }

        if (type == EntityType::VAULT) {
            const auto vault = std::static_pointer_cast<types::Vault>(entity);
            if (owner) vault->owner_id = owner->id;
            const auto handler = [this, &vault](const std::string& field) { handleVaultUpdate(vault, field); };
            const auto builder = [this, &vault](const std::unordered_set<std::string>& updated) { return buildVaultUpdate(vault, updated); };
            return {update(handler, builder, cmd), vault};
        }

        if (type == EntityType::GROUP) {
            const auto group = std::static_pointer_cast<types::Group>(entity);
            const auto handler = [this, &group](const std::string& field) { handleGroupUpdate(group, field); };
            const auto builder = [this, &group](const std::unordered_set<std::string>& updated) { return buildGroupUpdate(group, updated); };
            return {update(handler, builder, cmd), group};
        }

        if (type == EntityType::USER_ROLE) {
            const auto role = std::static_pointer_cast<types::UserRole>(entity);
            const auto handler = [this, &role](const std::string& field) { handleUserRoleUpdate(role, field); };
            const auto builder = [this, &role](const std::unordered_set<std::string>& updated) { return buildUserRoleUpdate(role, updated); };
            return {update(handler, builder, cmd), role};
        }

        if (type == EntityType::VAULT_ROLE) {
            const auto role = std::static_pointer_cast<types::VaultRole>(entity);
            const auto handler = [this, &role](const std::string& field) { handleVaultRoleUpdate(role, field); };
            const auto builder = [this, &role](const std::unordered_set<std::string>& updated) { return buildVaultRoleUpdate(role, updated); };
            return {update(handler, builder, cmd), role};
        }

        throw std::runtime_error("EntityFactory: unsupported entity type for update");
    }

private:
    std::shared_ptr<TestUsageManager> usage_;
    std::shared_ptr<CLITestContext> ctx_;
    std::shared_ptr<shell::Router> router_;

    UserAliases userAliases_;
    GroupAliases groupAliases_;
    VaultRoleAliases vaultRoleAliases_;
    UserRoleAliases userRoleAliases_;
    S3VaultAliases vaultAliases_;
};

}
