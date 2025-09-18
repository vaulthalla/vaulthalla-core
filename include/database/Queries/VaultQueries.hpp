#pragma once

#include "types/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace vh::types {
    struct Vault;
    struct Sync;
    enum class VaultType;
}

namespace vh::database {

struct VaultQueries {
    VaultQueries() = default;

    static unsigned int upsertVault(const std::shared_ptr<types::Vault>& vault,
                                 const std::shared_ptr<types::Sync>& sync = nullptr);
    static void removeVault(unsigned int vaultId);
    static std::shared_ptr<types::Vault> getVault(unsigned int vaultID);
    static std::shared_ptr<types::Vault> getVault(const std::string& name, unsigned int ownerId);
    static std::vector<std::shared_ptr<types::Vault>> listVaults(const std::optional<types::VaultType>& type = std::nullopt, types::ListQueryParams&& params = {});
    static std::vector<std::shared_ptr<types::Vault>> listUserVaults(unsigned int userId, const std::optional<types::VaultType>& type = std::nullopt, types::ListQueryParams&& params = {});

    static void updateVaultSync(const std::shared_ptr<types::Sync>& sync, const types::VaultType& type);

    [[nodiscard]] static bool vaultExists(const std::string& name, unsigned int ownerId);

    [[nodiscard]] static bool vaultRootExists(unsigned int vaultId);

    [[nodiscard]] static unsigned int maxVaultId();

    static bool localDiskVaultExists();

    [[nodiscard]] static std::string getVaultMountPoint(unsigned int vaultId);

    static std::string getVaultOwnersName(unsigned int vaultId);
    [[nodiscard]] static unsigned int getVaultOwnerId(unsigned int vaultId);
};

} // namespace vh::database
