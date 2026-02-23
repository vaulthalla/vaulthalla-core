#pragma once

#include "EntityType.hpp"
#include "generators.hpp"
#include "types/entities/User.hpp"
#include "types/vault/Vault.hpp"
#include "types/entities/Group.hpp"
#include "types/rbac/Role.hpp"
#include "types/rbac/UserRole.hpp"
#include "types/rbac/VaultRole.hpp"
#include "CLITestContext.hpp"
#include "permsUtil.hpp"

#include <string>
#include <memory>
#include <optional>

namespace vh::test::cli {

class EntityFactory {
public:
    explicit EntityFactory(const std::shared_ptr<CLITestContext>& ctx) : ctx_(ctx) {}

    [[nodiscard]] std::shared_ptr<void> create(const EntityType& type) const {
        if (type == EntityType::USER) {
            const std::string usage = "user/create";
            const auto user = std::make_shared<types::User>();
            user->name = generateName(usage);
            if (coin()) user->email = generateEmail(usage);
            user->role = ctx_->randomUserRole();
            return user;
        }

        if (type == EntityType::VAULT) {
            const std::string usage = "vault/create";
            const auto vault = std::make_shared<types::Vault>();
            vault->name = generateName(usage);
            vault->setQuotaFromStr(generateQuotaStr(usage));
            vault->owner_id = ctx_->pickRandomUser()->id;
            return vault;
        }

        if (type == EntityType::GROUP) {
            const auto group = std::make_shared<types::Group>();
            group->name = generateName("group/create");
            return group;
        }

        if (type == EntityType::USER_ROLE) {
            const auto role = std::make_shared<types::UserRole>();
            role->name = generateRoleName(type, "role/create");
            role->description = "Auto-generated user role";
            role->type = "user";
            role->permissions = generateBitmask(ADMIN_SHELL_PERMS.size());
            return role;
        }

        if (type == EntityType::VAULT_ROLE) {
            const auto role = std::make_shared<types::VaultRole>();
            role->name = generateRoleName(type, "role/create");
            role->description = "Auto-generated vault role";
            role->type = "vault";
            role->permissions = generateBitmask(VAULT_SHELL_PERMS.size());
            return role;
        }

        return nullptr;
    }

    void seedBaseline(const std::shared_ptr<CLITestContext>& ctx);

private:
    std::shared_ptr<CLITestContext> ctx_;
};

}
