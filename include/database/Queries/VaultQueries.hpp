#pragma once

#include "types/DBQueryParams.hpp"

#include <memory>
#include <vector>

namespace vh::types {
    struct Vault;
    struct Sync;
}

namespace vh::database {

struct VaultQueries {
    VaultQueries() = default;

    static unsigned int upsertVault(const std::shared_ptr<types::Vault>& vault,
                                 const std::shared_ptr<types::Sync>& sync = nullptr);
    static void removeVault(unsigned int vaultId);
    static std::shared_ptr<types::Vault> getVault(unsigned int vaultID);
    static std::vector<std::shared_ptr<types::Vault>> listVaults(types::DBQueryParams&& params = {});
    static std::vector<std::shared_ptr<types::Vault>> listUserVaults(unsigned int userId, types::DBQueryParams&& params = {});
    static std::string getVaultOwnersName(unsigned int vaultId);

    [[nodiscard]] static unsigned int maxVaultId();

    static bool localDiskVaultExists();
};

} // namespace vh::database
