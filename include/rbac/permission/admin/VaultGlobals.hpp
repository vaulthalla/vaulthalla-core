#pragma once

#include "rbac/role/vault/Global.hpp"

#include <nlohmann/json_fwd.hpp>

namespace pqxx { class result; }

namespace vh::rbac::permission::admin {

    struct VaultGlobals {
        role::vault::Global self{}, admin{}, user{};

        explicit VaultGlobals(const pqxx::result& res);
    };

    void to_json(nlohmann::json& j, const VaultGlobals& v);
    void from_json(const nlohmann::json& j, VaultGlobals& v);

}
