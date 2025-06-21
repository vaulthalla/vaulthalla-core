#include "database/Queries/VaultQueries.hpp"
#include "types/db/Vault.hpp"

namespace vh::database {
unsigned int VaultQueries::addVault(const std::shared_ptr<types::Vault>& vault) {
    return Transactions::exec("VaultQueries::addVault", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("INSERT INTO vaults (name, type) VALUES (" + txn.quote(vault->name) + ", " +
                                    txn.quote(types::to_string(vault->type)) + ") RETURNING id");

        if (res.empty()) throw std::runtime_error("Failed to insert vault into database");
        auto vaultId = res[0][0].as<unsigned int>();

        // dynamic cast
        if (auto* localVault = dynamic_cast<const types::LocalDiskVault*>(&*vault)) {
            txn.exec("INSERT INTO local_disk_vaults (vault_id, mount_point) VALUES (" + txn.quote(vaultId) + ", " +
                     txn.quote(localVault->mount_point.string()) + ")");
        } else if (auto* s3Vault = dynamic_cast<const types::S3Vault*>(&*vault)) {
            txn.exec("INSERT INTO s3_vaults (vault_id, api_key_id, bucket) VALUES (" + txn.quote(vaultId) + ", " +
                     txn.quote(s3Vault->api_key_id) + ", " + txn.quote(s3Vault->bucket) + ")");
        }

        txn.commit();

        return vaultId;
    });
}

void VaultQueries::removeVault(unsigned int vaultId) {
    Transactions::exec("VaultQueries::removeVault", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM vaults WHERE id = " + txn.quote(vaultId));
        txn.commit();
    });
}

std::shared_ptr<types::Vault> VaultQueries::getVault(unsigned int vaultID) {
    return Transactions::exec("VaultQueries::getVault",
                              [vaultID](pqxx::work& txn) -> std::shared_ptr<types::Vault> {
                                  // First, get the vault type
                                  pqxx::row typeRow =
                                      txn.exec("SELECT type FROM vaults WHERE id = " + txn.quote(vaultID)).one_row();

                                  auto typeStr = typeRow["type"].as<std::string>();
                                  types::VaultType vaultType = types::from_string(typeStr);

                                  switch (vaultType) {
                                  case types::VaultType::Local: {
                                      auto res = txn.exec(
                                          "SELECT * FROM vaults "
                                          "INNER JOIN local_disk_vaults ON vaults.id = local_disk_vaults.vault_id "
                                          "WHERE vaults.id = " +
                                          txn.quote(vaultID));
                                      return std::make_shared<types::LocalDiskVault>(res.one_row());
                                  }
                                  case types::VaultType::S3: {
                                      auto res = txn.exec("SELECT * FROM vaults "
                                                          "INNER JOIN s3_vaults ON vaults.id = s3_vaults.vault_id "
                                                          "WHERE vaults.id = " +
                                                          txn.quote(vaultID));
                                      return std::make_shared<types::S3Vault>(res.one_row());
                                  }
                                  default: throw std::runtime_error("Unsupported VaultType: " + typeStr);
                                  }
                              });
}

std::vector<std::shared_ptr<types::Vault> > VaultQueries::listVaults() {
    return Transactions::exec("VaultQueries::listVaults", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec(R"SQL(
            SELECT *
            FROM vaults
            LEFT JOIN local_disk_vaults ON local_disk_vaults.vault_id = vaults.id
            LEFT JOIN s3_vaults         ON s3_vaults.vault_id = vaults.id
        )SQL");

        std::vector<std::shared_ptr<types::Vault> > vaults;

        for (const auto& row : res) {
            auto typeStr = row["type"].as<std::string>();
            auto type = types::from_string(typeStr);

            switch (type) {
            case types::VaultType::Local: {
                vaults.push_back(std::make_shared<types::LocalDiskVault>(row));
                break;
            }
            case types::VaultType::S3: {
                vaults.push_back(std::make_shared<types::S3Vault>(row));
                break;
            }
            default: throw std::runtime_error("Unsupported VaultType in listVaults()");
            }
        }

        return vaults;
    });
}

bool VaultQueries::localDiskVaultExists() {
    return Transactions::exec("VaultQueries::localDiskVaultExists", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT COUNT(*) FROM vaults WHERE type = " +
                                    txn.quote(static_cast<int>(types::VaultType::Local)));
        return res[0][0].as<int>() > 0;
    });
}

