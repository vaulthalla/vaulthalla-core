#include "database/Queries/StorageQueries.hpp"
#include "database/Transactions.hpp"
#include <string>

namespace vh::database {
    void StorageQueries::addStorageVolume(const vh::types::StorageVolume& volume,
                                          const vh::types::UserStorageVolume& userVolume) {
        Transactions::exec("StorageQueries::addStorageVolume", [&](pqxx::work& txn) {

            pqxx::result res = txn.exec(
                    "INSERT INTO storage_volumes (config_id, config_type, name, path_prefix, quota_bytes, created_at) "
                    "VALUES ("
                    + txn.quote(volume.config_id) + ", "
                    + txn.quote(to_string(volume.type)) + ", "
                    + txn.quote(volume.name) + ", "
                    + txn.quote(volume.path_prefix.value_or("")) + ", "
                    + txn.quote(volume.quota_bytes.value_or(0ULL)) + ", "
                    + txn.quote(volume.created_at) + ") "
                     "RETURNING id"
            );

            txn.exec("INSERT INTO user_storage_volumes (user_id, storage_volume_id) VALUES ("
                     + txn.quote(userVolume.user_id) + ", "
                     + txn.quote(res[0][0].as<unsigned short>()) + ")");

            txn.commit();
        });
    }

    void StorageQueries::removeStorageVolume(unsigned int volumeId) {
        Transactions::exec("StorageQueries::removeStorageVolume", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM user_storage_volumes WHERE volume_id = " + txn.quote(volumeId));
            txn.exec("DELETE FROM storage_volumes WHERE id = " + txn.quote(volumeId));
            txn.commit();
        });
    }

    std::vector<vh::types::StorageVolume> StorageQueries::listStorageVolumes(unsigned int userId) {
        return Transactions::exec("StorageQueries::listStorageVolumes", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT sv.* FROM storage_volumes sv "
                                        "JOIN user_storage_volumes usv ON sv.id = usv.volume_id "
                                        "WHERE usv.user_id = " + txn.quote(userId));
            std::vector<vh::types::StorageVolume> volumes;
            for (const auto& row : res) volumes.emplace_back(row);
            return volumes;
        });
    }

    vh::types::StorageVolume StorageQueries::getStorageVolume(unsigned int volumeId) {
        return Transactions::exec("StorageQueries::getStorageVolume", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM storage_volumes WHERE id = " + txn.quote(volumeId));
            if (res.empty()) throw std::runtime_error("No storage volume found with ID: " + std::to_string(volumeId));
            return vh::types::StorageVolume(res[0]);
        });
    }

}
