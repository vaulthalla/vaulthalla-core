#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <cstdint>
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission::vault {

enum class APIKeyPermissions : uint8_t {
    None = 0,
    View = 1 << 0,
    ViewSecret = 1 << 1,
    Modify = 1 << 2,
    All = View | ViewSecret | Modify
};

struct APIKey final : Set<APIKeyPermissions, uint8_t> {
    static constexpr const auto* FLAG_CONTEXT = "api-key";

    [[nodiscard]] const char* flagPrefix() const override { return FLAG_CONTEXT; }
    [[nodiscard]] std::string toString(uint8_t indent) const override;

    [[nodiscard]] bool canView() const noexcept { return has(APIKeyPermissions::View); }
    [[nodiscard]] bool canViewSecret() const noexcept { return has(APIKeyPermissions::ViewSecret); }
    [[nodiscard]] bool canModify() const noexcept { return has(APIKeyPermissions::Modify); }
    [[nodiscard]] bool any() const noexcept { return has(APIKeyPermissions::All); }
    [[nodiscard]] bool none() const noexcept { return has(APIKeyPermissions::None); }
};

void to_json(nlohmann::json& j, const APIKey& k);
void from_json(const nlohmann::json& j, APIKey& k);

}

template <>
struct vh::rbac::permission::PermissionTraits<vh::rbac::permission::vault::APIKeyPermissions> {
    using E = PermissionEntry<vault::APIKeyPermissions>;

    static constexpr std::array entries {
        E{vault::APIKeyPermissions::View, "view"},
        E{vault::APIKeyPermissions::ViewSecret, "view_secret"},
        E{vault::APIKeyPermissions::Modify, "modify"}
    };
};
