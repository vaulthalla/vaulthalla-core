#pragma once

#include "EntityType.hpp"
#include "generators.hpp"
#include "../../../include/identities/User.hpp"
#include "vault/model/Vault.hpp"
#include "../../../include/identities/Group.hpp"
#include "../../../include/rbac/role/Base.hpp"
#include "rbac/model/UserRole.hpp"
#include "../../../include/rbac/role/Vault.hpp"
#include "CLITestContext.hpp"
#include "permsUtil.hpp"

#include <string>
#include <memory>
#include <optional>

using namespace vh::identities::model;
using namespace vh::rbac::model;
using namespace vh::vault::model;

namespace vh::test::cli {

class EntityFactory {
public:
    explicit EntityFactory(const std::shared_ptr<CLITestContext>& ctx) : ctx_(ctx) {}

    [[nodiscard]] std::shared_ptr<void> create(const EntityType& type) const {
        if (type == EntityType::USER) {
            const std::string usage = "user/create";
            const auto user = std::make_shared<Admin>();
            user->name = generateName(usage);
            if (coin()) user->email = generateEmail(usage);
            user->role = ctx_->randomUserRole();
            return user;
        }

        if (type == EntityType::VAULT) {
            const std::string usage = "vault/create";
            const auto vault = std::make_shared<Vault>();
            vault->name = generateName(usage);
            vault->setQuotaFromStr(generateQuotaStr(usage));
            vault->owner_id = ctx_->pickRandomUser()->id;
            return vault;
        }

        if (type == EntityType::GROUP) {
            const auto group = std::make_shared<Group>();
            group->name = generateName("group/create");
            return group;
        }

        if (type == EntityType::USER_ROLE) {
            const auto role = std::make_shared<Admin>();
            role->name = generateRoleName(type, "role/create");
            role->description = "Auto-generated user role";
            role->type = "user";
            role->permissions = generateBitmask(ADMIN_SHELL_PERMS.size());
            return role;
        }

        if (type == EntityType::VAULT_ROLE) {
            const auto role = std::make_shared<Vault>();
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
