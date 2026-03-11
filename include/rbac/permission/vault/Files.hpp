#pragma once

#include "rbac/permission/vault/Share.hpp"
#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Module.hpp"

#include <cstdint>

namespace vh::rbac::permission::vault {

enum class FilePermissions : uint16_t {
    None = 0,
    Preview = 1 << 0,
    Upload = 1 << 1,
    Download = 1 << 2,
    Overwrite = 1 << 3,
    Rename = 1 << 4,
    Delete = 1 << 5,
    Move = 1 << 6,
    Copy = 1 << 7,
    All = Preview | Upload | Download | Overwrite | Rename | Delete | Move
};

struct Files final : Module<uint32_t> {
    static constexpr const auto* ModuleName = "Files";

    Set<FilePermissions, uint16_t> permissions;
    Share share;

    const char* name() const override { return ModuleName; }
    [[nodiscard]] uint32_t toMask() const override { return pack(permissions, share); }
    void fromMask(const Mask mask) override { unpack(mask, permissions, share); }

    [[nodiscard]] bool canPreview() const noexcept { return permissions.has(FilePermissions::Preview); }
    [[nodiscard]] bool canUpload() const noexcept { return permissions.has(FilePermissions::Upload); }
    [[nodiscard]] bool canDownload() const noexcept { return permissions.has(FilePermissions::Download); }
    [[nodiscard]] bool canOverwrite() const noexcept { return permissions.has(FilePermissions::Overwrite); }
    [[nodiscard]] bool canRename() const noexcept { return permissions.has(FilePermissions::Rename); }
    [[nodiscard]] bool canDelete() const noexcept { return permissions.has(FilePermissions::Delete); }
    [[nodiscard]] bool canMove() const noexcept { return permissions.has(FilePermissions::Move); }
    [[nodiscard]] bool canCopy() const noexcept { return permissions.has(FilePermissions::Copy); }
    [[nodiscard]] bool all() const noexcept { return permissions.has(FilePermissions::All); }
    [[nodiscard]] bool none() const noexcept { return permissions.has(FilePermissions::None); }
};

}