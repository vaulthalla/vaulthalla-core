#include "database/Queries/VaultQueries.hpp"
#include "types/db/Vault.hpp"

namespace vh::database {
unsigned int VaultQueries::addVault(const std::shared_ptr<types::Vault>& vault) {
    return Transactions::exec("VaultQueries::addVault", [&](pqxx::work& txn) {
        const auto res = txn.exec("INSERT INTO vault (name, type) VALUES (" + txn.quote(vault->name) + ", " +
                                  txn.quote(types::to_string(vault->type)) + ") RETURNING id");

        if (res.empty()) throw std::runtime_error("Failed to insert vault into database");
        auto vaultId = res[0][0].as<unsigned int>();

        // dynamic cast
        if (auto* localVault = dynamic_cast<const types::LocalDiskVault*>(&*vault)) {
            txn.exec("INSERT INTO local (vault_id, mount_point) VALUES (" + txn.quote(vaultId) + ", " +
                     txn.quote(localVault->mount_point.string()) + ")");
        } else if (auto* s3Vault = dynamic_cast<const types::S3Vault*>(&*vault)) {
            txn.exec("INSERT INTO s3 (vault_id, api_key_id, bucket) VALUES (" + txn.quote(vaultId) + ", " +
                     txn.quote(s3Vault->api_key_id) + ", " + txn.quote(s3Vault->bucket) + ")");
        }

        txn.commit();

        return vaultId;
    });
}

void VaultQueries::removeVault(const unsigned int vaultId) {
    Transactions::exec("VaultQueries::removeVault", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM vault WHERE id = " + txn.quote(vaultId));
        txn.commit();
    });
}

std::shared_ptr<types::Vault> VaultQueries::getVault(unsigned int vaultID) {
    return Transactions::exec("VaultQueries::getVault",
                              [vaultID](pqxx::work& txn) -> std::shared_ptr<types::Vault> {
                                  const auto typeRow =
                                      txn.exec("SELECT type FROM vault WHERE id = " + txn.quote(vaultID)).one_row();

                                  const auto typeStr = typeRow["type"].as<std::string>();

                                  switch (types::from_string(typeStr)) {
                                  case types::VaultType::Local: {
                                      const auto res = txn.exec(
                                          "SELECT * FROM vault "
                                          "INNER JOIN local ON vault.id = local.vault_id "
                                          "WHERE vault.id = " +
                                          txn.quote(vaultID));
                                      return std::make_shared<types::LocalDiskVault>(res.one_row());
                                  }
                                  case types::VaultType::S3: {
                                      const auto res = txn.exec("SELECT * FROM vault "
                                                                "INNER JOIN s3 ON vault.id = s3.vault_id "
                                                                "WHERE vault.id = " +
                                                                txn.quote(vaultID));
                                      return std::make_shared<types::S3Vault>(res.one_row());
                                  }
                                  default: throw std::runtime_error("Unsupported VaultType: " + typeStr);
                                  }
                              });
}

std::vector<std::shared_ptr<types::Vault> > VaultQueries::listVaults() {
    return Transactions::exec("VaultQueries::listVaults", [&](pqxx::work& txn) {
        const auto res = txn.exec(R"SQL(
            SELECT *
            FROM vault
            LEFT JOIN local ON local.vault_id = vault.id
            LEFT JOIN s3         ON s3.vault_id = vault.id
        )SQL");

        std::vector<std::shared_ptr<types::Vault> > vaults;

        for (const auto& row : res) {
            switch (types::from_string(row["type"].as<std::string>())) {
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
        const auto res = txn.exec("SELECT COUNT(*) FROM vaults WHERE type = " +
                                  txn.quote(static_cast<int>(types::VaultType::Local)));
        return res[0][0].as<int>() > 0;
    });
}

unsigned int VaultQueries::addVolume(const unsigned int userID, const std::shared_ptr<types::Volume>& volume) {
    return Transactions::exec("VaultQueries::addVolume", [&](pqxx::work& txn) {
        const auto res = txn.exec("INSERT INTO volume (vault_id, name, path_prefix, quota_bytes) "
                                  "VALUES (" +
                                  txn.quote(volume->vault_id) + ", " + txn.quote(volume->name) + ", " +
                                  txn.quote(volume->path_prefix.string()) + ", " +
                                  txn.quote(volume->quota_bytes.value_or(0ULL)) + ") RETURNING id");

        const auto volumeId = res[0][0].as<unsigned int>();
        txn.exec("INSERT INTO volumes (user_id, volume_id) VALUES (" + txn.quote(userID) + ", " +
                 txn.quote(volumeId) + ")");

        txn.commit();

        return volumeId;
    });
}

void VaultQueries::removeVolume(const unsigned int volumeId) {
    Transactions::exec("VaultQueries::removeVolume", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM volumes WHERE volume_id = " + txn.quote(volumeId));
        txn.exec("DELETE FROM volume WHERE id = " + txn.quote(volumeId));
        txn.commit();
    });
}

std::vector<std::shared_ptr<types::Volume> > VaultQueries::listUserVolumes(const unsigned int userId) {
    return Transactions::exec("VaultQueries::listVolumes", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT sv.* FROM volume sv "
                                  "JOIN volumes vs ON sv.id = vs.volume_id "
                                  "WHERE vs.subject_id = " +
                                  txn.quote(userId));
        std::vector<std::shared_ptr<types::Volume> > volumes;
        for (const auto& row : res) volumes.push_back(std::make_shared<types::Volume>(row));
        return volumes;
    });
}

std::vector<std::shared_ptr<types::Volume> > VaultQueries::listVolumes() {
    return Transactions::exec("VaultQueries::listVolumes", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT sv.* FROM volume sv "
            "JOIN volumes vs ON sv.id = vs.volume_id ");
        std::vector<std::shared_ptr<types::Volume> > volumes;
        for (const auto& row : res) volumes.push_back(std::make_shared<types::Volume>(row));
        return volumes;
    });
}

std::vector<std::shared_ptr<types::Volume> > VaultQueries::listVaultVolumes(const unsigned int vaultId) {
    return Transactions::exec("VaultQueries::listVaultVolumes", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM volume WHERE vault_id = " + txn.quote(vaultId));
        std::vector<std::shared_ptr<types::Volume> > volumes;
        for (const auto& row : res) volumes.push_back(std::make_shared<types::Volume>(row));
        return volumes;
    });
}

std::shared_ptr<types::Volume> VaultQueries::getVolume(const unsigned int volumeId) {
    return Transactions::exec("VaultQueries::getVolume", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM volume WHERE id = " + txn.quote(volumeId));
        if (res.empty()) throw std::runtime_error("No storage volume found with ID: " + std::to_string(volumeId));
        return std::make_shared<types::Volume>(res[0]);
    });
}

} // namespace vh::database