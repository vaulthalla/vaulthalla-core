#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <cstdint>
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission {
    namespace vault {
        enum class RolePermissions : uint8_t {
            None = 0,
            Assign = 1 << 0,
            Modify = 1 << 1,
            Revoke = 1 << 2,
            All = Assign | Modify | Revoke
        };
    }

    template<>
    struct PermissionTraits<vault::RolePermissions> {
        using Entry = PermissionEntry<vault::RolePermissions>;

        static constexpr std::array entries{
            Entry{vault::RolePermissions::Assign, "assign"},
            Entry{vault::RolePermissions::Modify, "modify"},
            Entry{vault::RolePermissions::Revoke, "revoke"}
        };
    };

    namespace vault {
        struct Roles final : Set<RolePermissions, uint8_t> {
            static constexpr const auto *FLAG_CONTEXT = "roles";

            Roles() = default;

            explicit Roles(const Mask &mask) : Set(mask) {
            }

            [[nodiscard]] const char *flagPrefix() const override { return FLAG_CONTEXT; }

            [[nodiscard]] std::string toString(uint8_t indent) const override;

            [[nodiscard]] bool canAssign() const noexcept { return has(RolePermissions::Assign); }
            [[nodiscard]] bool canModify() const noexcept { return has(RolePermissions::Modify); }
            [[nodiscard]] bool canRevoke() const noexcept { return has(RolePermissions::Revoke); }
            [[nodiscard]] bool any() const noexcept { return has(RolePermissions::All); }
            [[nodiscard]] bool none() const noexcept { return has(RolePermissions::None); }
        };

        void to_json(nlohmann::json &j, const Roles &r);

        void from_json(const nlohmann::json &j, Roles &r);
    }
}
