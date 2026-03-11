#pragma once

#include "rbac/permission/template/Set.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace vh::rbac::permission {

template <typename MaskT>
struct Module {
    using Mask = MaskT;

    virtual ~Module() = default;
    Module() = default;

    [[nodiscard]] static constexpr std::size_t bitWidth() {
        return sizeof(Mask) * 8u;
    }

    [[nodiscard]] virtual const char* name() const = 0;
    [[nodiscard]] virtual Mask toMask() const = 0;
    virtual void fromMask(Mask mask) = 0;

protected:
    template <typename EnumT, typename SetMaskT>
    static void append(Mask& dstMask, uint8_t& offset, const Set<EnumT, SetMaskT>& set, const char* context) {
        constexpr auto setBits = Set<EnumT, SetMaskT>::bitWidth();

        if (offset + setBits > bitWidth())
            throw std::runtime_error(std::string(context) + ": bit width exceeds destination mask");

        dstMask |= static_cast<Mask>(set.raw()) << offset;
        offset += static_cast<uint8_t>(setBits);
    }

    template <typename EnumT, typename SetMaskT>
    static void extract(const Mask srcMask, uint8_t& offset, Set<EnumT, SetMaskT>& set, const char* context) {
        constexpr auto setBits = Set<EnumT, SetMaskT>::bitWidth();

        if (offset + setBits > bitWidth())
            throw std::runtime_error(std::string(context) + ": bit width exceeds source mask");

        constexpr Mask fieldMask =
            setBits >= bitWidth()
                ? static_cast<Mask>(~static_cast<Mask>(0))
                : static_cast<Mask>((static_cast<Mask>(1) << setBits) - 1);

        const auto value = static_cast<SetMaskT>((srcMask >> offset) & fieldMask);
        set.setRaw(value);

        offset += static_cast<uint8_t>(setBits);
    }

    template <typename... SetTs>
    [[nodiscard]] Mask pack(const SetTs&... sets) const {
        Mask mask = 0;
        uint8_t offset = 0;
        (append(mask, offset, sets, name()), ...);
        return mask;
    }

    template <typename... SetTs>
    void unpack(const Mask srcMask, SetTs&... sets) const {
        uint8_t offset = 0;
        (extract(srcMask, offset, sets, name()), ...);
    }
};

}
