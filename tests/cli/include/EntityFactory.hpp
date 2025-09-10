#pragma once

#include "ArgsGenerator.hpp"
#include "EntityType.hpp"
#include "generators.hpp"
#include "protocols/shell/types.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"
#include "types/UserRole.hpp"
#include "types/VaultRole.hpp"
#include "CLITestContext.hpp"
#include "permsUtil.hpp"

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <cstdlib>

namespace vh::test::cli {

class EntityFactory {
public:
    explicit EntityFactory(const std::shared_ptr<CLITestContext>& ctx) : ctx_(ctx) {}

    std::shared_ptr<void> create(const EntityType& type, const std::shared_ptr<types::User>& owner = nullptr) {
        if (type == EntityType::USER) {
            const std::string usage = "user/create";
            const auto user = std::make_shared<types::User>();
            user->name = generateName(usage);
            if (rand() < 0.5) user->email = generateEmail(usage);
            user->role = ctx_->randomUserRole();
            return user;
        }

        if (type == EntityType::VAULT) {
            const std::string usage = "vault/create";
            const auto vault = std::make_shared<types::Vault>();
            vault->name = generateName(usage);
            vault->setQuotaFromStr(generateQuotaStr(usage));
            if (owner) vault->owner_id = owner->id;
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

    std::vector<std::shared_ptr<void>> create(const EntityType& type, const std::size_t count, const std::shared_ptr<types::User>& owner = nullptr) {
        std::vector<std::shared_ptr<void>> entities;
        for (std::size_t i = 0; i < count; ++i) if (auto e = create(type, owner)) entities.push_back(e);
        return entities;
    }

    void seedBaseline(const std::shared_ptr<CLITestContext>& ctx);

private:
    std::shared_ptr<CLITestContext> ctx_;
};

}
