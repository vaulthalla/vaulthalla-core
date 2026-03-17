#pragma once

#include <rbac/permission/admin/roles/Base.hpp>

#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission::admin::roles {

    struct Vault : Base {
        static constexpr const auto* FLAG_CONTEXT = "vault";
        static constexpr const auto* DESCRIPTION_CONTEXT = "Vault";

        [[nodiscard]] const char* flagPrefix() const override { return FLAG_CONTEXT; }
        [[nodiscard]] std::string toString(uint8_t indent) const override;
        [[nodiscard]] std::string_view descriptionObject() const override { return DESCRIPTION_CONTEXT; }
    };

    void to_json(nlohmann::json& j, const Vault& admin);
    void from_json(const nlohmann::json& j, Vault& admin);
}
