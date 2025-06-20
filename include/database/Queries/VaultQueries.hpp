#pragma once

#include "../../../shared/include/types/db/Vault.hpp"
#include "../../../shared/include/types/db/StorageVolume.hpp"
#include "../../../shared/include/types/db/UserStorageVolume.hpp"
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
        static std::vector<std::shared_ptr<vh::types::StorageVolume>> listUserVolumes(unsigned int userId);
        static std::vector<std::shared_ptr<vh::types::StorageVolume>> listVaultVolumes(unsigned int vaultId);
        static std::vector<std::shared_ptr<vh::types::StorageVolume>> listVolumes();
        static std::shared_ptr<vh::types::StorageVolume> getVolume(unsigned int volumeId);

        static std::shared_ptr<vh::types::UserStorageVolume> getUserVolume(unsigned int volumeId, unsigned int userId);
        static std::vector<std::shared_ptr<vh::types::UserStorageVolume>> listUserAssignedVolumes(unsigned int userId);
    };

}
