#include "database/DBConnection.hpp"
#include "config/Config.hpp"
#include "config/ConfigRegistry.hpp"

namespace vh::database {

DBConnection::DBConnection() {
    const auto& config = config::ConfigRegistry::get();
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

    initPreparedUsers();
    initPreparedVaults();
    initPreparedVaultKeys();
    initPreparedAPIKeys();
    initPreparedFsEntries();
    initPreparedFiles();
    initPreparedDirectories();
    initPreparedOperations();
    initPreparedRoles();
    initPreparedUserRoles();
    initPreparedVaultRoles();
    initPreparedPermOverrides();
    initPreparedPermissions();
    initPreparedSync();
    initPreparedCache();
}

void DBConnection::initPreparedUsers() const {
    conn_->prepare("insert_user",
                   "INSERT INTO users (name, email, password_hash, is_active) "
                   "VALUES ($1, $2, $3, $4) RETURNING id");

    conn_->prepare("get_user", "SELECT * FROM users WHERE id = $1");

    conn_->prepare("get_user_by_name", "SELECT * FROM users WHERE name = $1");

    conn_->prepare("get_user_by_refresh_token",
                   "SELECT u.* FROM users u "
                   "JOIN refresh_tokens rt ON u.id = rt.user_id WHERE rt.jti = $1");

    conn_->prepare("get_users", "SELECT * FROM users");

    conn_->prepare("update_user",
                   "UPDATE users SET name = $2, email = $3, password_hash = $4, is_active = $5 "
                   "WHERE id = $1");

    conn_->prepare("update_user_password", "UPDATE users SET password_hash = $2 WHERE id = $1");

    conn_->prepare("update_user_last_login", "UPDATE users SET last_login = NOW() WHERE id = $1");

    conn_->prepare("insert_refresh_token",
                   "INSERT INTO refresh_tokens (jti, user_id, token_hash, ip_address, user_agent) "
                   "VALUES ($1, $2, $3, $4, $5)");

    conn_->prepare("revoke_most_recent_refresh_token",
                   "UPDATE refresh_tokens SET revoked = TRUE "
                   "WHERE jti = (SELECT jti FROM refresh_tokens WHERE user_id = $1 AND revoked = FALSE ORDER BY created_at DESC LIMIT 1)");

    conn_->prepare("delete_refresh_tokens_older_than_7_days",
                   "DELETE FROM refresh_tokens "
                   "WHERE user_id = $1 AND created_at < NOW() - INTERVAL '7 days'");

    conn_->prepare("delete_refresh_tokens_keep_five",
                   "DELETE FROM refresh_tokens "
                   "WHERE user_id = $1 AND created_at >= NOW() - INTERVAL '7 days' "
                   "AND jti NOT IN ("
                   "    SELECT jti FROM refresh_tokens "
                   "    WHERE user_id = $1 AND created_at >= NOW() - INTERVAL '7 days' "
                   "    ORDER BY created_at DESC "
                   "    LIMIT 5"
                   ")");

    conn_->prepare("get_user_id_by_linux_uid", "SELECT id FROM users WHERE linux_uid = $1");

    conn_->prepare("admin_user_exists", "SELECT EXISTS(SELECT 1 FROM users WHERE name = 'admin') AS exists");
}

void DBConnection::initPreparedAPIKeys() const {
    conn_->prepare("list_api_keys", "SELECT * FROM api_keys");

    conn_->prepare("list_user_api_keys",
                   "SELECT * FROM api_keys WHERE user_id = $1");

    conn_->prepare("get_api_key", "SELECT * FROM api_keys WHERE id = $1");

    conn_->prepare("upsert_api_key",
                   "INSERT INTO api_keys (user_id, name, provider, access_key, "
                   "encrypted_secret_access_key, iv, region, endpoint) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7, $8) "
                   "ON CONFLICT (user_id, name, access_key) DO UPDATE SET "
                   "  provider = EXCLUDED.provider, "
                   "  encrypted_secret_access_key = EXCLUDED.encrypted_secret_access_key, "
                   "  iv = EXCLUDED.iv, "
                   "  region = EXCLUDED.region, "
                   "  endpoint = EXCLUDED.endpoint, "
                   "  created_at = CURRENT_TIMESTAMP "
                   "RETURNING id");

    conn_->prepare("remove_api_key",
                   "DELETE FROM api_keys WHERE id = $1");
}

void DBConnection::initPreparedVaultKeys() const {
    conn_->prepare("insert_vault_key",
        "INSERT INTO vault_keys (vault_id, encrypted_key, iv) "
        "VALUES ($1, $2, $3)");

    conn_->prepare("update_vault_key",
        "UPDATE vault_keys "
        "SET encrypted_key = $2, iv = $3, updated_at = NOW() "
        "WHERE vault_id = $1");

    conn_->prepare("get_vault_key", "SELECT * FROM vault_keys WHERE vault_id = $1");

    conn_->prepare("delete_vault_key", "DELETE FROM vault_keys WHERE vault_id = $1");
}

void DBConnection::initPreparedUserRoles() const {
    conn_->prepare("get_user_assigned_role",
                   "SELECT ura.id as assignment_id, ura.user_id, ura.role_id, ura.assigned_at, "
                   "r.name, r.description, r.type, r.created_at, p.permissions::int AS permissions "
                   "FROM user_role_assignments ura "
                   "JOIN role r ON ura.role_id = r.id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE ura.user_id = $1");

    conn_->prepare("assign_user_role",
                   "INSERT INTO user_role_assignments (user_id, role_id) "
                   "VALUES ($1, $2) ON CONFLICT (user_id, role_id) DO NOTHING");

    conn_->prepare("change_user_role", "UPDATE user_role_assignments SET role_id = $2 WHERE user_id = $1");
}

void DBConnection::initPreparedVaults() const {
    conn_->prepare("upsert_vault",
                   R"(INSERT INTO vault (name, type, description, owner_id, mount_point, quota, is_active)
       VALUES ($1, $2, $3, $4, $5, $6, $7)
       ON CONFLICT (name, owner_id)
       DO UPDATE SET
           type = EXCLUDED.type,
           description = EXCLUDED.description,
           quota = EXCLUDED.quota,
           is_active = EXCLUDED.is_active,
           mount_point = EXCLUDED.mount_point
       RETURNING id)");

    conn_->prepare("upsert_s3_bucket",
                   "INSERT INTO s3_buckets (name, api_key_id) VALUES ($1, $2) "
                   "ON CONFLICT (name, api_key_id) DO UPDATE "
                   "SET name = EXCLUDED.name, api_key_id = EXCLUDED.api_key_id "
                   "RETURNING id");

    conn_->prepare("insert_s3_vault", "INSERT INTO s3 (vault_id, bucket_id) VALUES ($1, $2)");

    conn_->prepare("get_vault",
                   "SELECT v.*, s.*, s3b.name AS bucket, s3b.api_key_id AS api_key_id "
                   "FROM vault v "
                   "LEFT JOIN s3 s ON v.id = s.vault_id "
                   "LEFT JOIN s3_buckets s3b ON s.bucket_id = s3b.id "
                   "WHERE v.id = $1");

    conn_->prepare("list_vaults",
                   "SELECT v.*, s.*, s3b.name AS bucket, s3b.api_key_id AS api_key_id "
                   "FROM vault v "
                   "LEFT JOIN s3 s ON v.id = s.vault_id "
                   "LEFT JOIN s3_buckets s3b ON s.bucket_id = s3b.id");

    conn_->prepare("list_user_vaults",
                   "SELECT v.*, s.*, s3b.name AS bucket, s3b.api_key_id AS api_key_id "
                   "FROM vault v "
                   "LEFT JOIN s3 s ON v.id = s.vault_id "
                   "LEFT JOIN s3_buckets s3b ON s.bucket_id = s3b.id "
                   "WHERE v.owner_id = $1");

    conn_->prepare("get_vault_owners_name",
                   "SELECT u.name FROM users u "
                   "JOIN vault v ON u.id = v.owner_id "
                   "WHERE v.id = $1");

    conn_->prepare("get_max_vault_id", "SELECT MAX(id) FROM vault");

    conn_->prepare("get_vault_root_dir_id_by_vault_id", "SELECT id FROM fs_entry WHERE vault_id = $1 AND path = '/'");
}

void DBConnection::initPreparedFsEntries() const {
    conn_->prepare("update_fs_entry_by_inode",
                   R"SQL(
    UPDATE fs_entry
    SET vault_id         = $2,
        parent_id        = $3,
        name             = $4,
        last_modified_by = $5,
        path             = $6,
        fuse_path        = $7,
        mode             = $8,
        owner_uid        = $9,
        group_gid        = $10,
        is_hidden        = $11,
        is_system        = $12,
        updated_at       = CURRENT_TIMESTAMP
    WHERE inode = $1
    RETURNING id
    )SQL");

    conn_->prepare("delete_fs_entry", "DELETE FROM fs_entry WHERE id = $1");

    conn_->prepare("delete_fs_entry_by_path", "DELETE FROM fs_entry WHERE vault_id = $1 AND path = $2");

    conn_->prepare("delete_fs_entry_by_inode", "DELETE FROM fs_entry WHERE inode = $1");

    conn_->prepare("get_fs_entry_by_fuse_path", "SELECT * FROM fs_entry WHERE fuse_path = $1");

    conn_->prepare("get_fs_entry_by_id", "SELECT * FROM fs_entry WHERE id = $1");

    conn_->prepare("get_fs_entry_id_by_fuse_path", "SELECT id FROM fs_entry WHERE fuse_path = $1");

    conn_->prepare("rename_fs_entry",
                   "UPDATE fs_entry SET name = $2, path = $3, fuse_path = $4, parent_id = $5, updated_at = NOW() WHERE id = $1");

    conn_->prepare("get_fs_entry_parent_id", "SELECT parent_id FROM fs_entry WHERE id = $1");

    conn_->prepare("get_fs_entry_parent_id_and_path", "SELECT parent_id, path FROM fs_entry WHERE id = $1");

    conn_->prepare("get_fs_entry_parent_id_by_inode", "SELECT parent_id FROM fs_entry WHERE inode = $1");

    conn_->prepare("get_fs_entry_id_by_path", "SELECT id FROM fs_entry WHERE vault_id = $1 AND path = $2");

    conn_->prepare("fs_entry_exists_by_inode", "SELECT EXISTS(SELECT 1 FROM fs_entry WHERE inode = $1)");

    conn_->prepare("get_next_inode", "SELECT MAX(inode) + 1 FROM fs_entry");

    conn_->prepare("get_fuse_path_from_fs_entry", "SELECT fuse_path FROM fs_entry WHERE id = $1");
}

void DBConnection::initPreparedFiles() const {
    conn_->prepare("upsert_file_by_entry_id",
                   "INSERT INTO files (fs_entry_id, size_bytes, mime_type, content_hash) "
                   "VALUES ($1, $2, $3, $4) "
                   "ON CONFLICT (fs_entry_id) DO UPDATE SET "
                   "size_bytes = EXCLUDED.size_bytes, "
                   "mime_type = EXCLUDED.mime_type, "
                   "content_hash = EXCLUDED.content_hash");

    conn_->prepare("update_file_only",
                   "UPDATE files SET size_bytes = $2, mime_type = $3, content_hash = $4, encryption_iv = $5 "
                   "WHERE fs_entry_id = $1");

    conn_->prepare("upsert_file_full",
                   "WITH upsert_entry AS ("
                   "  INSERT INTO fs_entry (vault_id, parent_id, name, created_by, last_modified_by, path, fuse_path, inode, mode, owner_uid, group_gid, is_hidden, is_system) "
                   "  VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) "
                   "  ON CONFLICT (parent_id, name) DO UPDATE SET "
                   "    last_modified_by = EXCLUDED.last_modified_by, "
                   "    parent_id = EXCLUDED.parent_id, "
                   "    inode = EXCLUDED.inode, "
                   "    mode = EXCLUDED.mode, "
                   "    owner_uid = EXCLUDED.owner_uid, "
                   "    group_gid = EXCLUDED.group_gid, "
                   "    is_hidden = EXCLUDED.is_hidden, "
                   "    is_system = EXCLUDED.is_system, "
                   "    updated_at = NOW() "
                   "  RETURNING id"
                   ") "
                   "INSERT INTO files (fs_entry_id, size_bytes, mime_type, content_hash, encryption_iv) "
                   "SELECT id, $14, $15, $16, $17 FROM upsert_entry "
                   "ON CONFLICT (fs_entry_id) DO UPDATE SET "
                   "  size_bytes = EXCLUDED.size_bytes, "
                   "  mime_type = EXCLUDED.mime_type, "
                   "  content_hash = EXCLUDED.content_hash, "
                   "  encryption_iv = EXCLUDED.encryption_iv "
                   "RETURNING fs_entry_id");

    conn_->prepare("get_file_mime_type",
                   "SELECT f.mime_type FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("mark_file_trashed",
                   "WITH target AS ("
                   "  SELECT fs.id AS fs_entry_id, fs.fuse_path, fs.uuid, fs.vault_id "
                   "  FROM fs_entry fs "
                   "  WHERE fs.vault_id = $1 AND fs.path = $2"
                   "), moved AS ("
                   "  INSERT INTO files_trashed (vault_id, fuse_path, uuid, trashed_at, trashed_by) "
                   "  SELECT vault_id, fuse_path, uuid, NOW(), $3 "
                   "  FROM target"
                   "), removed AS ("
                   "  DELETE FROM files WHERE fs_entry_id IN (SELECT fs_entry_id FROM target)"
                   ") "
                   "DELETE FROM fs_entry WHERE id IN (SELECT fs_entry_id FROM target);"
        );

    conn_->prepare("mark_file_trashed_by_id",
                   "WITH target AS ("
                   "  SELECT fs.id AS fs_entry_id, fs.fuse_path, fs.uuid, fs.vault_id "
                   "  FROM fs_entry fs "
                   "  WHERE fs.id = $1"
                   "), moved AS ("
                   "  INSERT INTO files_trashed (vault_id, fuse_path, uuid, trashed_at, trashed_by) "
                   "  SELECT vault_id, fuse_path, uuid, NOW(), $2 "
                   "  FROM target"
                   "), removed AS ("
                   "  DELETE FROM files WHERE fs_entry_id IN (SELECT fs_entry_id FROM target)"
                   ") "
                   "DELETE FROM fs_entry WHERE id IN (SELECT fs_entry_id FROM target);"
        );

    conn_->prepare("list_trashed_files",
                   "SELECT * FROM files_trashed WHERE vault_id = $1 AND deleted_at IS NULL");

    conn_->prepare("mark_trashed_file_deleted_by_fuse_path_and_uuid",
                   "UPDATE files_trashed SET deleted_at = NOW() "
                   "WHERE fuse_path = $1 AND uuid = $2 AND deleted_at IS NULL");

    conn_->prepare("mark_trashed_file_deleted_by_id",
                   "UPDATE files_trashed SET deleted_at = NOW() "
                   "WHERE id = $1 AND deleted_at IS NULL");

    conn_->prepare("get_file_uuid_by_path", "SELECT uuid FROM fs_entry WHERE vault_id = $1 AND path = $2");

    conn_->prepare("list_files_in_dir",
                   "SELECT f.*, fs.* "
                   "FROM fs_entry fs "
                   "JOIN files f ON fs.id = f.fs_entry_id "
                   "WHERE fs.vault_id = $1 AND fs.path LIKE $2 AND fs.path NOT LIKE $3");

    conn_->prepare("list_files_in_dir_recursive",
                   "SELECT f.*, fs.* "
                   "FROM fs_entry fs "
                   "JOIN files f ON fs.id = f.fs_entry_id "
                   "WHERE fs.vault_id = $1 AND fs.path LIKE $2");

    conn_->prepare("get_file_by_id",
                   "SELECT f.*, fs.* "
                   "FROM fs_entry fs "
                   "JOIN files f ON fs.id = f.fs_entry_id "
                   "WHERE fs.id = $1");

    conn_->prepare("get_file_by_path",
                   "SELECT f.*, fs.* "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("get_file_by_fuse_path",
                   "SELECT f.*, fs.* "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.fuse_path = $1");

    conn_->prepare("get_file_by_inode",
                   "SELECT f.*, fs.* "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.inode = $1");

    conn_->prepare("list_files_in_dir_by_fuse_path",
                   "SELECT f.*, fs.* "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.fuse_path LIKE $1 AND fs.fuse_path NOT LIKE $2 AND fs.fuse_path != '/'");

    conn_->prepare("list_files_in_dir_by_fuse_path_recursive",
                   "SELECT f.*, fs.* "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.fuse_path LIKE $1 AND fs.fuse_path != '/'");

    conn_->prepare("list_files_in_dir_by_parent_id",
                   "SELECT f.*, fs.* "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.parent_id = $1");

    conn_->prepare("list_files_in_dir_recursive_by_parent_id",
                   "WITH RECURSIVE dir_tree AS ("
                   "    SELECT fs.*, f.* "
                   "    FROM fs_entry fs "
                   "    JOIN files f ON fs.id = f.fs_entry_id "
                   "    WHERE fs.parent_id = $1 "
                   "  UNION ALL "
                   "   SELECT fs2.*, f2.* "
                   "   FROM fs_entry fs2 "
                   "   JOIN files f2 ON fs2.id = f2.fs_entry_id "
                   "   JOIN dir_tree dt ON fs2.parent_id = dt.id "
                   ") "
                   "SELECT * FROM dir_tree");

    conn_->prepare("get_file_id_by_path",
                   "SELECT fs.id "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("get_file_size_bytes", "SELECT size_bytes FROM files WHERE fs_entry_id = $1");

    conn_->prepare("get_file_size_by_inode",
                   "SELECT f.size_bytes "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.inode = $1");

    conn_->prepare("get_file_parent_id_and_size",
                   "SELECT fs.parent_id, f.size_bytes "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.id = $1");

    conn_->prepare("get_file_parent_id_and_size_by_path",
                   "SELECT fs.parent_id, f.size_bytes "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("get_file_parent_id_and_size_by_inode",
                   "SELECT fs.parent_id, f.size_bytes "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.inode = $1");

    conn_->prepare("is_file",
                   "SELECT EXISTS (SELECT 1 FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2)");

    conn_->prepare("get_file_encryption_iv",
                   "SELECT f.encryption_iv "
                   "FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("get_file_content_hash",
                   "SELECT content_hash FROM files f "
                   "JOIN fs_entry fs ON f.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("set_file_encryption_iv",
                   R"(UPDATE files f
       SET encryption_iv = $3
       FROM fs_entry fs
       WHERE f.fs_entry_id = fs.id
         AND fs.vault_id = $1
         AND fs.path = $2)");
}

void DBConnection::initPreparedDirectories() const {
    conn_->prepare("update_dir_stats",
                   "UPDATE directories "
                   "SET size_bytes = size_bytes + $2, file_count = file_count + $3, subdirectory_count = subdirectory_count + $4, "
                   "last_modified = NOW() "
                   "WHERE fs_entry_id = $1 RETURNING file_count");

    conn_->prepare("get_dir_file_count", "SELECT file_count FROM directories WHERE fs_entry_id = $1");

    conn_->prepare("delete_empty_dir",
                   "DELETE FROM fs_entry f "
                   "USING directories d "
                   "WHERE f.id = d.fs_entry_id AND f.id = $1 AND d.file_count = 0 AND d.subdirectory_count = 0");

    conn_->prepare("upsert_directory",
                   "WITH inserted AS ( "
                   "  INSERT INTO fs_entry (vault_id, parent_id, name, created_by, last_modified_by, path, fuse_path, inode, mode, owner_uid, group_gid, is_hidden, is_system) "
                   "  VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13) "
                   "  ON CONFLICT (parent_id, name) DO UPDATE SET "
                   "    last_modified_by = EXCLUDED.last_modified_by, "
                   "    parent_id = EXCLUDED.parent_id, "
                   "    inode = EXCLUDED.inode, "
                   "    mode = EXCLUDED.mode, "
                   "    owner_uid = EXCLUDED.owner_uid, "
                   "    group_gid = EXCLUDED.group_gid, "
                   "    is_hidden = EXCLUDED.is_hidden, "
                   "    is_system = EXCLUDED.is_system, "
                   "    updated_at = NOW() "
                   "  RETURNING id "
                   ") "
                   "INSERT INTO directories (fs_entry_id, size_bytes, file_count, subdirectory_count, last_modified) "
                   "SELECT id, $14, $15, $16, NOW() FROM inserted "
                   "ON CONFLICT (fs_entry_id) DO UPDATE SET "
                   "  size_bytes = EXCLUDED.size_bytes, "
                   "  file_count = EXCLUDED.file_count, "
                   "  subdirectory_count = EXCLUDED.subdirectory_count, "
                   "  last_modified = NOW()");

    conn_->prepare("list_directories_in_dir",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.vault_id = $1 AND fs.path LIKE $2 AND fs.path NOT LIKE $3 AND fs.path != '/'");

    conn_->prepare("list_directories_in_dir_recursive",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.vault_id = $1 AND fs.path LIKE $2");

    conn_->prepare("list_directories_in_dir_by_fuse_path",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.fuse_path LIKE $1 AND fs.fuse_path NOT LIKE $2 AND fs.fuse_path != '/'");

    conn_->prepare("list_directories_in_dir_by_fuse_path_recursive",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.fuse_path LIKE $1 AND fs.fuse_path != '/'");

    conn_->prepare("list_dirs_in_dir_by_parent_id",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.parent_id = $1");

    conn_->prepare("list_dirs_in_dir_recursive_by_parent_id",
                   "WITH RECURSIVE dir_tree AS ("
                   "    SELECT fs.*, d.* "
                   "    FROM fs_entry fs "
                   "    JOIN directories d ON fs.id = d.fs_entry_id "
                   "    WHERE fs.parent_id = $1 "
                   "  UNION ALL "
                   "    SELECT fs2.*, d2.* "
                   "    FROM fs_entry fs2 "
                   "    JOIN directories d2 ON fs2.id = d2.fs_entry_id "
                   "    JOIN dir_tree dt ON fs2.parent_id = dt.id "
                   ") "
                   "SELECT * FROM dir_tree");

    conn_->prepare("get_dir_by_id",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.id = $1");

    conn_->prepare("get_dir_by_path",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2");

    conn_->prepare("get_dir_by_fuse_path",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.fuse_path = $1");

    conn_->prepare("get_dir_by_inode",
                   "SELECT fs.*, d.* "
                   "FROM fs_entry fs "
                   "JOIN directories d ON fs.id = d.fs_entry_id "
                   "WHERE fs.inode = $1");

    conn_->prepare("is_directory",
                   "SELECT EXISTS (SELECT 1 FROM directories d "
                   "JOIN fs_entry fs ON d.fs_entry_id = fs.id "
                   "WHERE fs.vault_id = $1 AND fs.path = $2)");
}

void DBConnection::initPreparedOperations() const {
    conn_->prepare("insert_operation",
                   "INSERT INTO operations (fs_entry_id, executed_by, operation, target, status, "
                   "source_path, destination_path) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7) ");

    conn_->prepare("get_pending_operations",
                   "SELECT * FROM operations WHERE status = 'pending' AND fs_entry_id = $1");

    conn_->prepare("list_pending_operations_by_vault",
                   "SELECT * FROM operations WHERE status = 'pending' AND fs_entry_id IN "
                   "(SELECT id FROM fs_entry WHERE vault_id = $1)");

    conn_->prepare("mark_operation_completed_and_update",
                   "UPDATE operations SET status = $2, completed_at = NOW(), error = $3 WHERE id = $1");

    conn_->prepare("delete_operation", "DELETE FROM operations WHERE id = $1");
}

void DBConnection::initPreparedRoles() const {
    conn_->prepare("insert_role",
                   "INSERT INTO role (name, description, type) "
                   "VALUES ($1, $2, $3) RETURNING id");

    conn_->prepare("update_role",
                   "UPDATE role SET name = $2, description = $3, type = $4 "
                   "WHERE id = $1");

    conn_->prepare("delete_role", "DELETE FROM role WHERE id = $1");

    conn_->prepare("get_permissions_type", "SELECT type FROM role WHERE id = $1");

    conn_->prepare("get_role",
                   "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
                   "p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE r.id = $1");

    conn_->prepare("get_role_by_name",
                   "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
                   "p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE r.name = $1");

    conn_->prepare("list_roles",
                   "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
                   "p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN permissions p ON r.id = p.role_id");

    conn_->prepare("list_roles_by_type",
                   "SELECT r.id as role_id, r.name, r.description, r.type, r.created_at, "
                   "p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE r.type = $1");

    conn_->prepare("assign_permission_to_role",
        "INSERT INTO permissions (role_id, permissions) VALUES ($1, $2::bit(16))");
}

void DBConnection::initPreparedPermissions() const {
    conn_->prepare("insert_raw_permission",
        "INSERT INTO permission (bit_position, name, description, category) "
        "VALUES ($1, $2, $3, $4)");

    conn_->prepare("upsert_permission",
                   "INSERT INTO permissions (role_id, permissions) "
                   "VALUES ($1, $2) ON CONFLICT (role_id) DO UPDATE SET permissions = $2");

    conn_->prepare("update_permission", "UPDATE permissions SET permissions = $2 WHERE role_id = $1");

    conn_->prepare("delete_permission", "DELETE FROM permissions WHERE role_id = $1");
}

void DBConnection::initPreparedVaultRoles() const {
    conn_->prepare("remove_vault_role_assignment", "DELETE FROM vault_role_assignments WHERE id = $1");

    conn_->prepare("get_vault_assigned_role",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.id = $1");

    conn_->prepare("get_vault_assigned_roles",
                   "SELECT r.name, r.description, r.type, "
                   "vra.role_id, vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.assigned_at, "
                   "p.permissions::int as permissions "
                   "FROM role r "
                   "JOIN permissions p ON r.id = p.role_id "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "WHERE vra.vault_id = $1");

    conn_->prepare("get_subject_assigned_vault_role",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.subject_type = $1 AND vra.subject_id = $2 AND vra.role_id = $3");

    conn_->prepare("get_subject_assigned_vault_roles",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.subject_type = $1 AND vra.subject_id = $2");

    conn_->prepare("assign_vault_role",
                   "INSERT INTO vault_role_assignments (subject_type, subject_id, vault_id, role_id, assigned_at) "
                   "VALUES ($1, $2, $3, $4, NOW())");

    conn_->prepare("upsert_assigned_vault_role",
                   "INSERT INTO vault_role_assignments (subject_type, vault_id, subject_id, role_id, assigned_at) "
                   "VALUES ($1, $2, $3, $4, NOW()) ON CONFLICT DO NOTHING");
}

void DBConnection::initPreparedPermOverrides() const {
    conn_->prepare("get_permission_override",
                   "SELECT p.*, vpo.enabled, vpo.regex, vpo.assignment_id, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.id = $1");

    conn_->prepare("get_vault_permission_overrides",
                   "SELECT p.*, vpo.enabled, vpo.regex, vpo.assignment_id, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.vault_id = $1");

    conn_->prepare("get_subject_permission_overrides",
                   "SELECT p.*, vpo.enabled, vpo.regex, vpo.assignment_id, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.subject_type = $1 AND vra.subject_id = $2");
}

void DBConnection::initPreparedSync() const {
    conn_->prepare("insert_sync",
                   "INSERT INTO sync (vault_id, interval) "
                   "VALUES ($1, $2) RETURNING id");

    conn_->prepare("insert_sync_and_fsync",
                   "WITH ins AS ("
                   "  INSERT INTO sync (vault_id, interval) "
                   "  VALUES ($1, $2) RETURNING id"
                   ") "
                   "INSERT INTO fsync (sync_id, conflict_policy) "
                   "SELECT id, $3 FROM ins "
                   "RETURNING sync_id as id");

    conn_->prepare("insert_sync_and_rsync",
                   "WITH ins AS ("
                   "  INSERT INTO sync (vault_id, interval) "
                   "  VALUES ($1, $2) RETURNING id"
                   ") "
                   "INSERT INTO rsync (sync_id, conflict_policy, strategy) "
                   "SELECT id, $3, $4 FROM ins "
                   "RETURNING sync_id as id");

    conn_->prepare("update_sync_and_fsync",
                   "WITH updated_sync AS ("
                   "  UPDATE sync SET interval = $2, enabled = $3, "
                   "      last_sync_at = $4, last_success_at = $5, updated_at = NOW() "
                   "  WHERE id = $1 RETURNING id"
                   ") "
                   "UPDATE fsync SET conflict_policy = $6 "
                   "WHERE sync_id = (SELECT id FROM updated_sync)");

    conn_->prepare("update_sync_and_rsync",
                   "WITH updated_sync AS ("
                   "  UPDATE sync SET interval = $2, enabled = $3, "
                   "      last_sync_at = $4, last_success_at = $5, updated_at = NOW() "
                   "  WHERE id = $1 RETURNING id"
                   ") "
                   "UPDATE rsync SET strategy = $6, conflict_policy = $7 "
                   "WHERE sync_id = (SELECT id FROM updated_sync)");

    conn_->prepare("report_sync_started", "UPDATE sync SET last_sync_at = NOW() WHERE id = $1");

    conn_->prepare("report_sync_success",
                   "UPDATE sync SET last_success_at = NOW(), last_sync_at = NOW() WHERE id = $1");

    conn_->prepare("get_fsync_config",
                   "SELECT fs.*, s.* FROM fsync fs JOIN sync s ON s.id = fs.sync_id WHERE vault_id = $1");

    conn_->prepare("get_rsync_config",
                   "SELECT rs.*, s.* FROM rsync rs JOIN sync s ON s.id = rs.sync_id WHERE vault_id = $1");
}

void DBConnection::initPreparedCache() const {
    conn_->prepare("upsert_cache_index",
                   "INSERT INTO cache_index (vault_id, file_id, path, type, size) "
                   "VALUES ($1, $2, $3, $4, $5) "
                   "ON CONFLICT (vault_id, path, type) DO UPDATE "
                   "SET type = EXCLUDED.type, "
                   "    size = EXCLUDED.size, "
                   "    last_accessed = CURRENT_TIMESTAMP");

    conn_->prepare("update_cache_index",
                   "UPDATE cache_index SET path = $2, type = $3, size = $4, last_accessed = NOW() WHERE id = $1");

    conn_->prepare("get_cache_index", "SELECT * FROM cache_index WHERE id = $1");

    conn_->prepare("get_cache_index_by_path", "SELECT * FROM cache_index WHERE vault_id = $1 AND path = $2");

    conn_->prepare("delete_cache_index", "DELETE FROM cache_index WHERE id = $1");

    conn_->prepare("delete_cache_index_by_path", "DELETE FROM cache_index WHERE vault_id = $1 AND path = $2");

    conn_->prepare("list_cache_indices", "SELECT * FROM cache_index WHERE vault_id = $1");

    conn_->prepare("list_cache_indices_by_path_recursive",
                   "SELECT * FROM cache_index WHERE vault_id = $1 AND path LIKE $2");

    conn_->prepare("list_cache_indices_by_path",
                   "SELECT * FROM cache_index WHERE vault_id = $1 AND path LIKE $2 AND path NOT LIKE $3");

    conn_->prepare("list_cache_indices_by_type",
                   "SELECT * FROM cache_index WHERE vault_id = $1 AND type = $2");

    conn_->prepare("list_cache_indices_by_file", "SELECT * FROM cache_index WHERE file_id = $1");

    conn_->prepare("n_largest_cache_indices",
                   "SELECT * FROM cache_index WHERE vault_id = $1 ORDER BY size DESC LIMIT $2");

    conn_->prepare("n_largest_cache_indices_by_path",
                   "SELECT * FROM cache_index WHERE vault_id = $1 "
                   "AND path LIKE $2 AND path NOT LIKE $3 ORDER BY size DESC LIMIT $4");

    conn_->prepare("n_largest_cache_indices_by_path_recursive",
                   "SELECT * FROM cache_index WHERE vault_id = $1 AND path LIKE $2 ORDER BY size DESC LIMIT $3");

    conn_->prepare("n_largest_cache_indices_by_type",
                   "SELECT * FROM cache_index WHERE vault_id = $1 AND type = $2 ORDER BY size DESC LIMIT $3");

    conn_->prepare("cache_index_exists",
                   "SELECT EXISTS (SELECT 1 FROM cache_index WHERE vault_id = $1 AND path = $2)");

    conn_->prepare("count_cache_indices", "SELECT COUNT(*) FROM cache_index WHERE vault_id = $1");

    conn_->prepare("count_cache_indices_by_type", "SELECT COUNT(*) FROM cache_index WHERE vault_id = $1 AND type = $2");
}

} // namespace vh::database