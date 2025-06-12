#include "database/Queries/MountQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {



    void MountQueries::addLocalMount(const vh::types::LocalDiskConfig& config) {
        vh::database::Transactions::exec("MountQueries::addLocalMount", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO local_disk_configs (storage_backend_id, mount_point) VALUES ("
                     + txn.quote(config.storage_backend_id) + ", "
                     + txn.quote(config.mount_point) + ")");

            txn.commit();
        });
    }

    void MountQueries::addS3Mount(const vh::types::StorageBackend& backend,
                                  const vh::types::S3Config& config) {
        vh::database::Transactions::exec("MountQueries::addS3Mount", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("INSERT INTO storage_backends (name, type, is_active, created_at) VALUES ("
                     + txn.quote(backend.name) + ", "
                     + txn.quote(static_cast<int>(backend.type)) + ", "
                     + txn.quote(backend.is_active) + ", "
                     + txn.quote(backend.created_at) + ") "
                     "RETURNING id");

            txn.exec("INSERT INTO s3_configs (storage_backend_id, api_key_id, bucket) VALUES ("
                     + txn.quote(res[0][0].get<unsigned short>()) + ", "
                     + txn.quote(config.api_key_id) + ", "
                     + txn.quote(config.bucket) + ")");

            txn.commit();
        });
    }

    void MountQueries::removeMount(const std::string& mountName) {
        vh::database::Transactions::exec("MountQueries::removeMount", [&](pqxx::work& txn) {

            txn.exec("DELETE FROM local_disk_configs WHERE storage_backend_id = (SELECT id FROM mounts WHERE name = " + txn.quote(mountName) + ")");

            txn.exec("DELETE FROM s3_configs WHERE storage_backend_id = (SELECT id FROM mounts WHERE name = " + txn.quote(mountName) + ")");

            txn.exec("DELETE FROM mounts WHERE name = " + txn.quote(mountName));

            txn.commit();
        });
    }

    bool MountQueries::mountExists(const std::string& mountName) {
        return vh::database::Transactions::exec("MountQueries::mountExists", [&](pqxx::work& txn) -> bool {
            pqxx::result res = txn.exec("SELECT COUNT(*) FROM mounts WHERE name = " + txn.quote(mountName));
            return res[0][0].as<int>() > 0;
        });
    }

    std::unordered_map<std::string, mount_pair> MountQueries::listMounts() {
        return vh::database::Transactions::exec("MountQueries::listMounts", [&](pqxx::work& txn) -> std::unordered_map<std::string, mount_pair> {
            std::unordered_map<std::string, mount_pair> mounts;

            pqxx::result res = txn.exec("SELECT * FROM mounts");

            txn.conn().prepare("get_local_mounts", "SELECT * FROM local_disk_configs WHERE storage_backend_id = $1");
            txn.conn().prepare("get_s3_mounts", "SELECT * FROM s3_configs WHERE storage_backend_id = $1");

            for (const auto& row : res) {
                vh::types::StorageBackend backend(row);

                if (backend.type == vh::types::StorageBackendType::Local) {
                    pqxx::result cfg_res = txn.exec("get_local_mounts", backend.id);
                    vh::types::LocalDiskConfig config(cfg_res[0]);

                    mounts.emplace(backend.name, std::make_pair(backend, config));

                } else if (backend.type == vh::types::StorageBackendType::S3) {
                    pqxx::result cfg_res = txn.exec("get_s3_mounts", backend.id);
                    vh::types::S3Config config(cfg_res[0]);

                    mounts.emplace(backend.name, std::make_pair(backend, config));
                }
            }

            return mounts;
        });
    }

}
