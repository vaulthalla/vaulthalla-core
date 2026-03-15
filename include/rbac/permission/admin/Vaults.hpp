#pragma once

#include "rbac/permission/template/Set.hpp"
#include "rbac/permission/template/Module.hpp"
#include "rbac/permission/template/Traits.hpp"

#include <nlohmann/json_fwd.hpp>
#include <utility>

namespace pqxx { class result; }

namespace vh::rbac::permission {

    namespace admin {
        enum class VaultPermissions : uint8_t {
            None   = 0,
            View   = 1 << 0,
            Create = 1 << 1,
            Edit   = 1 << 2,
            Remove = 1 << 3
        };
    }

    template <>
    struct PermissionTraits<admin::VaultPermissions> {
        using Entry = PermissionEntry<admin::VaultPermissions>;

        static constexpr std::array entries {
            Entry{admin::VaultPermissions::View, "view"},
            Entry{admin::VaultPermissions::Create, "create"},
            Entry{admin::VaultPermissions::Edit, "edit"},
            Entry{admin::VaultPermissions::Remove, "remove"}
        };
    };

    namespace admin {
        struct Vault : Set<VaultPermissions, uint8_t> {
            std::string flag_prefix;

            explicit Vault(std::string  prefix) : flag_prefix(std::move(prefix)) {}

            [[nodiscard]] const char *flagPrefix() const override { return flag_prefix.c_str(); }
            [[nodiscard]] std::string toString(uint8_t indent) const override;
        };

        struct Vaults : Module<uint32_t> {
            static constexpr const auto* MODULE_NAME = "vault";

            Vault self{std::string(MODULE_NAME) + "-self"};
            Vault admin{std::string(MODULE_NAME) + "-admin"};
            Vault user{std::string(MODULE_NAME) + "-user"};

            explicit Vaults(const Mask mask) { fromMask(mask); }

            [[nodiscard]] const char *name() const override { return MODULE_NAME; }
            [[nodiscard]] std::string toString(uint8_t indent) const override;
            [[nodiscard]] std::string toFlagsString() const override;
            [[nodiscard]] Mask toMask() const override { return pack(self, admin, user); }
            void fromMask(const Mask mask) override { unpack(mask, self, admin, user); }
        };

        void to_json(nlohmann::json& j, const Vault& v);
        void from_json(const nlohmann::json& j, Vault& v);

        void to_json(nlohmann::json& j, const Vaults& v);
        void from_json(const nlohmann::json& j, Vaults& v);
    }

}
