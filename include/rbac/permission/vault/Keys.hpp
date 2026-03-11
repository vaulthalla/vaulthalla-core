#pragma once

#include "rbac/permission/template/Module.hpp"
#include "rbac/permission/vault/APIKey.hpp"
#include "rbac/permission/vault/EncryptionKey.hpp"

namespace vh::rbac::permission::vault {

struct Keys final : Module<uint16_t> {
    static constexpr const auto* ModuleName = "Keys";
    APIKey apiKey;
    EncryptionKey encryptionKey;

    const char* name() const override { return ModuleName; }
    [[nodiscard]] uint16_t toMask() const override { return pack(apiKey, encryptionKey); }
    void fromMask(const Mask mask) override { unpack(mask, apiKey, encryptionKey); }
};

}
