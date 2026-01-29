#include "database/DBConnection.hpp"

using namespace vh::database;

void DBConnection::initPreparedPermissions() const {
    conn_->prepare("insert_raw_permission",
                   "INSERT INTO permission (bit_position, name, description, category) "
                   "VALUES ($1, $2, $3, $4)");

    conn_->prepare("insert_role_permission",
                   "INSERT INTO permissions (role_id, permissions) VALUES ($1, $2::bit(16))");

    conn_->prepare("update_permission", "UPDATE permissions SET permissions = $2 WHERE role_id = $1");

    conn_->prepare("delete_permission", "DELETE FROM permissions WHERE role_id = $1");
}
