#pragma once

#include "rbac/permission/Permission.hpp"
#include "rbac/resolver/permission/Fwd.hpp"
#include "rbac/resolver/permission/helpers.hpp"
#include "rbac/resolver/permission/TargetTraits.hpp"
#include "rbac/fs/glob/model/Pattern.hpp"
#include "rbac/fs/glob/Tokenizer.hpp"
#include "rbac/fs/glob/Matcher.hpp"

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <type_traits>

namespace vh::rbac::resolver {
    struct PermissionLookup {
        const permission::Permission *permission{};
        PermissionOperation operation{};
    };

    template<typename RoleT, typename... Enums>
    class PermissionResolver {
        using OverrideSet = decltype(std::declval<role::Vault>().fs.overrides);

    public:
        using Role = RoleT;

        static std::unordered_map<std::string, PermissionLookup>
        buildFlagMap(const std::vector<permission::Permission> &permissions) {
            std::unordered_map<std::string, PermissionLookup> byFlag;

            for (const auto &perm: permissions)
                for (const auto &flag: perm.flags) {
                    const auto op = flag.starts_with("--deny-")
                                        ? PermissionOperation::Revoke
                                        : PermissionOperation::Grant;

                    auto [it, inserted] = byFlag.emplace(flag, PermissionLookup{&perm, op});
                    if (!inserted)
                        throw std::runtime_error("Duplicate permission flag mapping: " + flag);
                }

            return byFlag;
        }

        static bool apply(Role &role, const permission::Permission &perm, const PermissionOperation op) {
            return (dispatchApplyOne<Enums>(role, perm, op) || ...);
        }

        static bool apply(Role &role, const PermissionLookup &lookup) {
            if (!lookup.permission) return false;
            return apply(role, *lookup.permission, lookup.operation);
        }

        static bool applyOverride(
            Role &role,
            const PermissionLookup &lookup,
            const fs::glob::model::Pattern &pattern,
            const bool enabled = true
        ) {
            if (!lookup.permission) return false;
            if (!hasOverrideSupport(*lookup.permission)) return false;
            if (!fs::glob::Tokenizer::isValid(pattern.source)) return false;

            auto vaultRole = requireVaultRole(role);
            OverrideSet &overrideSet = overrides(role);

            permission::Override ov;
            ov.assignment_id = vaultRole->assignment_id;
            ov.permission = *lookup.permission;
            ov.pattern = pattern;
            ov.effect = overrideOptFromPermissionOp(lookup.operation);
            ov.enabled = enabled;

            overrideSet.push_back(ov);
            return true;
        }

        static bool updateOverride(
            Role &role,
            const PermissionLookup &lookup,
            const std::string& target,
            const bool enabled = true
        ) {
            if (!lookup.permission) return false;
            if (!hasOverrideSupport(*lookup.permission)) return false;

            OverrideSet &overrideSet = overrides(role);

            for (auto &ov: overrideSet)
                if (fs::glob::Matcher::matches(ov.pattern, target) && ov.permission.id == lookup.permission->id) {
                    handleUpdateOverride(lookup, ov, enabled);
                    return true;
                }

            return false;
        }

        static bool updateOverride(
            Role &role,
            const PermissionLookup &lookup,
            const uint32_t target,
            const bool enabled = true
        ) {
            if (!lookup.permission) return false;
            if (!hasOverrideSupport(*lookup.permission)) return false;

            OverrideSet &overrideSet = overrides(role);

            for (auto &ov: overrideSet)
                if (ov.id == target && ov.permission.id == lookup.permission->id) {
                    handleUpdateOverride(lookup, ov, enabled);
                    return true;
                }

            return false;
        }

        static bool removeOverride(
            Role &role,
            const uint32_t target
        ) {
            OverrideSet &overrideSet = overrides(role);

            auto it = std::remove_if(overrideSet.begin(), overrideSet.end(),
                                     [&](const permission::Override &ov) {
                                         return ov.id == target;
                                     });

            if (it != overrideSet.end()) {
                overrideSet.erase(it, overrideSet.end());
                return true;
            }

            return false;
        }

        static std::optional<permission::Override> findOverride(
            const Role &role,
            const permission::Permission &perm,
            const std::string &target
        ) {
            if (!hasOverrideSupport(perm)) return std::nullopt;

            const OverrideSet &overrideSet = overrides(role);
            for (const auto &ov: overrideSet)
                if (fs::glob::Matcher::matches(ov.pattern, target) && ov.permission.id == perm.id)
                    return ov;

            return std::nullopt;
        }

        static std::optional<permission::Override> findOverride(
            const Role &role,
            const permission::Permission &perm,
            const uint32_t target
        ) {
            if (!hasOverrideSupport(perm)) return std::nullopt;

            const OverrideSet &overrideSet = overrides(role);
            for (const auto &ov: overrideSet)
                if (ov.id == target && ov.permission.id == perm.id)
                    return ov;

            return std::nullopt;
        }

        static bool has(const Role &role, const permission::Permission &perm) {
            return (dispatchHasOne<Enums>(role, perm) || ...);
        }

        static bool hasOverrideSupport(const permission::Permission &perm) {
            return (dispatchHasOverrideSupportOne<Enums>(perm) || ...);
        }

        static OverrideSet &overrides(Role &role) {
            auto vaultRole = requireVaultRole(role);
            return vaultRole->fs.overrides;
        }

        static const OverrideSet &overrides(const Role &role) {
            auto vaultRole = requireVaultRole(role);
            return vaultRole->fs.overrides;
        }

