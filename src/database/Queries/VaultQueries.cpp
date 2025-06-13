#include "database/Queries/VaultQueries.hpp"
#include "types/Vault.hpp"

namespace vh::database {
    void VaultQueries::addVault(const types::Vault& vault) {
        Transactions::exec("VaultQueries::addVault", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("INSERT INTO vaults (name, type, is_active, created_at) VALUES ("
                     + txn.quote(vault.name) + ", "
                     + txn.quote(static_cast<int>(vault.type)) + ", "
                     + txn.quote(vault.is_active) + ", "
                     + txn.quote(vault.created_at) + ") RETURNING id");

            auto vaultId = res[0][0].as<unsigned int>();

            // dynamic cast
            if (auto* localVault = dynamic_cast<const vh::types::LocalDiskVault*>(&vault)) {
                txn.exec("INSERT INTO local_disk_vaults (vault_id, storage_backend_id, mount_point) VALUES ("
                         + txn.quote(vaultId) + ", "
                         + txn.quote(localVault->storage_backend_id) + ", "
                         + txn.quote(localVault->mount_point.string()) + ")");
            } else if (auto* s3Vault = dynamic_cast<const vh::types::S3Vault*>(&vault)) {
                txn.exec("INSERT INTO s3_vaults (vault_id, api_key_id, bucket) VALUES ("
                         + txn.quote(vaultId) + ", "
                         + txn.quote(s3Vault->api_key_id) + ", "
                         + txn.quote(s3Vault->bucket) + ")");
            }

            txn.commit();
        });
    }

    void VaultQueries::removeVault(unsigned int vaultId) {
        Transactions::exec("VaultQueries::removeVault", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM vaults WHERE id = " + txn.quote(vaultId));
            txn.commit();
        });
    }

    std::vector<std::unique_ptr<types::Vault>> VaultQueries::listVaults() {
        return Transactions::exec("VaultQueries::listVaults", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM vaults");
            std::vector<std::unique_ptr<types::Vault>> vaults;

            for (const auto& row : res) {
                types::Vault baseVault(row);

                switch (baseVault.type) {
                    case types::VaultType::Local: {
                        pqxx::result localRes = txn.exec("SELECT * FROM local_disk_vaults WHERE vault_id = " + txn.quote(baseVault.id));
                        if (localRes.empty()) throw std::runtime_error("No LocalDiskVault data for vault ID: " + std::to_string(baseVault.id));
                        vaults.push_back(std::make_unique<types::LocalDiskVault>(localRes[0]));
                        break;
                    }
                    case types::VaultType::S3: {
                        pqxx::result s3Res = txn.exec("SELECT * FROM s3_vaults WHERE vault_id = " + txn.quote(baseVault.id));
                        if (s3Res.empty()) throw std::runtime_error("No S3Vault data for vault ID: " + std::to_string(baseVault.id));
                        vaults.push_back(std::make_unique<types::S3Vault>(s3Res[0]));
                        break;
                    }
                    default:
                        throw std::runtime_error("Unsupported VaultType in listVaults()");
                }
            }

            return vaults;
        });
    }

    bool VaultQueries::localDiskVaultExists() {
        return Transactions::exec("VaultQueries::localDiskVaultExists", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT COUNT(*) FROM vaults WHERE type = " + txn.quote(static_cast<int>(types::VaultType::Local)));
            return res[0][0].as<int>() > 0;
        });
    }

    void VaultQueries::addVolume(unsigned short userID, const std::shared_ptr<vh::types::StorageVolume>& volume) {
        Transactions::exec("VaultQueries::addVolume", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec(
                "INSERT INTO storage_volumes (vault_id, name, path_prefix, quota_bytes, created_at) "
                "VALUES ("
                + txn.quote(volume->vault_id) + ", "
                + txn.quote(volume->name) + ", "
                + txn.quote(volume->path_prefix.string()) + ", "
                + txn.quote(volume->quota_bytes.value_or(0ULL)) + ", "
                + txn.quote(volume->created_at) + ") "
                "RETURNING id"
            );

            unsigned int volumeId = res[0][0].as<unsigned int>();
            txn.exec("INSERT INTO user_storage_volumes (user_id, storage_volume_id) VALUES ("
                     + txn.quote(userID) + ", "
                     + txn.quote(volumeId) + ")");
            txn.commit();
        });
    }

    void VaultQueries::removeVolume(unsigned int volumeId) {
        Transactions::exec("VaultQueries::removeVolume", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM user_storage_volumes WHERE storage_volume_id = " + txn.quote(volumeId));
            txn.exec("DELETE FROM storage_volumes WHERE id = " + txn.quote(volumeId));
            txn.commit();
        });
    }

    std::vector<std::shared_ptr<vh::types::StorageVolume>> VaultQueries::listVolumes(unsigned int userId) {
        return Transactions::exec("VaultQueries::listVolumes", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT sv.* FROM storage_volumes sv "
                                        "JOIN user_storage_volumes usv ON sv.id = usv.storage_volume_id "
                                        "WHERE usv.user_id = " + txn.quote(userId));
            std::vector<std::shared_ptr<vh::types::StorageVolume>> volumes;
            for (const auto& row : res) volumes.push_back(std::make_shared<vh::types::StorageVolume>(row));
            return volumes;
        });
    }

    std::shared_ptr<vh::types::StorageVolume> VaultQueries::getVolume(unsigned int volumeId) {
        return Transactions::exec("VaultQueries::getVolume", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM storage_volumes WHERE id = " + txn.quote(volumeId));
            if (res.empty()) throw std::runtime_error("No storage volume found with ID: " + std::to_string(volumeId));
            return std::make_shared<vh::types::StorageVolume>(res[0]);
        });
    }

    std::shared_ptr<vh::types::UserStorageVolume> VaultQueries::getUserVolume(unsigned int volumeId, unsigned int userId) {
        return Transactions::exec("VaultQueries::getUserVolume", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM user_storage_volumes WHERE storage_volume_id = " + txn.quote(volumeId) +
                                        " AND user_id = " + txn.quote(userId));
            if (res.empty()) throw std::runtime_error("No user storage volume found for user ID: " + std::to_string(userId) +
                                                   " and volume ID: " + std::to_string(volumeId));
            return std::make_shared<vh::types::UserStorageVolume>(res[0]);
        });
    }

    std::vector<std::shared_ptr<vh::types::UserStorageVolume>> VaultQueries::listUserVolumes(unsigned int userId) {
        return Transactions::exec("VaultQueries::listUserVolumes", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT usv.* FROM user_storage_volumes usv "
                                        "JOIN storage_volumes sv ON usv.storage_volume_id = sv.id "
                                        "WHERE usv.user_id = " + txn.quote(userId));
            std::vector<std::shared_ptr<vh::types::UserStorageVolume>> userVolumes;
            for (const auto& row : res) userVolumes.push_back(std::make_shared<vh::types::UserStorageVolume>(row));
            return userVolumes;
        });
    }
}
