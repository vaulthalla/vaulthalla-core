#pragma once

#include <memory>
#include <vector>

namespace vh::types {
    struct Vault;
    struct Sync;
}

namespace vh::database {

struct VaultQueries {
    VaultQueries() = default;

    static unsigned int addVault(const std::shared_ptr<types::Vault>& vault,
                                 const std::shared_ptr<types::Sync>& proxySync = nullptr);
    static void removeVault(unsigned int vaultId);
    static std::shared_ptr<types::Vault> getVault(unsigned int vaultID);
    static std::vector<std::shared_ptr<types::Vault>> listVaults();
    static std::vector<std::shared_ptr<types::Vault>> listUserVaults(unsigned int userId);
    static std::string getVaultOwnersName(unsigned int vaultId);

    [[nodiscard]] static unsigned int maxVaultId();

    static bool localDiskVaultExists();
};

} // namespace vh::database
