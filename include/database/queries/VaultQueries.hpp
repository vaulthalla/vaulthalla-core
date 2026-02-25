#pragma once

#include "database/model/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace vh::vault::model {
    struct Vault;
    enum class VaultType;
}

namespace vh::sync::model {
struct Policy;
}

namespace vh::database {

struct VaultQueries {
    VaultQueries() = default;

    static unsigned int upsertVault(const std::shared_ptr<vault::model::Vault>& vault,
                                 const std::shared_ptr<sync::model::Policy>& sync = nullptr);
    static void removeVault(unsigned int vaultId);
    static std::shared_ptr<vault::model::Vault> getVault(unsigned int vaultID);
    static std::shared_ptr<vault::model::Vault> getVault(const std::string& name, unsigned int ownerId);
    static std::vector<std::shared_ptr<vault::model::Vault>> listVaults(const std::optional<vault::model::VaultType>& type = std::nullopt, model::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<vault::model::Vault>> listUserVaults(unsigned int userId, const std::optional<vault::model::VaultType>& type = std::nullopt, model::ListQueryParams&& params = {});

    static void updateVaultSync(const std::shared_ptr<sync::model::Policy>& sync, const vault::model::VaultType& type);

    [[nodiscard]] static bool vaultExists(const std::string& name, unsigned int ownerId);

    [[nodiscard]] static bool vaultRootExists(unsigned int vaultId);

    [[nodiscard]] static unsigned int maxVaultId();

    static bool localDiskVaultExists();

    [[nodiscard]] static std::string getVaultMountPoint(unsigned int vaultId);

    static std::string getVaultOwnersName(unsigned int vaultId);
    [[nodiscard]] static unsigned int getVaultOwnerId(unsigned int vaultId);
};

} // namespace vh::database
