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
    initPreparedFiles();
    initPreparedDirectories();
    initPreparedPerms();
}

void DBConnection::initPreparedUsers() const {
    conn_->prepare("insert_user",
        "INSERT INTO users (name, email, password_hash, is_active) "
        "VALUES ($1, $2, $3, $4) RETURNING id");

    conn_->prepare("assign_user_role",
        "INSERT INTO user_roles (user_id, role_id) "
        "VALUES ($1, $2) ON CONFLICT (user_id, role_id) DO NOTHING");

    conn_->prepare("get_user",
        "SELECT u.id, u.name, u.password_hash, u.email, u.is_active, "
        "u.created_at, u.last_login, u.is_active, sp.permissions::int as role_permissions, "
        "r.id as role_id, r.name as role_name, r.description as role_description, r.created_at as role_created_at "
        "FROM users u "
        "JOIN user_roles ur ON u.id = ur.user_id "
        "JOIN simple_permissions sp ON ur.role_id = sp.role_id "
        "JOIN role r ON ur.role_id = r.id "
        "WHERE u.id = $1");

    conn_->prepare("get_user_by_name",
        "SELECT u.id, u.name, u.password_hash, u.email, u.is_active, "
        "u.created_at, u.last_login, u.is_active, sp.permissions::int as role_permissions, "
        "r.id as role_id, r.name as role_name, r.description as role_description, r.created_at as role_created_at "
        "FROM users u "
        "JOIN user_roles ur ON u.id = ur.user_id "
        "JOIN simple_permissions sp ON ur.role_id = sp.role_id "
        "JOIN role r ON ur.role_id = r.id "
        "WHERE u.name = $1");

    conn_->prepare("get_user_by_refresh_token",
        "SELECT u.id, u.name, u.password_hash, u.email, u.is_active, "
        "u.created_at, u.last_login, u.is_active, sp.permissions::int as role_permissions, "
        "r.id as role_id, r.name as role_name, r.description as role_description, r.created_at as role_created_at "
        "FROM users u "
        "JOIN user_roles ur ON u.id = ur.user_id "
        "JOIN simple_permissions sp ON ur.role_id = sp.role_id "
        "JOIN role r ON ur.role_id = r.id "
        "JOIN refresh_tokens rt ON u.id = rt.user_id WHERE rt.jti = $1");

    conn_->prepare("get_users",
        "SELECT u.id, u.name, u.password_hash, u.email, u.is_active, u.created_at, u.last_login, u.is_active, "
        "sp.permissions::int as role_permissions, r.id as role_id, r.name as role_name, "
        "r.description as role_description, r.created_at as role_created_at "
        "FROM users u "
        "JOIN user_roles ur ON u.id = ur.user_id "
        "JOIN simple_permissions sp ON ur.role_id = sp.role_id "
        "JOIN role r ON ur.role_id = r.id ");

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
}


void DBConnection::initPreparedFiles() const {
    conn_->prepare("insert_file",
                   "INSERT INTO files (vault_id, parent_id, name, created_by, last_modified_by, size_bytes, "
                   "mime_type, content_hash, path) "
                   "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)");

    conn_->prepare("update_file",
                   "UPDATE files SET vault_id = $2, parent_id = $3, name = $4, updated_at = NOW(), "
                   "last_modified_by = $5, size_bytes = $6, mime_type = $7, content_hash = $8, path = $9 "
                   "WHERE id = $1");

    conn_->prepare("delete_file", "DELETE FROM files WHERE id = $1");

    conn_->prepare("list_files_in_dir",
                   "SELECT * FROM files WHERE vault_id = $1 AND path LIKE $2");
}

void DBConnection::initPreparedDirectories() const {
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

    conn_->prepare("delete_directory", "DELETE FROM directories WHERE id = $1");

    conn_->prepare("list_directories_in_dir",
                   "SELECT * FROM directories WHERE vault_id = $1 AND path LIKE $2");
}