unsigned int VaultQueries::addVolume(unsigned int userID, const std::shared_ptr<types::StorageVolume>& volume) {
    return Transactions::exec("VaultQueries::addVolume", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("INSERT INTO storage_volumes (vault_id, name, path_prefix, quota_bytes) "
                                    "VALUES (" +
                                    txn.quote(volume->vault_id) + ", " + txn.quote(volume->name) + ", " +
                                    txn.quote(volume->path_prefix.string()) + ", " +
                                    txn.quote(volume->quota_bytes.value_or(0ULL)) + ") RETURNING id");

        auto volumeId = res[0][0].as<unsigned int>();
        txn.exec("INSERT INTO user_storage_volumes (user_id, storage_volume_id) VALUES (" + txn.quote(userID) + ", " +
                 txn.quote(volumeId) + ")");

        txn.commit();

        return volumeId;
    });
}

void VaultQueries::removeVolume(unsigned int volumeId) {
    Transactions::exec("VaultQueries::removeVolume", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM user_storage_volumes WHERE storage_volume_id = " + txn.quote(volumeId));
        txn.exec("DELETE FROM storage_volumes WHERE id = " + txn.quote(volumeId));
        txn.commit();
    });
}

std::vector<std::shared_ptr<types::StorageVolume> > VaultQueries::listUserVolumes(unsigned int userId) {
    return Transactions::exec("VaultQueries::listVolumes", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT sv.* FROM storage_volumes sv "
                                    "JOIN user_storage_volumes usv ON sv.id = usv.storage_volume_id "
                                    "WHERE usv.user_id = " +
                                    txn.quote(userId));
        std::vector<std::shared_ptr<types::StorageVolume> > volumes;
        for (const auto& row : res) volumes.push_back(std::make_shared<types::StorageVolume>(row));
        return volumes;
    });
}

std::vector<std::shared_ptr<types::StorageVolume> > VaultQueries::listVolumes() {
    return Transactions::exec("VaultQueries::listVolumes", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT sv.* FROM storage_volumes sv "
            "JOIN user_storage_volumes usv ON sv.id = usv.storage_volume_id ");
        std::vector<std::shared_ptr<types::StorageVolume> > volumes;
        for (const auto& row : res) volumes.push_back(std::make_shared<types::StorageVolume>(row));
        return volumes;
    });
}

std::vector<std::shared_ptr<types::StorageVolume> > VaultQueries::listVaultVolumes(unsigned int vaultId) {
    return Transactions::exec("VaultQueries::listVaultVolumes", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT * FROM storage_volumes WHERE vault_id = " + txn.quote(vaultId));
        std::vector<std::shared_ptr<types::StorageVolume> > volumes;
        for (const auto& row : res) volumes.push_back(std::make_shared<types::StorageVolume>(row));
        return volumes;
    });
}

std::shared_ptr<types::StorageVolume> VaultQueries::getVolume(unsigned int volumeId) {
    return Transactions::exec("VaultQueries::getVolume", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT * FROM storage_volumes WHERE id = " + txn.quote(volumeId));
        if (res.empty()) throw std::runtime_error("No storage volume found with ID: " + std::to_string(volumeId));
        return std::make_shared<types::StorageVolume>(res[0]);
    });
}

std::shared_ptr<types::UserStorageVolume> VaultQueries::getUserVolume(unsigned int volumeId, unsigned int userId) {
    return Transactions::exec("VaultQueries::getUserVolume", [&](pqxx::work& txn) {
        pqxx::result res =
            txn.exec("SELECT * FROM user_storage_volumes WHERE storage_volume_id = " + txn.quote(volumeId) +
                     " AND user_id = " + txn.quote(userId));
        if (res.empty())
            throw std::runtime_error("No user storage volume found for user ID: " + std::to_string(userId) +
                                     " and volume ID: " + std::to_string(volumeId));
        return std::make_shared<types::UserStorageVolume>(res[0]);
    });
}

std::vector<std::shared_ptr<types::UserStorageVolume> > VaultQueries::listUserAssignedVolumes(unsigned int userId) {
    return Transactions::exec("VaultQueries::listUserVolumes", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT usv.* FROM user_storage_volumes usv "
                                    "JOIN storage_volumes sv ON usv.storage_volume_id = sv.id "
                                    "WHERE usv.user_id = " +
                                    txn.quote(userId));
        std::vector<std::shared_ptr<types::UserStorageVolume> > userVolumes;
        for (const auto& row : res) userVolumes.push_back(std::make_shared<types::UserStorageVolume>(row));
        return userVolumes;
    });
}
} // namespace vh::database