#pragma once

#include "Share.hpp"
#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/ModuleSet.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <cstdint>
#include <array>
#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission {
    namespace vault::fs {
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
    }

    template<>
    struct PermissionTraits<vault::fs::DirectoryPermissions> {
        using Entry = PermissionEntry<vault::fs::DirectoryPermissions>;

        static constexpr std::array entries{
            Entry{vault::fs::DirectoryPermissions::List, "list"},
            Entry{vault::fs::DirectoryPermissions::Upload, "upload"},
            Entry{vault::fs::DirectoryPermissions::Download, "download"},
            Entry{vault::fs::DirectoryPermissions::Touch, "touch"},
            Entry{vault::fs::DirectoryPermissions::Delete, "delete"},
            Entry{vault::fs::DirectoryPermissions::Rename, "rename"},
            Entry{vault::fs::DirectoryPermissions::Move, "move"},
            Entry{vault::fs::DirectoryPermissions::Copy, "copy"}
        };
    };

    namespace vault::fs {
        struct Directories final : ModuleSet<uint32_t, DirectoryPermissions, uint16_t> {
            static constexpr const auto *ModuleName = "Directories";
            static constexpr const auto *FLAG_PREFIX = "dirs";

            Share share;

            Directories() = default;

            explicit Directories(const Mask &mask) { fromMask(mask); }

            [[nodiscard]]
            [[nodiscard]] std::string toString(uint8_t indent) const override;

            [[nodiscard]] const char *name() const override { return ModuleName; }
            [[nodiscard]] const char *flagPrefix() const override { return FLAG_PREFIX; }

            [[nodiscard]] std::string toFlagsString() const override;

            [[nodiscard]] uint32_t toMask() const override { return packWithOwn(share); }
            void fromMask(const Mask mask) override { unpackWithOwn(mask, share); }

            [[nodiscard]] bool canList() const noexcept { return has(DirectoryPermissions::List); }
            [[nodiscard]] bool canUpload() const noexcept { return has(DirectoryPermissions::Upload); }
            [[nodiscard]] bool canDownload() const noexcept { return has(DirectoryPermissions::Download); }
            [[nodiscard]] bool canTouch() const noexcept { return has(DirectoryPermissions::Touch); }
            [[nodiscard]] bool canDelete() const noexcept { return has(DirectoryPermissions::Delete); }
            [[nodiscard]] bool canRename() const noexcept { return has(DirectoryPermissions::Rename); }
            [[nodiscard]] bool canMove() const noexcept { return has(DirectoryPermissions::Move); }
            [[nodiscard]] bool canCopy() const noexcept { return has(DirectoryPermissions::Copy); }

            [[nodiscard]] bool canShareInternally() const noexcept { return share.has(SharePermissions::Internal); }
            [[nodiscard]] bool canSharePublicly() const noexcept { return share.has(SharePermissions::Public); }

            [[nodiscard]] bool canSharePubliclyWithVal() const noexcept {
                return share.has(SharePermissions::PublicWithValidation);
            }
        };

        void to_json(nlohmann::json &j, const Directories &v);

        void from_json(const nlohmann::json &j, Directories &v);
    }
}
