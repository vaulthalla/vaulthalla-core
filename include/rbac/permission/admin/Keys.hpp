#pragma once

#include "rbac/permission/template/Module.hpp"
#include "rbac/permission/admin/keys/API.hpp"
#include "rbac/permission/admin/keys/Encryption.hpp"

#include <nlohmann/json_fwd.hpp>

namespace vh::rbac::permission::admin {

struct Keys final : Module<uint16_t> {
    static constexpr const auto* ModuleName = "keys";

    keys::APIKeys apiKeys;
    keys::EncryptionKey encryptionKeys;

    Keys() = default;
    explicit Keys(const Mask& mask) { fromMask(mask); }

    [[nodiscard]] std::string toFlagsString() const override;
    [[nodiscard]] std::string toString(uint8_t indent) const override;

    [[nodiscard]] const char* name() const override { return ModuleName; }
    [[nodiscard]] uint16_t toMask() const override;
    void fromMask(Mask mask) override;
};

void to_json(nlohmann::json& j, const Keys& k);
void from_json(const nlohmann::json& j, Keys& k);

}
