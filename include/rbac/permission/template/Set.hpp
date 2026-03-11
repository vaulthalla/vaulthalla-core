#pragma once

#include <cstdint>

namespace vh::rbac::permission {

template <typename EnumT, typename MaskT>
struct Set {
    using Enum = EnumT;
    using Mask = MaskT;

    Mask permissions{0};

    [[nodiscard]] static constexpr std::size_t bitWidth() {
        return sizeof(Mask) * 8u;
    }

    [[nodiscard]] constexpr bool has(const Enum perm) const {
        return (permissions & static_cast<Mask>(perm)) != 0;
    }

    constexpr void grant(const Enum perm) {
        permissions |= static_cast<Mask>(perm);
    }

    constexpr void revoke(const Enum perm) {
        permissions &= static_cast<Mask>(~static_cast<Mask>(perm));
    }

    [[nodiscard]] constexpr Mask operator()() const { return permissions; }
    [[nodiscard]] constexpr Mask raw() const { return permissions; }

    constexpr void setRaw(const Mask mask) { permissions = mask; }
    constexpr void clear() { permissions = 0; }
};

}
