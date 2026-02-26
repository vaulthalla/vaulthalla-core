#include "db/DBConnection.hpp"

void vh::db::DBConnection::initPreparedVaultRoles() const {
    conn_->prepare("remove_vault_role_assignment", "DELETE FROM vault_role_assignments WHERE id = $1");

    conn_->prepare("get_vault_assigned_role",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions, r.created_at, vra.vault_id "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.id = $1");

    conn_->prepare("get_vault_assigned_roles",
                   "SELECT r.name, r.description, r.type, "
                   "vra.role_id, vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.assigned_at, "
                   "p.permissions::int as permissions, r.created_at, vra.vault_id "
                   "FROM role r "
                   "JOIN permissions p ON r.id = p.role_id "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "WHERE vra.vault_id = $1");

    conn_->prepare("get_subject_assigned_vault_roles",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions, r.created_at, vra.vault_id "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.subject_type = $1 AND vra.subject_id = $2");

    conn_->prepare("get_subject_assigned_vault_role",
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "r.name, r.description, r.type, p.permissions::int AS permissions, r.created_at, vra.vault_id "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.subject_type = $1 AND vra.subject_id = $2 AND vra.role_id = $3");

    conn_->prepare("get_user_and_group_assigned_vault_roles",
                   "("
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "       r.name, r.description, r.type, p.permissions::int AS permissions, r.created_at, vra.vault_id "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "WHERE vra.subject_type = 'user' AND vra.subject_id = $1"
                   ") "
                   "UNION ALL "
                   "("
                   "SELECT vra.id as assignment_id, vra.subject_type, vra.subject_id, vra.role_id, vra.assigned_at, "
                   "       r.name, r.description, r.type, p.permissions::int AS permissions, r.created_at, vra.vault_id "
                   "FROM role r "
                   "JOIN vault_role_assignments vra ON r.id = vra.role_id "
                   "JOIN permissions p ON r.id = p.role_id "
                   "JOIN group_members gm ON vra.subject_type = 'group' AND vra.subject_id = gm.group_id "
                   "WHERE gm.user_id = $1"
                   ")"
        );

    conn_->prepare("assign_vault_role",
                   "INSERT INTO vault_role_assignments (subject_type, subject_id, vault_id, role_id, assigned_at) "
                   "VALUES ($1, $2, $3, $4, NOW()) RETURNING id");

    conn_->prepare("upsert_assigned_vault_role",
                   "INSERT INTO vault_role_assignments (subject_type, vault_id, subject_id, role_id, assigned_at) "
                   "VALUES ($1, $2, $3, $4, NOW()) ON CONFLICT DO NOTHING");
}
