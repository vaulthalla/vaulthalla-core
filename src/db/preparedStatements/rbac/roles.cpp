#include "db/DBConnection.hpp"

void vh::db::DBConnection::initPreparedRoles() const {
    conn_->prepare("insert_role",
                   "INSERT INTO role (name, description, type) "
                   "VALUES ($1, $2, $3) RETURNING id");

    conn_->prepare("update_role",
                   "UPDATE role SET name = $2, description = $3, type = $4 "
                   "WHERE id = $1");

    conn_->prepare("update_role_permissions", "UPDATE permissions SET permissions = $2 WHERE role_id = $1");

    conn_->prepare("delete_role", "DELETE FROM role WHERE id = $1");

    conn_->prepare("get_permissions_type", "SELECT type FROM role WHERE id = $1");

    conn_->prepare("role_exists", "SELECT EXISTS(SELECT 1 FROM role WHERE name = $1) AS exists");

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

    conn_->prepare("assign_permission_to_role",
                   "INSERT INTO permissions (role_id, permissions) VALUES ($1, $2::bit(16))");
}
