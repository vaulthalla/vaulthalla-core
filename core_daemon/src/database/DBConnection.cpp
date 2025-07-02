#include "database/DBConnection.hpp"
#include "config/Config.hpp"
#include "config/ConfigRegistry.hpp"

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
                   "INSERT INTO files (vault_id, parent_id, name, created_by, last_modified_by, size_bytes, "
                   "mime_type, content_hash, path) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)");

    conn_->prepare("update_file",
                   "UPDATE files SET vault_id = $2, parent_id = $3, name = $4, updated_at = NOW(), "
                   "last_modified_by = $5, size_bytes = $6, mime_type = $7, content_hash = $8, path = $9 "
                   "WHERE id = $1");

    conn_->prepare("insert_directory",
        "INSERT INTO directories (vault_id, parent_id, name, created_by, last_modified_by, path) "
        "VALUES ($1, $2, $3, $4, $5, $6)");

    conn_->prepare("update_directory",
        "UPDATE directories SET vault_id = $2, parent_id = $3, name = $4, updated_at = NOW(), "
        "last_modified_by = $5, path = $6 WHERE id = $1");

    conn_->prepare("update_dir_stats",
        "UPDATE directory_stats SET size_bytes = $2, file_count = $3, subdirectory_count = $4, "
        "last_modified = NOW() WHERE directory_id = $1");

    conn_->prepare("insert_dir_stats",
        "INSERT INTO directory_stats (directory_id, size_bytes, file_count, subdirectory_count, last_modified) "
        "VALUES ($1, $2, $3, $4, $5)");

    conn_->prepare("delete_file", "DELETE FROM files WHERE id = $1");

    conn_->prepare("delete_directory", "DELETE FROM directories WHERE id = $1");

    conn_->prepare("list_files_in_dir",
                   "SELECT * FROM files WHERE vault_id = $1 AND path LIKE $2");

    conn_->prepare("list_directories_in_dir",
                   "SELECT * FROM directories WHERE vault_id = $1 AND path LIKE $2");)

}

} // namespace vh::database