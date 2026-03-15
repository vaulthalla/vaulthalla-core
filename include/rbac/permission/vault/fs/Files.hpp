#pragma once

#include "Share.hpp"
#include "rbac/permission/template/ModuleSet.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <cstdint>
#include <array>
#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission {
    namespace vault::fs {
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
            All = Preview | Upload | Download | Overwrite | Rename | Delete | Move | Copy
        };
    }

    template<>
    struct PermissionTraits<vault::fs::FilePermissions> {
        using Entry = PermissionEntry<vault::fs::FilePermissions>;

        static constexpr std::array entries{
            Entry{vault::fs::FilePermissions::Preview, "preview"},
            Entry{vault::fs::FilePermissions::Upload, "upload"},
            Entry{vault::fs::FilePermissions::Download, "download"},
            Entry{vault::fs::FilePermissions::Overwrite, "overwrite"},
            Entry{vault::fs::FilePermissions::Rename, "rename"},
            Entry{vault::fs::FilePermissions::Delete, "delete"},
            Entry{vault::fs::FilePermissions::Move, "move"},
            Entry{vault::fs::FilePermissions::Copy, "copy"},
        };
    };

    namespace vault::fs {
        struct Files final : ModuleSet<uint32_t, FilePermissions, uint16_t> {
            static constexpr const auto *ModuleName = "Files";
            static constexpr const auto *FLAG_PREFIX = "files";

            Share share;

            Files() = default;

            explicit Files(const Mask &mask) { fromMask(mask); }

            [[nodiscard]] std::string toString(uint8_t indent) const override;

            [[nodiscard]] const char *flagPrefix() const override { return FLAG_PREFIX; }

            [[nodiscard]] std::string toFlagsString() const override;

            [[nodiscard]] const char *name() const override { return ModuleName; }
            [[nodiscard]] uint32_t toMask() const override { return pack(permissions, share); }
            void fromMask(const Mask mask) override { unpack(mask, permissions, share); }

            [[nodiscard]] bool canPreview() const noexcept { return has(FilePermissions::Preview); }
            [[nodiscard]] bool canUpload() const noexcept { return has(FilePermissions::Upload); }
            [[nodiscard]] bool canDownload() const noexcept { return has(FilePermissions::Download); }
            [[nodiscard]] bool canOverwrite() const noexcept { return has(FilePermissions::Overwrite); }
            [[nodiscard]] bool canRename() const noexcept { return has(FilePermissions::Rename); }
            [[nodiscard]] bool canDelete() const noexcept { return has(FilePermissions::Delete); }
            [[nodiscard]] bool canMove() const noexcept { return has(FilePermissions::Move); }
            [[nodiscard]] bool canCopy() const noexcept { return has(FilePermissions::Copy); }
            [[nodiscard]] bool none() const noexcept { return raw() == 0; }

            [[nodiscard]] bool all() const noexcept {
                return (raw() & static_cast<SetMask>(FilePermissions::All)) ==
                       static_cast<SetMask>(FilePermissions::All);
            }

            [[nodiscard]] bool canShareInternally() const noexcept { return share.has(SharePermissions::Internal); }
            [[nodiscard]] bool canSharePublicly() const noexcept { return share.has(SharePermissions::Public); }

            [[nodiscard]] bool canSharePubliclyWithVal() const noexcept {
                return share.has(SharePermissions::PublicWithValidation);
            }
        };

        void to_json(nlohmann::json &j, const Files &f);

        void from_json(const nlohmann::json &j, Files &f);
    }
}
