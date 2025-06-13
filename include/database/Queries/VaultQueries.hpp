#pragma once

#include "types/Vault.hpp"
#include "types/StorageVolume.hpp"
#include "types/UserStorageVolume.hpp"
#include "database/Transactions.hpp"

namespace vh::database {

    struct VaultQueries {
        VaultQueries() = default;

        static void addVault(const vh::types::Vault& vault);
        static void removeVault(unsigned int backendId);
        static std::vector<std::unique_ptr<types::Vault>> listVaults();

        template<class T = vh::types::Vault>
        static std::unique_ptr<T> getVault(unsigned int vaultID) {
            return Transactions::exec("VaultQueries::getVault", [&](pqxx::work& txn) -> std::unique_ptr<T> {
                pqxx::result res = txn.exec("SELECT * FROM vaults WHERE id = " + txn.quote(vaultID));
                if (res.empty()) throw std::runtime_error("No vault found with ID: " + std::to_string(vaultID));

                // Read base Vault to get type
                vh::types::Vault baseVault(res[0]);

                switch (baseVault.type) {
                    case vh::types::VaultType::Local: {
                        pqxx::result localRes = txn.exec("SELECT * FROM local_disk_vaults WHERE vault_id = " + txn.quote(vaultID));
                        if (localRes.empty()) throw std::runtime_error("No LocalDiskVault data found for vault ID: " + std::to_string(vaultID));
                        return std::make_unique<vh::types::LocalDiskVault>(localRes[0]);
                    }
                    case vh::types::VaultType::S3: {
                        pqxx::result s3Res = txn.exec("SELECT * FROM s3_vaults WHERE vault_id = " + txn.quote(vaultID));
                        if (s3Res.empty()) throw std::runtime_error("No S3Vault data found for vault ID: " + std::to_string(vaultID));
                        return std::make_unique<vh::types::S3Vault>(s3Res[0]);
                    }
                    default:
                        throw std::runtime_error("Unsupported VaultType in getVault()");
                }
            });
        }

        static bool localDiskVaultExists();

        static void addVolume(unsigned short userID, const std::shared_ptr<vh::types::StorageVolume>& volume);
        static void removeVolume(unsigned int volumeId);
        static std::vector<std::shared_ptr<vh::types::StorageVolume>> listVolumes(unsigned int userId);
        static std::shared_ptr<vh::types::StorageVolume> getVolume(unsigned int volumeId);

        static std::shared_ptr<vh::types::UserStorageVolume> getUserVolume(unsigned int volumeId, unsigned int userId);
        static std::vector<std::shared_ptr<vh::types::UserStorageVolume>> listUserVolumes(unsigned int userId);
    };

}
