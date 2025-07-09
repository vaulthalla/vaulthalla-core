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
    initPreparedFiles();
    initPreparedDirectories();
    initPreparedRoles();
    initPreparedUserRoles();
    initPreparedVaultRoles();
    initPreparedPermOverrides();
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
    conn_->prepare("insert_vault",
        "INSERT INTO vault (name, type, description, owner_id) "
        "VALUES ($1, $2, $3, $4) RETURNING id");

    conn_->prepare("get_vault",
        "SELECT v.*, l.*, s.* "
        "FROM vault v "
        "LEFT JOIN local l ON v.id = l.vault_id "
        "LEFT JOIN s3 s ON v.id = s.vault_id "
        "WHERE v.id = $1");

    conn_->prepare("list_vaults",
        "SELECT v.*, l.*, s.* "
        "FROM vault v "
        "LEFT JOIN local l ON v.id = l.vault_id "
        "LEFT JOIN s3 s ON v.id = s.vault_id");

    conn_->prepare("list_user_vaults",
        "SELECT v.*, l.*, s.* "
        "FROM vault v "
        "LEFT JOIN local l ON v.id = l.vault_id "
        "LEFT JOIN s3 s ON v.id = s.vault_id "
        "WHERE v.owner_id = $1");

    conn_->prepare("get_vault_owners_name",
        "SELECT u.name FROM users u "
        "JOIN vault v ON u.id = v.owner_id "
        "WHERE v.id = $1");
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

    conn_->prepare("get_file_mime_type", "SELECT mime_type FROM files WHERE vault_id = $1 AND path = $2");

    conn_->prepare("list_files_in_dir",
               "SELECT * FROM files "
               "WHERE vault_id = $1 AND path LIKE $2 AND path NOT LIKE $3");

    conn_->prepare("list_files_in_dir_recursive",
               "SELECT * FROM files WHERE vault_id = $1 AND path LIKE $2");

    conn_->prepare("get_file_id_by_path", "SELECT id FROM files WHERE vault_id = $1 AND path = $2");

    conn_->prepare("get_file_size_bytes", "SELECT size_bytes FROM files WHERE id = $1");

    conn_->prepare("get_file_parent_id_and_size",
        "SELECT parent_id, size_bytes FROM files WHERE id = $1");
}

void DBConnection::initPreparedDirectories() const {
    conn_->prepare("update_dir_stats",
        "UPDATE directory_stats "
        "SET size_bytes = size_bytes + $2, file_count = file_count + $3, subdirectory_count = subdirectory_count + $4, "
        "last_modified = NOW() "
        "WHERE directory_id = $1");

    conn_->prepare("get_dir_parent_id", "SELECT parent_id FROM directories WHERE id = $1");

    conn_->prepare("insert_directory",
                   "INSERT INTO directories (vault_id, parent_id, name, created_by, last_modified_by, path) "
                   "VALUES ($1, $2, $3, $4, $5, $6) RETURNING id");

    conn_->prepare("update_directory",
                   "UPDATE directories SET vault_id = $2, parent_id = $3, name = $4, updated_at = NOW(), "
                   "last_modified_by = $5, path = $6 WHERE id = $1");

    conn_->prepare("insert_dir_stats",
                   "INSERT INTO directory_stats (directory_id, size_bytes, file_count, subdirectory_count, last_modified) "
                   "VALUES ($1, $2, $3, $4, NOW())");

    conn_->prepare("delete_directory", "DELETE FROM directories WHERE id = $1");

    conn_->prepare("list_directories_in_dir",
                   "SELECT d.*, ds.* "
                   "FROM directories d "
                   "JOIN directory_stats ds ON d.id = ds.directory_id "
                   "WHERE d.vault_id = $1 AND d.path LIKE $2 AND d.path NOT LIKE $3 AND d.path != '/'");

    conn_->prepare("list_directories_in_dir_recursive",
               "SELECT d.*, ds.* FROM directories d "
               "JOIN directory_stats ds ON d.id = ds.directory_id "
               "WHERE d.vault_id = $1 AND d.path LIKE $2");

    conn_->prepare("get_directory_id_by_path", "SELECT id FROM directories WHERE vault_id = $1 AND path = $2");
}

void DBConnection::initPreparedRoles() const {
    conn_->prepare("insert_role",
                   "INSERT INTO role (name, description, type) "
                   "VALUES ($1, $2, $3)");

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
}

void DBConnection::initPreparedPermissions() const {
    conn_->prepare("insert_permission", "INSERT INTO permissions (role_id, permissions) VALUES ($1, $2)");

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


} // namespace vh::database