void DBConnection::initPreparedPerms() const {
    conn_->prepare("insert_role",
        "INSERT INTO role (name, description, simple_permissions) "
        "VALUES ($1, $2, $3)");

    conn_->prepare("insert_simple_permissions",
        "INSERT INTO simple_permissions (role_id, permissions) "
        "VALUES ($1, $2)");

    conn_->prepare("insert_permissions",
        "INSERT INTO permissions (role_id, file_permissions, directory_permissions) "
        "VALUES ($1, $2, $3)");

    conn_->prepare("update_role",
        "UPDATE role SET name = $2, description = $3, simple_permissions = $4 "
        "WHERE id = $1");

    conn_->prepare("update_simple_permissions",
        "UPDATE simple_permissions SET permissions = $2 WHERE role_id = $1");

    conn_->prepare("update_permissions",
        "UPDATE permissions SET file_permissions = $2, directory_permissions = $3 WHERE role_id = $1");

    conn_->prepare("delete_role", "DELETE FROM role WHERE id = $1");

    conn_->prepare("delete_simple_permissions", "DELETE FROM simple_permissions WHERE role_id = $1");

    conn_->prepare("delete_permissions", "DELETE FROM permissions WHERE id = $1");

    conn_->prepare("get_permissions_type", "SELECT simple_permissions FROM role WHERE id = $1");

    conn_->prepare("get_vault_assigned_roles",
        "SELECT r.id, r.name, r.description, r.simple_permissions, "
        "sp.permissions::int as permissions, "
        "pp.file_permissions::int as file_permissions, pp.directory_permissions::int as directory_permissions "
        "FROM role r "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id "
        "JOIN roles rs ON r.id = rs.role_id "
        "WHERE rs.vault_id = $1");

    conn_->prepare("assign_role",
        "INSERT INTO roles (subject_type, vault_id, subject_id, role_id, assigned_at) "
        "VALUES ($1, $2, $3, $4, NOW())");

    conn_->prepare("upsert_assigned_role",
        "INSERT INTO roles (subject_type, vault_id, subject_id, role_id, assigned_at) "
        "VALUES ($1, $2, $3, $4, NOW()) ON CONFLICT DO NOTHING");

    conn_->prepare("get_subject_assigned_role",
        "SELECT rs.id, rs.subject_type, rs.subject_id, rs.role_id, rs.assigned_at, "
        "r.name, r.description, r.simple_permissions, sp.permissions::int AS permissions, "
        "pp.file_permissions::int AS file_permissions, pp.directory_permissions::int AS directory_permissions "
        "FROM role r "
        "JOIN roles rs ON r.id = rs.role_id "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id "
        "WHERE rs.subject_type = $1 AND rs.subject_id = $2 AND rs.role_id = $3");

    conn_->prepare("get_subject_assigned_roles",
        "SELECT rs.id, rs.subject_type, rs.subject_id, rs.role_id, rs.assigned_at, "
        "r.name, r.description, r.simple_permissions, sp.permissions::int AS permissions, "
        "pp.file_permissions::int AS file_permissions, pp.directory_permissions::int AS directory_permissions "
        "FROM role r "
        "JOIN roles rs ON r.id = rs.role_id "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id "
        "WHERE rs.subject_type = $1 AND rs.subject_id = $2");

    conn_->prepare("get_assigned_role",
        "SELECT rs.id, rs.subject_type, rs.subject_id, rs.role_id, rs.assigned_at, "
        "r.name, r.description, r.simple_permissions, sp.permissions::int AS permissions, "
        "pp.file_permissions::int AS file_permissions, pp.directory_permissions::int AS directory_permissions "
        "FROM role r "
        "JOIN roles rs ON r.id = rs.role_id "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id "
        "WHERE rs.id = $1");

    conn_->prepare("get_permission_override",
        "SELECT p.*, po.enabled, po.regex, po.is_file "
        "FROM permission p "
        "JOIN permission_overrides po ON p.id = po.permission_id "
        "JOIN roles ar ON po.role_id = ar.id "
        "WHERE ar.id = $1");

    conn_->prepare("get_vault_permission_overrides",
        "SELECT p.*, po.enabled, po.regex, po.role_id, po.is_file "
        "FROM permission p "
        "JOIN permission_overrides po ON p.id = po.permission_id "
        "JOIN roles ar ON po.role_id = ar.id "
        "WHERE ar.vault_id = $1");

    conn_->prepare("get_subject_permission_overrides",
        "SELECT p.*, po.enabled, po.regex, po.is_file "
        "FROM permission p "
        "JOIN permission_overrides po ON p.id = po.permission_id "
        "JOIN roles ar ON po.role_id = ar.id "
        "WHERE ar.subject_type = $1 AND ar.subject_id = $2");

    conn_->prepare("list_base_roles",
        "SELECT r.*, sp.permissions::int AS permissions, "
        "pp.file_permissions::int AS file_permissions, pp.directory_permissions::int AS directory_permissions "
        "FROM role r "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id");

    conn_->prepare("list_user_base_roles",
        "SELECT r.*, sp.permissions::int AS permissions, "
        "pp.file_permissions::int AS file_permissions, pp.directory_permissions::int AS directory_permissions "
        "FROM role r "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id "
        "WHERE r.type = 'user'");

    conn_->prepare("list_fs_base_roles",
        "SELECT r.*, sp.permissions::int AS permissions, "
        "pp.file_permissions::int AS file_permissions, pp.directory_permissions::int AS directory_permissions "
        "FROM role r "
        "JOIN simple_permissions sp ON r.id = sp.role_id "
        "JOIN permissions pp ON r.id = pp.role_id "
        "WHERE r.type = 'vault'");
}



} // namespace vh::database