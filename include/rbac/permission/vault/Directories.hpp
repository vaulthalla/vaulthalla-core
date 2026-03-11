#pragma once

#include "rbac/permission/vault/Share.hpp"
#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Module.hpp"

#include <cstdint>

namespace vh::rbac::permission::vault {

enum class DirectoryPermissions : uint16_t {
    None = 0,
    List = 1 << 0,
    Upload = 1 << 1,
    Download = 1 << 2,
    Touch = 1 << 3,
    Delete = 1 << 4,
    Rename = 1 << 5,
    Move = 1 << 6,
    Copy = 1 << 7,
    All = List | Upload | Download | Touch | Delete | Rename | Move | Copy
};

struct Directories final : Module<uint32_t> {
    static constexpr const auto* ModuleName = "Directories";

    Set<DirectoryPermissions, uint16_t> permissions;
    Share share;

    Directories() = default;
    explicit Directories(const Mask& mask) { fromMask(mask); }

    const char* name() const override { return ModuleName; }

    [[nodiscard]] uint32_t toMask() const override { return pack(permissions, share); }
    void fromMask(const Mask mask) override { unpack(mask, permissions, share); }

    [[nodiscard]] bool canList() const noexcept {
        return permissions.has(DirectoryPermissions::List);
    }

    [[nodiscard]] bool canUpload() const noexcept {
        return permissions.has(DirectoryPermissions::Upload);
    }

    [[nodiscard]] bool canDownload() const noexcept {
        return permissions.has(DirectoryPermissions::Download);
    }

    [[nodiscard]] bool canTouch() const noexcept {
        return permissions.has(DirectoryPermissions::Touch);
    }

    [[nodiscard]] bool canDelete() const noexcept {
        return permissions.has(DirectoryPermissions::Delete);
    }

    [[nodiscard]] bool canRename() const noexcept {
        return permissions.has(DirectoryPermissions::Rename);
    }

    [[nodiscard]] bool canMove() const noexcept {
        return permissions.has(DirectoryPermissions::Move);
    }

    [[nodiscard]] bool canCopy() const noexcept {
        return permissions.has(DirectoryPermissions::Copy);
    }
};

}