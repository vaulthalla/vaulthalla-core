#include "db/DBConnection.hpp"

void vh::db::DBConnection::initPreparedAdminPermissions() const {
    conn_->prepare(
        "admin_permissions_upsert",
        R"SQL(
            INSERT INTO admin_permissions (
                role_id,
                identity_permissions,
                audit_permissions,
                settings_permissions
            )
            VALUES (
                $1,
                $2::bit(32),
                $3::bit(8),
                $4::bit(16)
            )
            ON CONFLICT (role_id) DO UPDATE SET
                identity_permissions = EXCLUDED.identity_permissions,
                audit_permissions    = EXCLUDED.audit_permissions,
                settings_permissions = EXCLUDED.settings_permissions
        )SQL"
    );

    conn_->prepare(
        "admin_permissions_get",
        R"SQL(
            SELECT
                role_id,
                identity_permissions::text AS identity_permissions,
                audit_permissions::text    AS audit_permissions,
                settings_permissions::text AS settings_permissions
            FROM admin_permissions
            WHERE role_id = $1
        )SQL"
    );

    conn_->prepare(
        "admin_permissions_delete",
        R"SQL(
            DELETE FROM admin_permissions
            WHERE role_id = $1
        )SQL"
    );

    conn_->prepare(
        "admin_permissions_exists",
        R"SQL(
            SELECT EXISTS(
                SELECT 1
                FROM admin_permissions
                WHERE role_id = $1
            )
        )SQL"
    );

    conn_->prepare(
        "admin_permissions_list",
        R"SQL(
            SELECT
                role_id,
                identity_permissions::text AS identity_permissions,
                audit_permissions::text    AS audit_permissions,
                settings_permissions::text AS settings_permissions
            FROM admin_permissions
            ORDER BY role_id
        )SQL"
    );
}