    private:
        template<typename T>
        using RawT = std::remove_cvref_t<T>;

        static auto &roleObject(Role &role) {
            if constexpr (is_shared_ptr_v<Role>) {
                if (!role) throw std::runtime_error("Permission resolver received null role");
                return *role;
            } else {
                return role;
            }
        }

        static const auto &roleObject(const Role &role) {
            if constexpr (is_shared_ptr_v<Role>) {
                if (!role) throw std::runtime_error("Permission resolver received null role");
                return *role;
            } else {
                return role;
            }
        }

        static auto requireVaultRole(Role &role) {
            if constexpr (is_shared_ptr_v<Role>) {
                if (!role) throw std::runtime_error("Permission override requires a non-null role");

                using Pointee = typename RawT<Role>::element_type;

                if constexpr (std::same_as<Pointee, role::Vault>)
                    return role;
                else {
                    auto vault = std::dynamic_pointer_cast<role::Vault>(role);
                    if (!vault)
                        throw std::runtime_error("Permission override requires a vault role");
                    return vault;
                }
            } else {
                if constexpr (std::derived_from<RawT<Role>, role::Vault>)
                    return static_cast<role::Vault &>(role);
                else
                    throw std::runtime_error("Permission override requires a vault role");
            }
        }

        static auto requireVaultRole(const Role &role) {
            if constexpr (is_shared_ptr_v<Role>) {
                if (!role) throw std::runtime_error("Permission override requires a non-null role");

                using Pointee = typename RawT<Role>::element_type;

                if constexpr (std::same_as<Pointee, role::Vault>)
                    return role;
                else {
                    auto vault = std::dynamic_pointer_cast<const role::Vault>(role);
                    if (!vault)
                        throw std::runtime_error("Permission override requires a vault role");
                    return vault;
                }
            } else {
                if constexpr (std::derived_from<RawT<Role>, role::Vault>)
                    return static_cast<const role::Vault &>(role);
                else
                    throw std::runtime_error("Permission override requires a vault role");
            }
        }

        static bool handleUpdateOverride(
            const PermissionLookup &lookup,
            permission::Override &ov,
            const bool enabled = true
        ) {
            if (!hasOverrideSupport(*lookup.permission)) return false;

            ov.effect = overrideOptFromPermissionOp(lookup.operation);
            ov.enabled = enabled;

            return true;
        }

        template<typename Enum>
        static bool tryApplyDirect(Role &role, const permission::Permission &perm, const PermissionOperation op) {
            using Traits = PermissionTargetTraits<Enum>;
            using RawRole = std::remove_cvref_t<decltype(roleObject(role))>;

            if constexpr (!HasMutableTarget<Traits, RawRole>)
                return false;
            else {
                auto &target = Traits::target(roleObject(role));
                return applyToSet<Enum>(target, perm, op);
            }
        }

        template<typename Enum>
        static bool tryHasDirect(const Role &role, const permission::Permission &perm) {
            using Traits = PermissionTargetTraits<Enum>;
            using RawRole = std::remove_cvref_t<decltype(roleObject(role))>;

            if constexpr (!HasConstTarget<Traits, RawRole>)
                return false;
            else {
                const auto &target = Traits::target(roleObject(role));
                return hasInSet<Enum>(target, perm);
            }
        }

        template<typename Enum>
        static bool tryApplyContext(Role &role, const permission::Permission &perm, const PermissionOperation op) {
            using Traits = PermissionContextPolicyTraits<Enum>;
            using RawRole = std::remove_cvref_t<decltype(roleObject(role))>;

            if constexpr (!HasMutableResolve<Traits, RawRole>)
                return false;
            else {
                const auto parts = splitQualifiedName(perm.qualified_name);
                auto *target = Traits::resolve(roleObject(role), parts);
                if (!target) return false;
                return applyToSet<Enum>(*target, perm, op);
            }
        }

        template<typename Enum>
        static bool tryHasContext(const Role &role, const permission::Permission &perm) {
            using Traits = PermissionContextPolicyTraits<Enum>;
            using RawRole = std::remove_cvref_t<decltype(roleObject(role))>;

            if constexpr (!HasConstResolve<Traits, RawRole>)
                return false;
            else {
                const auto parts = splitQualifiedName(perm.qualified_name);
                const auto *target = Traits::resolve(roleObject(role), parts);
                if (!target) return false;
                return hasInSet<Enum>(*target, perm);
            }
        }

        template<typename Enum>
        static bool tryHasOverrideSupportDirect(const permission::Permission &perm) {
            using Traits = PermissionTargetTraits<Enum>;
            if (perm.enumType != typeid(Enum)) return false;
            return Traits::canOverride;
        }

        template<typename Enum>
        static bool dispatchApplyOne(Role &role, const permission::Permission &perm, const PermissionOperation op) {
            if (perm.enumType != typeid(Enum)) return false;
            if (tryApplyDirect<Enum>(role, perm, op)) return true;
            if (tryApplyContext<Enum>(role, perm, op)) return true;
            return false;
        }

        template<typename Enum>
        static bool dispatchHasOne(const Role &role, const permission::Permission &perm) {
            if (perm.enumType != typeid(Enum)) return false;
            if (tryHasDirect<Enum>(role, perm)) return true;
            if (tryHasContext<Enum>(role, perm)) return true;
            return false;
        }

        template<typename Enum>
        static bool dispatchHasOverrideSupportOne(const permission::Permission &perm) {
            return tryHasOverrideSupportDirect<Enum>(perm);
        }
    };
}
