#include "database/DBConnection.hpp"

using namespace vh::database;

void DBConnection::initPreparedUserRoles() const {
    conn_->prepare("get_user_assigned_role",
                   "SELECT ura.id as assignment_id, ura.user_id, ura.role_id, ura.assigned_at, "
                   "r.name, r.description, r.type, r.created_at, p.permissions::int AS permissions "
                   "FROM user_role_assignments ura "
                   "JOIN role r ON ura.role_id = r.id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE ura.user_id = $1");

    conn_->prepare("get_user_role_id", "SELECT role_id FROM user_role_assignments WHERE user_id = $1");

    conn_->prepare("assign_user_role",
                   "INSERT INTO user_role_assignments (user_id, role_id) "
                   "VALUES ($1, $2) ON CONFLICT (user_id, role_id) DO NOTHING");

    conn_->prepare("update_user_role", "UPDATE user_role_assignments SET role_id = $2 WHERE user_id = $1");
}
