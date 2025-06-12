#pragma once

#include "types/Vault.hpp"
#include "types/StorageVolume.hpp"
#include "types/UserStorageVolume.hpp"

namespace vh::database {

    struct VaultQueries {
        VaultQueries() = default;

        static void addVault(const vh::types::Vault& vault);
        static void removeVault(unsigned int backendId);
        static std::vector<vh::types::Vault> listVaults();
        static vh::types::Vault getVault(unsigned int vaultID);

        static bool localDiskVaultExists();

        static void addVolume(unsigned short userID, const vh::types::StorageVolume& volume);
        static void removeVolume(unsigned int volumeId);
        static std::vector<vh::types::StorageVolume> listVolumes(unsigned int userId);
        static vh::types::StorageVolume getVolume(unsigned int volumeId);
    };

}
