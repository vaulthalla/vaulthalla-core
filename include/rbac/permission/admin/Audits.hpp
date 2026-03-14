#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <cstdint>
#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission::admin {

enum class AuditPermissions : uint8_t {
    None = 0,
    View = 1 << 0,
};

struct Audits final : Set<AuditPermissions, uint8_t> {
    static constexpr const auto* FLAG_CONTEXT = "audit";

    Audits() = default;
    explicit Audits(const Mask& mask) : Set(mask) {}

    [[nodiscard]] const char* flagPrefix() const override { return FLAG_CONTEXT; }
    [[nodiscard]] std::string toString(uint8_t indent) const override;

    [[nodiscard]] bool canView() const noexcept { return has(AuditPermissions::View); }
};

void to_json(nlohmann::json& j, const Audits& a);
void from_json(const nlohmann::json& j, Audits& a);

}

template <>
struct vh::rbac::permission::PermissionTraits<vh::rbac::permission::admin::AuditPermissions> {
    using E = PermissionEntry<admin::AuditPermissions>;

    static constexpr std::array entries {
        E{admin::AuditPermissions::View, "view"},
    };
};
