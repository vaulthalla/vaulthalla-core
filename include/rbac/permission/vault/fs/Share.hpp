#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <cstdint>
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission {
    namespace vault::fs {
        enum class SharePermissions : uint8_t {
            None = 0,
            Internal = 1 << 0,
            Public = 1 << 1,
            PublicWithValidation = 1 << 2,
            All = Internal | Public | PublicWithValidation
        };
    }

    template<>
    struct PermissionTraits<vault::fs::SharePermissions> {
        using Entry = PermissionEntry<vault::fs::SharePermissions>;

        static constexpr std::array entries{
            Entry{vault::fs::SharePermissions::Internal, "internal", "Allows sharing with other users within the same vault."},
            Entry{vault::fs::SharePermissions::Public, "public", "Allows sharing with users outside the vault without validation."},
            Entry{vault::fs::SharePermissions::PublicWithValidation, "public_with_val", "Allows validation with valid public authority."},
        };
    };

    namespace vault::fs {
        struct Share final : Set<SharePermissions, uint8_t> {
            static constexpr const auto *FLAG_PREFIX = "share";

            Share() = default;

            explicit Share(const Mask &mask) : Set(mask) {
            }

            [[nodiscard]] const char *flagPrefix() const override { return FLAG_PREFIX; }

            [[nodiscard]] std::string toString(uint8_t indent) const override;

            [[nodiscard]] bool canShareInternally() const noexcept {
                return has(SharePermissions::Internal);
            }

            [[nodiscard]] bool canSharePublicly() const noexcept {
                return has(SharePermissions::Public);
            }

            [[nodiscard]] bool canSharePubliclyWithValidation() const noexcept {
                return has(SharePermissions::PublicWithValidation);
            }

            [[nodiscard]] bool all() const noexcept {
                return has(SharePermissions::All);
            }

            [[nodiscard]] bool none() const noexcept {
                return has(SharePermissions::None);
            }
        };

        void to_json(nlohmann::json &j, const Share &s);

        void from_json(const nlohmann::json &j, Share &s);
    }
}
