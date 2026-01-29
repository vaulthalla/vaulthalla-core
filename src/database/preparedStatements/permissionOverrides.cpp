#include "database/DBConnection.hpp"

using namespace vh::database;

void DBConnection::initPreparedPermOverrides() const {
    conn_->prepare("get_permission_override_by_vault_subject_and_bitpos",
                   "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.vault_id = $1 AND vra.subject_type = $2 AND vra.subject_id = $3 AND p.bit_position = $4");

    conn_->prepare("get_subject_assigned_vault_role_by_vault",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.vault_id = $1 AND vra.subject_type = $2 AND vra.subject_id = $3");

    conn_->prepare("list_vault_permission_overrides",
                   "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.vault_id = $1");

    conn_->prepare("list_subject_permission_overrides",
                   "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.subject_type = $1 AND vra.subject_id = $2");

    conn_->prepare("list_user_and_group_permission_overrides",
                   "("
                   "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "WHERE vra.subject_type = 'user' AND vra.subject_id = $1"
                   ") "
                   "UNION ALL "
                   "("
                   "SELECT p.*, vpo.enabled, vpo.pattern, vpo.assignment_id, vpo.effect, vra.role_id "
                   "FROM permission p "
                   "JOIN vault_permission_overrides vpo ON p.id = vpo.permission_id "
                   "JOIN vault_role_assignments vra ON vpo.assignment_id = vra.id "
                   "JOIN group_members gm ON vra.subject_type = 'group' AND vra.subject_id = gm.group_id "
                   "WHERE gm.user_id = $1"
                   ")"
        );

    conn_->prepare("insert_vault_permission_override",
                   "INSERT INTO vault_permission_overrides (assignment_id, permission_id, pattern, enabled, effect) "
                   "VALUES ($1, $2, $3, $4, $5) RETURNING id");

    conn_->prepare("update_vault_permission_override",
                   "UPDATE vault_permission_overrides SET pattern = $2, enabled = $3, effect = $4 "
                   "WHERE id = $1");

    conn_->prepare("delete_vault_permission_override", "DELETE FROM vault_permission_overrides WHERE id = $1");
}
