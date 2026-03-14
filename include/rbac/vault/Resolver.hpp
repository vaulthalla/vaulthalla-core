#pragma once

#include "resolver/Context.hpp"
#include "identities/User.hpp"
#include "rbac/role/Vault.hpp"
#include "rbac/permission/Vault.hpp"
#include "rbac/permission/admin/Vaults.hpp"
#include "vault/model/Vault.hpp"
#include "runtime/Deps.hpp"
#include "storage/Engine.hpp"
#include "storage/Manager.hpp"
#include "db/query/identities/User.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

namespace vh::identities { struct User; }

namespace vh::rbac::vault {

class Resolver {
    struct ResolvedVaultContext {
        std::shared_ptr<storage::Engine> engine;
        std::shared_ptr<vh::vault::model::Vault> vault;
        std::shared_ptr<identities::User> owner;

        [[nodiscard]] bool isValid() const {
            return engine && vault && owner;
        }
    };

    template <typename EnumT, typename DirectAccessor, typename SelfAccessor, typename AdminAccessor, typename UserAccessor>
    static bool checkModulePermission(const std::shared_ptr<identities::User>& user,
                                      const ResolvedVaultContext& resolved,
                                      const EnumT permission,
                                      DirectAccessor&& directAccessor,
                                      SelfAccessor&& selfAccessor,
                                      AdminAccessor&& adminAccessor,
                                      UserAccessor&& userAccessor) {
        if (!user || !resolved.isValid()) return false;

        const auto vaultId = resolved.vault->id;

        if (user->hasDirectVaultRole(vaultId)) {
            const auto role = user->getDirectVaultRole(vaultId);
            if (!role) return false;

            if (directAccessor(*role).has(permission))
                return true;
        }

        const auto& globalPerms = user->globalVaultPerms();

        if (user->id == resolved.vault->owner_id)
            return selfAccessor(globalPerms.self).has(permission);

        if (resolved.owner->isAdmin())
            return adminAccessor(globalPerms.admin).has(permission);

        return userAccessor(globalPerms.user).has(permission);
    }

    static std::shared_ptr<storage::Engine> resolveEngine(const std::optional<uint32_t>& vaultId,
                                                          const std::optional<std::filesystem::path>& path) {
        auto& storageManager = runtime::Deps::get().storageManager;
        if (!storageManager) return nullptr;

        if (vaultId)
            return storageManager->getEngine(*vaultId);

        if (path && !path->empty())
            return storageManager->resolveStorageEngine(*path);

        return nullptr;
    }

    static ResolvedVaultContext resolveVaultContext(const std::optional<uint32_t>& vaultId,
                                                    const std::optional<std::filesystem::path>& path) {
        ResolvedVaultContext resolved{};

        resolved.engine = resolveEngine(vaultId, path);
        if (!resolved.engine) return resolved;

        resolved.vault = resolved.engine->vault;
        if (!resolved.vault) return resolved;

        resolved.owner = db::query::identities::User::getUserById(resolved.vault->owner_id);
        return resolved;
    }

public:
    template<typename EnumT>
    static bool has(Context<EnumT>&& ctx) {
        if (!ctx.isValid()) return false;

        const auto& user = ctx.user;
        if (!user) return false;
        if (user->isSuperAdmin()) return true;

        const auto resolved = resolveVaultContext(ctx.vault_id, ctx.path);
        if (!resolved.isValid()) return false;

        switch (ctx.module) {
            case Module::Files:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.files(); },
                    [](const auto& perms) -> const auto& { return perms.files(); },
                    [](const auto& perms) -> const auto& { return perms.files(); },
                    [](const auto& perms) -> const auto& { return perms.files(); }
                );

            case Module::Directory:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.directories(); },
                    [](const auto& perms) -> const auto& { return perms.directories(); },
                    [](const auto& perms) -> const auto& { return perms.directories(); },
                    [](const auto& perms) -> const auto& { return perms.directories(); }
                );

            case Module::API_Keys:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.apiKeys(); },
                    [](const auto& perms) -> const auto& { return perms.apiKeys(); },
                    [](const auto& perms) -> const auto& { return perms.apiKeys(); },
                    [](const auto& perms) -> const auto& { return perms.apiKeys(); }
                );

            case Module::Encryption_Keys:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.encryptionKeys(); },
                    [](const auto& perms) -> const auto& { return perms.encryptionKeys(); },
                    [](const auto& perms) -> const auto& { return perms.encryptionKeys(); },
                    [](const auto& perms) -> const auto& { return perms.encryptionKeys(); }
                );

            case Module::Roles:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.rolesPerms(); },
                    [](const auto& perms) -> const auto& { return perms.roles(); },
                    [](const auto& perms) -> const auto& { return perms.roles(); },
                    [](const auto& perms) -> const auto& { return perms.roles(); }
                );

            case Module::Sync_Config:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.syncConfig(); },
                    [](const auto& perms) -> const auto& { return perms.syncConfig(); },
                    [](const auto& perms) -> const auto& { return perms.syncConfig(); },
                    [](const auto& perms) -> const auto& { return perms.syncConfig(); }
                );

            case Module::Sync_Action:
                return checkModulePermission(
                    user,
                    resolved,
                    ctx.permission,
                    [](const auto& role) -> const auto& { return role.syncActions(); },
                    [](const auto& perms) -> const auto& { return perms.syncActions(); },
                    [](const auto& perms) -> const auto& { return perms.syncActions(); },
                    [](const auto& perms) -> const auto& { return perms.syncActions(); }
                );

            default:
                return false;
        }
    }

    template<typename EnumT>
    static bool has(const Context<EnumT>& ctx) {
        return has(Context<EnumT>{ctx});
    }

    template<typename... ContextTs>
    static bool hasAll(ContextTs&&... ctxs) {
        return (has(std::forward<ContextTs>(ctxs)) && ...);
    }

    template<typename... ContextTs>
    static bool hasAny(ContextTs&&... ctxs) {
        return (has(std::forward<ContextTs>(ctxs)) || ...);
    }
};

}
