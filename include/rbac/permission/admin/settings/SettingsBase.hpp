#pragma once

#include "rbac/permission/template/Set.hpp"

#include <cstdint>

namespace vh::rbac::permission::admin::settings {

enum class SettingsPermissions : uint8_t {
    None = 0,
    View = 1 << 0,
    Edit = 1 << 1,
    All = View | Edit
};

struct SettingsBase : Set<SettingsPermissions, uint8_t> {
    [[nodiscard]] bool canView() const noexcept { return has(SettingsPermissions::View); }
    [[nodiscard]] bool canEdit() const noexcept { return has(SettingsPermissions::Edit); }
};

}
