#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/Override.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace vh::rbac::permission::vault {

enum class EncryptionKeyPermissions : uint8_t {
    None = 0,
    View = 1 << 0,
    Export = 1 << 1,
    Rotate = 1 << 2,
    All = View | Export | Rotate
};

struct EncryptionKey : Set<EncryptionKeyPermissions, uint8_t> {
    std::vector<std::shared_ptr<Override>> overrides;

    [[nodiscard]] bool canView() const noexcept { return has(EncryptionKeyPermissions::View); }
    [[nodiscard]] bool canExport() const noexcept { return has(EncryptionKeyPermissions::Export); }
    [[nodiscard]] bool canRotate() const noexcept { return has(EncryptionKeyPermissions::Rotate); }
    [[nodiscard]] bool any() const noexcept { return has(EncryptionKeyPermissions::All); }
    [[nodiscard]] bool none() const noexcept { return has(EncryptionKeyPermissions::None); }
};

}
