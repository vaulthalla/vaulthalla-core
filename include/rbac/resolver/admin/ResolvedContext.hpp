#pragma once

#include "identities/User.hpp"
#include "identities/Group.hpp"
#include "storage/Engine.hpp"
#include "rbac/resolver/admin/Context.hpp"

#include <memory>

namespace vh::rbac::resolver::admin {

    struct ResolvedVaultContext {
        std::shared_ptr<storage::Engine> engine;
        std::shared_ptr<vault::model::Vault> vault;
        std::shared_ptr<identities::User> owner;
        std::shared_ptr<identities::Group> group;
        std::optional<Entity> identity{};  // Users, Admins, Groups
        std::optional<Scope> scope{};

        [[nodiscard]] bool isValid() const { return (owner || (identity == Entity::Group && group)) && (identity || scope); }
        [[nodiscard]] bool hasOwner() const { return !!owner; }
    };

}
