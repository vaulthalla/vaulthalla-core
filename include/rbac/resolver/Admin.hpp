#pragma once

#include "admin/Context.hpp"
#include "AccessTraits/Fwd.hpp"
#include "admin/policy/Base.hpp"
#include "admin/ResolvedContext.hpp"

#include "identities/User.hpp"
#include "rbac/permission/admin/VaultGlobals.hpp"
#include "runtime/Deps.hpp"
#include "db/query/vault/APIKey.hpp"
#include "storage/Manager.hpp"
#include "db/query/identities/User.hpp"
#include "rbac/role/Admin.hpp"

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace vh::rbac::resolver {
    class Resolver {
        template<typename EnumT>
        static bool checkPermission(const std::shared_ptr<identities::User> &user,
                                    const admin::ResolvedVaultContext &resolved,
                                    const EnumT permission) {
            static_assert(std::is_enum_v<EnumT>,
                          "vh::rbac::vault::Resolver::checkPermission(): EnumT must be an enum type");

            if (!user || !resolved.isValid()) return false;

            bool allowed = false;

            if (resolved.scope) {
                const auto &perms = user->apiKeysPerms();

                if (user->id == resolved.vault->owner_id)
                    allowed = AccessTraits<EnumT>::self(perms.self).has(permission);
                else if (resolved.owner->isAdmin())
                    allowed = AccessTraits<EnumT>::admin(perms.admin).has(permission);
                else
                    allowed = AccessTraits<EnumT>::user(perms.user).has(permission);
            }

            if (resolved.identity) {
                const auto& perms = user->identities();
                if (*resolved.identity == admin::Entity::Group)
                    allowed = AccessTraits<EnumT>::group(perms.groups).has(permission);
                else if (*resolved.identity == admin::Entity::User)
                    allowed = AccessTraits<EnumT>::user(perms.users).has(permission);
                else if (*resolved.identity == admin::Entity::Admin)
                    allowed = AccessTraits<EnumT>::admin(perms.admins).has(permission);
            }

            if (!allowed) return false;

            return ContextPolicy<EnumT>::validate(user, resolved, permission);
        }

        static admin::ResolvedVaultContext resolveContext(const std::optional<uint32_t> &vaultId,
                                                          const std::optional<uint32_t> &apiKeyId,
                                                          const std::optional<uint32_t> &targetUserId,
                                                          const std::optional<admin::Entity> &entity,
                                                          const std::optional<admin::Scope> &scope) {
            admin::ResolvedVaultContext resolved{};

            if (targetUserId) resolved.owner = db::query::identities::User::getUserById(*targetUserId);
            if (entity && apiKeyId) resolved.owner = db::query::vault::APIKey::getAPIKeyOwner(*apiKeyId);
            if (scope && vaultId) resolved.engine = runtime::Deps::get().storageManager->getEngine(*vaultId);

            return resolved;
        }

    public:
        template<typename EnumT>
        static bool has(admin::Context<EnumT> &&ctx) {
            if (!ctx.isValid()) return false;

            const auto &user = ctx.user;
            if (!user) return false;
            if (user->isSuperAdmin()) return true;

            const auto resolved = resolveContext(
                ctx.vault_id,
                ctx.api_key_id,
                ctx.target_user_id,
                ctx.identity,
                ctx.scope
            );

            if (!resolved.isValid()) return false;

            return checkPermission(user, resolved, ctx.permission);
        }

        template<typename EnumT>
        static bool has(const admin::Context<EnumT> &ctx) {
            return has(rbac::resolver::admin::Context<EnumT>{ctx});
        }

        template<typename... ContextTs>
        static bool hasAll(ContextTs &&... ctxs) {
            return (has(std::forward<ContextTs>(ctxs)) && ...);
        }

        template<typename... ContextTs>
        static bool hasAny(ContextTs &&... ctxs) {
            return (has(std::forward<ContextTs>(ctxs)) || ...);
        }
    };
}
