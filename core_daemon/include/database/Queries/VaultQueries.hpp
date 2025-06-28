#pragma once

#include "types/db/Volume.hpp"
#include "types/db/Vault.hpp"
#include "database/Transactions.hpp"

namespace vh::database {

struct VaultQueries {
    VaultQueries() = default;

    static unsigned int addVault(const std::shared_ptr<types::Vault>& vault);
    static void removeVault(unsigned int vaultId);
    static std::shared_ptr<types::Vault> getVault(unsigned int vaultID);
    static std::vector<std::shared_ptr<types::Vault>> listVaults();

    static bool localDiskVaultExists();

    static unsigned int addVolume(unsigned int userID, const std::shared_ptr<types::Volume>& volume);
    static void removeVolume(unsigned int volumeId);
    static std::vector<std::shared_ptr<types::Volume>> listUserVolumes(unsigned int userId);
    static std::vector<std::shared_ptr<types::Volume>> listVaultVolumes(unsigned int vaultId);
    static std::vector<std::shared_ptr<types::Volume>> listVolumes();
    static std::shared_ptr<types::Volume> getVolume(unsigned int volumeId);
};

} // namespace vh::database
