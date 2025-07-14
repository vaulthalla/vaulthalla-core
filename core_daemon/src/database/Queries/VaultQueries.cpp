#include "database/Queries/VaultQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Vault.hpp"
#include "types/ProxySync.hpp"

namespace vh::database {
unsigned int VaultQueries::addVault(const std::shared_ptr<types::Vault>& vault,
                                    const std::shared_ptr<types::ProxySync>& proxySync) {
    return Transactions::exec("VaultQueries::addVault", [&](pqxx::work& txn) {
        pqxx::params p{vault->name, to_string(vault->type), vault->description, vault->owner_id};
        const auto vaultId = txn.exec_prepared("insert_vault", p).one_row()["id"].as<unsigned int>();

        if (vault->type == types::VaultType::Local) {
            const auto localVault = std::static_pointer_cast<types::LocalDiskVault>(vault);
            txn.exec_prepared("insert_local_vault", pqxx::params{vaultId, localVault->mount_point.string()});
        } else if (vault->type == types::VaultType::S3) {
            if (!proxySync) throw std::runtime_error("ProxySync must be provided for S3 vaults");
            const auto s3Vault = std::static_pointer_cast<types::S3Vault>(vault);
            txn.exec_prepared("insert_s3_bucket", pqxx::params{s3Vault->bucket, s3Vault->api_key_id});

            pqxx::params sync_params{proxySync->interval.count(), proxySync->conflict_policy, proxySync->strategy};
            const auto syncId = txn.exec_prepared("insert_sync", sync_params).one_row()["id"].as<unsigned int>();

            pqxx::params psync_params{syncId, vaultId, proxySync->enabled, proxySync->last_sync_at, proxySync->last_success_at};
            txn.exec_prepared("insert_proxy_sync", psync_params);

            txn.exec_prepared("insert_s3_vault", pqxx::params{vaultId, s3Vault->api_key_id, s3Vault->bucket});
        }

        pqxx::params dir_params{vaultId, std::nullopt, "/", vault->owner_id, vault->owner_id, "/"};
        const auto dirId = txn.exec_prepared("insert_directory", dir_params).one_row()["id"].as<unsigned int>();

        pqxx::params dir_stats_params{dirId, 0, 0, 0}; // Initialize stats with zero values
        txn.exec_prepared("insert_dir_stats", dir_stats_params);

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
                                  const auto row = txn.exec_prepared("get_vault", pqxx::params{vaultID}).one_row();
                                  const auto typeStr = row["type"].as<std::string>();

                                  switch (types::from_string(typeStr)) {
                                  case types::VaultType::Local: return std::make_shared<types::LocalDiskVault>(row);
                                  case types::VaultType::S3: return std::make_shared<types::S3Vault>(row);
                                  default: throw std::runtime_error("Unsupported VaultType: " + typeStr);
                                  }
                              });
}

std::vector<std::shared_ptr<types::Vault> > VaultQueries::listVaults() {
    return Transactions::exec("VaultQueries::listVaults", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_vaults");

        std::vector<std::shared_ptr<types::Vault>> vaults;

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

std::vector<std::shared_ptr<types::Vault> > VaultQueries::listUserVaults(const unsigned int userId) {
    return Transactions::exec("VaultQueries::listUserVaults", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_user_vaults", pqxx::params{userId});

        std::vector<std::shared_ptr<types::Vault> > vaults;
        for (const auto& row : res) {
            const auto typeStr = row["type"].as<std::string>();
            switch (types::from_string(typeStr)) {
            case types::VaultType::Local: vaults.push_back(std::make_shared<types::LocalDiskVault>(row));
                break;
            case types::VaultType::S3: vaults.push_back(std::make_shared<types::S3Vault>(row));
                break;
            default: throw std::runtime_error("Unsupported VaultType in listUserVaults(): " + typeStr);
            }
        }
        return vaults;
    });
}

bool VaultQueries::localDiskVaultExists() {
    return Transactions::exec("VaultQueries::localDiskVaultExists", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT COUNT(*) FROM vault WHERE type = " +
                                  txn.quote(static_cast<int>(types::VaultType::Local)));
        return res[0][0].as<int>() > 0;
    });
}

std::string VaultQueries::getVaultOwnersName(const unsigned int vaultId) {
    return Transactions::exec("VaultQueries::getVaultOwnersName", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_vault_owners_name", pqxx::params{vaultId}).one_row();
        return row["name"].as<std::string>();
    });
}

} // namespace vh::database