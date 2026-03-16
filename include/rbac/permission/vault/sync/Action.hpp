#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission {
    namespace vault::sync {
        enum class SyncActionPermissions : uint8_t {
            None = 0,
            Trigger = 1 << 0,
            SignWaiver = 1 << 1,
        };
    }

    template<>
    struct PermissionTraits<vault::sync::SyncActionPermissions> {
        using Entry = PermissionEntry<vault::sync::SyncActionPermissions>;

        static constexpr std::array entries{
            Entry{vault::sync::SyncActionPermissions::Trigger, "trigger"},
            Entry{vault::sync::SyncActionPermissions::SignWaiver, "sign_waiver"},
        };
    };

    namespace vault::sync {
        struct Action final : Set<SyncActionPermissions, uint8_t> {
            static constexpr const auto *FLAG_CONTEXT = "action";

            [[nodiscard]] const char *flagPrefix() const override { return FLAG_CONTEXT; }

            [[nodiscard]] std::string toString(uint8_t indent) const override;

            [[nodiscard]] bool canTrigger() const noexcept { return has(SyncActionPermissions::Trigger); }
            [[nodiscard]] bool canSignWaiver() const noexcept { return has(SyncActionPermissions::SignWaiver); }
            [[nodiscard]] bool none() const noexcept { return has(SyncActionPermissions::None); }
        };

        void to_json(nlohmann::json &j, const Action &a);

        void from_json(const nlohmann::json &j, Action &a);
    }
}
