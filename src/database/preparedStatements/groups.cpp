#include "database/DBConnection.hpp"

using namespace vh::database;

void DBConnection::initPreparedGroups() const {
    conn_->prepare("insert_group",
                   "INSERT INTO groups (name, description, linux_gid) VALUES ($1, $2, $3) RETURNING id");

    conn_->prepare("update_group",
                   "UPDATE groups SET name = $2, description = $3, linux_gid = $4, updated_at = NOW() "
                   "WHERE id = $1");

    conn_->prepare("delete_group", "DELETE FROM groups WHERE id = $1");

    conn_->prepare("get_group", "SELECT * FROM groups WHERE id = $1");

    conn_->prepare("get_group_by_name", "SELECT * FROM groups WHERE name = $1");

    conn_->prepare("add_member_to_group",
                   "INSERT INTO group_members (group_id, user_id, joined_at) "
                   "VALUES ($1, $2, NOW()) "
                   "ON CONFLICT (group_id, user_id) DO NOTHING");

    conn_->prepare("remove_member_from_group", "DELETE FROM group_members WHERE group_id = $1 AND user_id = $2");

    conn_->prepare("list_group_members",
                   "SELECT u.*, gm.joined_at "
                   "FROM users u "
                   "JOIN group_members gm ON u.id = gm.user_id "
                   "WHERE gm.group_id = $1");

    conn_->prepare("group_exists", "SELECT EXISTS(SELECT 1 FROM groups WHERE name = $1) AS exists");
}
