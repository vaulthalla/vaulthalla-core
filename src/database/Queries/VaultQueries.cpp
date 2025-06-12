#include "database/Queries/VaultQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Vault.hpp"

namespace vh::database {
    void VaultQueries::addVault(const types::Vault& vault) {
        Transactions::exec("VaultQueries::addVault", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("INSERT INTO vaults (name, type, is_active, created_at) VALUES ("
                     + txn.quote(vault.name) + ", "
                     + txn.quote(static_cast<int>(vault.type)) + ", "
                     + txn.quote(vault.is_active) + ", "
                     + txn.quote(vault.created_at) + ") RETURNING id");

            unsigned int vaultId = res[0][0].as<unsigned int>();

            if (vault.type == vh::types::VaultType::Local) {
                txn.exec("INSERT INTO local_disk_vaults (vault_id, storage_backend_id, mount_point) VALUES ("
                         + txn.quote(vaultId) + ", "
                         + txn.quote(static_cast<const vh::types::LocalDiskVault&>(vault).storage_backend_id) + ", "
                         + txn.quote(static_cast<const vh::types::LocalDiskVault&>(vault).mount_point) + ")");
            } else if (vault.type == vh::types::VaultType::S3) {
                const auto& s3Vault = static_cast<const types::S3Vault&>(vault);
                txn.exec("INSERT INTO s3_vaults (vault_id, api_key_id, bucket) VALUES ("
                         + txn.quote(vaultId) + ", "
                         + txn.quote(s3Vault.api_key_id) + ", "
                         + txn.quote(s3Vault.bucket) + ")");
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

    std::vector<types::Vault> VaultQueries::listVaults() {
        return Transactions::exec("VaultQueries::listVaults", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM vaults");
            std::vector<types::Vault> vaults;
            for (const auto& row : res) vaults.emplace_back(row);
            return vaults;
        });
    }

    types::Vault VaultQueries::getVault(unsigned int vaultID) {
        return Transactions::exec("VaultQueries::getVault", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM vaults WHERE id = " + txn.quote(vaultID));
            if (res.empty()) throw std::runtime_error("No vault found with ID: " + std::to_string(vaultID));
            return types::Vault(res[0]);
        });
    }

    bool VaultQueries::localDiskVaultExists() {
        return Transactions::exec("VaultQueries::localDiskVaultExists", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT COUNT(*) FROM vaults WHERE type = " + txn.quote(static_cast<int>(types::VaultType::Local)));
            return res[0][0].as<int>() > 0;
        });
    }

    void VaultQueries::addVolume(unsigned short userID, const types::StorageVolume& volume) {
        Transactions::exec("VaultQueries::addVolume", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec(
                "INSERT INTO storage_volumes (vault_id, name, path_prefix, quota_bytes, created_at) "
                "VALUES ("
                + txn.quote(volume.vault_id) + ", "
                + txn.quote(volume.name) + ", "
                + txn.quote(volume.path_prefix.value_or("")) + ", "
                + txn.quote(volume.quota_bytes.value_or(0ULL)) + ", "
                + txn.quote(volume.created_at) + ") "
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

    std::vector<types::StorageVolume> VaultQueries::listVolumes(unsigned int userId) {
        return Transactions::exec("VaultQueries::listVolumes", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT sv.* FROM storage_volumes sv "
                                        "JOIN user_storage_volumes usv ON sv.id = usv.storage_volume_id "
                                        "WHERE usv.user_id = " + txn.quote(userId));
            std::vector<types::StorageVolume> volumes;
            for (const auto& row : res) volumes.emplace_back(row);
            return volumes;
        });
    }

    types::StorageVolume VaultQueries::getVolume(unsigned int volumeId) {
        return Transactions::exec("VaultQueries::getVolume", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM storage_volumes WHERE id = " + txn.quote(volumeId));
            if (res.empty()) throw std::runtime_error("No storage volume found with ID: " + std::to_string(volumeId));
            return types::StorageVolume(res[0]);
        });
    }
}
