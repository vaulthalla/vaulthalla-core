#include "database/DBConnection.hpp"
#include "types/config/Config.hpp"
#include "types/config/ConfigRegistry.hpp"

namespace vh::database {

DBConnection::DBConnection() {
    const auto& config = types::config::ConfigRegistry::get();
    const auto db = config.database;
    DB_CONNECTION_STR = "postgresql://" + db.user + ":" + db.password + "@" + db.host + ":" + std::to_string(db.port) +
                        "/" + db.name;
    conn_ = std::make_unique<pqxx::connection>(DB_CONNECTION_STR);
    initPrepared();
}

DBConnection::~DBConnection() { if (conn_ && conn_->is_open()) conn_->close(); }

pqxx::connection& DBConnection::get() const { return *conn_; }

void DBConnection::initPrepared() const {
    if (!conn_ || !conn_->is_open()) throw std::runtime_error("Database connection is not open");

    conn_->prepare("insert_file",
                   "INSERT INTO files (storage_volume_id, parent_id, name, is_directory, mode, uid, gid, created_by, "
                   "current_version_size_bytes, full_path) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)");

    conn_->prepare("update_file",
                   "UPDATE files SET storage_volume_id = $2, parent_id = $3, name = $4, is_directory = $5, "
                   "mode = $6, uid = $7, gid = $8, created_by = $9, current_version_size_bytes = $10, full_path = $11 "
                   "WHERE id = $1");

    conn_->prepare("list_files_in_dir",
                   "SELECT * FROM files WHERE storage_volume_id = $1 AND full_path LIKE $2");

}

} // namespace vh::database