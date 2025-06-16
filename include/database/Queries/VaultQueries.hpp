#pragma once

#include "types/Vault.hpp"
#include "types/StorageVolume.hpp"
#include "types/UserStorageVolume.hpp"
#include "database/Transactions.hpp"

namespace vh::database {

    struct VaultQueries {
        VaultQueries() = default;

        static unsigned int addVault(const std::shared_ptr<vh::types::Vault>& vault);
        static void removeVault(unsigned int vaultId);
        static std::shared_ptr<types::Vault> getVault(unsigned int vaultID);
        static std::vector<std::shared_ptr<types::Vault>> listVaults();

        static bool localDiskVaultExists();

        static unsigned int addVolume(unsigned int userID, const std::shared_ptr<vh::types::StorageVolume>& volume);
        static void removeVolume(unsigned int volumeId);
        static std::vector<std::shared_ptr<vh::types::StorageVolume>> listVolumes(unsigned int userId);
        static std::shared_ptr<vh::types::StorageVolume> getVolume(unsigned int volumeId);

        static std::shared_ptr<vh::types::UserStorageVolume> getUserVolume(unsigned int volumeId, unsigned int userId);
        static std::vector<std::shared_ptr<vh::types::UserStorageVolume>> listUserVolumes(unsigned int userId);
    };

}
