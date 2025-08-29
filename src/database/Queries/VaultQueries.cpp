#include "database/Queries/VaultQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/FSync.hpp"
#include "types/RSync.hpp"
#include "util/u8.hpp"

using namespace vh::database;
using namespace vh::types;

unsigned int VaultQueries::upsertVault(const std::shared_ptr<Vault>& vault,
                                       const std::shared_ptr<Sync>& sync) {
    if (!sync) throw std::invalid_argument("Sync cannot be null on vault creation.");

    return Transactions::exec("VaultQueries::addVault", [&](pqxx::work& txn) {
        const auto exists = vault->id != 0;

        pqxx::params p{
            vault->name,
            to_string(vault->type),
            vault->description,
            vault->owner_id,
            to_utf8_string(vault->mount_point.u8string()),
            vault->quota,
            vault->is_active
        };

        const auto vaultRes = txn.exec_prepared("upsert_vault", p);
        if (vaultRes.empty() || vaultRes.affected_rows() == 0)
            throw std::runtime_error("Failed to upsert vault: " + vault->name);
        const auto vaultId = vaultRes.one_field().as<unsigned int>();

        if (!exists) {
            if (vault->type == VaultType::Local) {
                const auto fSync = std::static_pointer_cast<FSync>(sync);
                pqxx::params sync_params{vaultId, fSync->interval.count(), to_string(fSync->conflict_policy)};
                txn.exec_prepared("insert_sync_and_fsync", sync_params);
            } else if (vault->type == VaultType::S3) {
                const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);

                const auto rSync = std::static_pointer_cast<RSync>(sync);
                pqxx::params sync_params{vaultId, rSync->interval.count(), to_string(rSync->conflict_policy),
                                         to_string(rSync->strategy)};
                txn.exec_prepared("insert_sync_and_rsync", sync_params);
            }
        } else if (sync) {  // exists and sync is provided
            if (vault->type == VaultType::Local) {
                const auto fsync = std::static_pointer_cast<FSync>(sync);
                pqxx::params f{
                    fsync->id,
                    fsync->interval.count(),
                    fsync->enabled,
                    to_string(fsync->conflict_policy)
                };
                txn.exec_prepared("update_sync_and_fsync", f);
            } else if (vault->type == VaultType::S3) {
                const auto rsync = std::static_pointer_cast<RSync>(sync);

                pqxx::params r{
                    rsync->id,
                    rsync->interval.count(),
                    rsync->enabled,
                    to_string(rsync->strategy),
                    to_string(rsync->conflict_policy)
                };
                txn.exec_prepared("update_sync_and_rsync", r);
            }
        }

        if (vault->type == VaultType::S3) {
            const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
            pqxx::params s3p{vaultId, s3Vault->api_key_id, s3Vault->bucket, s3Vault->encrypt_upstream};
            txn.exec_prepared("upsert_s3_vault", s3p);
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

std::shared_ptr<Vault> VaultQueries::getVault(unsigned int vaultID) {
    return Transactions::exec("VaultQueries::getVault",
                              [vaultID](pqxx::work& txn) -> std::shared_ptr<Vault> {
                                  const auto row = txn.exec_prepared("get_vault", pqxx::params{vaultID}).one_row();
                                  const auto typeStr = row["type"].as<std::string>();

                                  switch (from_string(typeStr)) {
                                  case VaultType::Local: return std::make_shared<Vault>(row);
                                  case VaultType::S3: return std::make_shared<S3Vault>(row);
                                  default: throw std::runtime_error("Unsupported VaultType: " + typeStr);
                                  }
                              });
}

std::shared_ptr<Vault> VaultQueries::getVault(const std::string& name, unsigned int ownerId) {
    return Transactions::exec("VaultQueries::getVaultByName", [&](pqxx::work& txn) -> std::shared_ptr<Vault> {
        const auto res = txn.exec_prepared("get_vault_by_name_and_owner", pqxx::params{name, ownerId});
        if (res.empty()) return nullptr;

        const auto row = res.one_row();
        const auto typeStr = row["type"].as<std::string>();

        switch (from_string(typeStr)) {
        case VaultType::Local: return std::make_shared<Vault>(row);
        case VaultType::S3: return std::make_shared<S3Vault>(row);
        default: throw std::runtime_error("Unsupported VaultType in getVaultByName(): " + typeStr);
        }
    });
}

std::vector<std::shared_ptr<Vault> > VaultQueries::listVaults(DBQueryParams&& params) {
    return Transactions::exec("VaultQueries::listVaults", [&](pqxx::work& txn) {
        pqxx::params p{params.sortBy, to_string(params.order), params.limit, params.offset};
        const auto res = txn.exec_prepared("list_vaults", p);

        std::vector<std::shared_ptr<Vault> > vaults;

        for (const auto& row : res) {
            switch (from_string(row["type"].as<std::string>())) {
            case VaultType::Local: {
                vaults.push_back(std::make_shared<Vault>(row));
                break;
            }
            case VaultType::S3: {
                vaults.push_back(std::make_shared<S3Vault>(row));
                break;
            }
            default: throw std::runtime_error("Unsupported VaultType in listVaults()");
            }
        }

        return vaults;
    });
}

std::vector<std::shared_ptr<Vault> > VaultQueries::listUserVaults(const unsigned int userId, DBQueryParams&& params) {
    return Transactions::exec("VaultQueries::listUserVaults", [&](pqxx::work& txn) {
        pqxx::params p{userId, params.sortBy, to_string(params.order), params.limit, params.offset};

        const auto res = txn.exec_prepared("list_user_vaults", p);

        std::vector<std::shared_ptr<Vault> > vaults;
        for (const auto& row : res) {
            const auto typeStr = row["type"].as<std::string>();
            switch (from_string(typeStr)) {
            case VaultType::Local: vaults.push_back(std::make_shared<Vault>(row));
                break;
            case VaultType::S3: vaults.push_back(std::make_shared<S3Vault>(row));
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
                                  txn.quote(static_cast<int>(VaultType::Local)));
        return res[0][0].as<int>() > 0;
    });
}

std::string VaultQueries::getVaultOwnersName(const unsigned int vaultId) {
    return Transactions::exec("VaultQueries::getVaultOwnersName", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_vault_owners_name", pqxx::params{vaultId}).one_row();
        return row["name"].as<std::string>();
    });
}

unsigned int VaultQueries::maxVaultId() {
    return Transactions::exec("VaultQueries::maxVaultId", [](pqxx::work& txn) {
        return txn.exec_prepared("get_max_vault_id").one_field().as<unsigned int>();
    });
}

bool VaultQueries::vaultExists(const std::string& name, const unsigned int ownerId) {
    return Transactions::exec("VaultQueries::vaultExists", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("vault_exists", pqxx::params{name, ownerId});
        return res.one_field().as<bool>();
    });
}

std::shared_ptr<Sync> VaultQueries::getVaultSyncConfig(const unsigned int vaultId) {
    return Transactions::exec("VaultQueries::getVaultSyncConfig", [&](pqxx::work& txn) -> std::shared_ptr<Sync> {
        const auto res = txn.exec_prepared("get_vault_sync_config", pqxx::params{vaultId});
        if (res.empty()) return nullptr;

        const auto isS3 = txn.exec_prepared("is_s3_vault", vaultId).one_field().as<bool>();

        const auto row = res.one_row();
        if (isS3) return std::make_shared<RSync>(row);
        return std::make_shared<FSync>(row);
    });
}

bool VaultQueries::vaultRootExists(const unsigned int vaultId) {
    return Transactions::exec("VaultQueries::vaultRootExists", [&](pqxx::work& txn) {
        return txn.exec_prepared("vault_root_exists", vaultId).one_field().as<bool>();
    });
}

void VaultQueries::updateVaultSync(const std::shared_ptr<Sync>& sync, const VaultType& type) {
    if (!sync) throw std::invalid_argument("Sync cannot be null on vault sync update.");

    Transactions::exec("VaultQueries::updateVaultSync", [&](pqxx::work& txn) {
        if (type == VaultType::Local) {
            const auto fsync = std::static_pointer_cast<FSync>(sync);
            pqxx::params p{fsync->id, fsync->interval.count(), fsync->enabled, to_string(fsync->conflict_policy)};
            txn.exec_prepared("update_fsync", p);
        } else if (type == VaultType::S3) {
            const auto rsync = std::static_pointer_cast<RSync>(sync);
            pqxx::params p{rsync->id, rsync->interval.count(), rsync->enabled, to_string(rsync->strategy),
                           to_string(rsync->conflict_policy)};
            txn.exec_prepared("update_rsync", p);
        } else throw std::runtime_error("Unsupported VaultType in updateVaultSync(): " + to_string(type));
    });
}

std::string VaultQueries::getVaultMountPoint(unsigned int vaultId) {
    return Transactions::exec("VaultQueries::getVaultMountPoint", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("get_vault_mount_point", pqxx::params{vaultId});
        if (res.empty()) throw std::runtime_error("Vault not found for ID: " + std::to_string(vaultId));
        return res.one_field().as<std::string>();
    });
}
