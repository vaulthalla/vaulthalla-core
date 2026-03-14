#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace vh::identities { struct User; }

namespace vh::rbac::vault {

    enum class Module : uint8_t {
        Files,
        Directory,
        API_Keys,
        Encryption_Keys,
        Roles,
        Sync_Config,
        Sync_Action
    };

    template<typename EnumT>
    struct Context {
        std::shared_ptr<identities::User> user;
        Module module{Module::Files};
        EnumT permission{};
        std::optional<uint32_t> vault_id{std::nullopt};
        std::optional<std::filesystem::path> path{std::nullopt};

        [[nodiscard]] bool isValid() const {
            return user && (vault_id || (path && !path->empty()));
        }
    };

}
