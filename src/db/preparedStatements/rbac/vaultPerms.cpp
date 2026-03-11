#include "db/DBConnection.hpp"

void vh::db::DBConnection::initPreparedVaultPermissions() const {
    conn_->prepare(
        "vault_permissions_upsert",
        R"SQL(
            INSERT INTO vault_permissions (
                role_id,
                global_name,
                files_permissions,
                directories_permissions,
                sync_permissions,
                roles_permissions,
                keys_permissions
            )
            VALUES (
                $1,
                $2,
                $3::bit(64),
                $4::bit(64),
                $5::bit(32),
                $6::bit(16),
                $7::bit(16)
            )
            ON CONFLICT (role_id, global_name) DO UPDATE SET
                files_permissions       = EXCLUDED.files_permissions,
                directories_permissions = EXCLUDED.directories_permissions,
                sync_permissions        = EXCLUDED.sync_permissions,
                roles_permissions       = EXCLUDED.roles_permissions,
                keys_permissions        = EXCLUDED.keys_permissions
        )SQL"
    );

    conn_->prepare(
        "vault_permissions_get",
        R"SQL(
            SELECT
                role_id,
                global_name,
                files_permissions::text       AS files_permissions,
                directories_permissions::text AS directories_permissions,
                sync_permissions::text        AS sync_permissions,
                roles_permissions::text       AS roles_permissions,
                keys_permissions::text        AS keys_permissions
            FROM vault_permissions
            WHERE role_id = $1
              AND global_name = $2
        )SQL"
    );

    conn_->prepare(
        "vault_permissions_delete",
        R"SQL(
            DELETE FROM vault_permissions
            WHERE role_id = $1
              AND global_name = $2
        )SQL"
    );

    conn_->prepare(
        "vault_permissions_delete_all_for_role",
        R"SQL(
            DELETE FROM vault_permissions
            WHERE role_id = $1
        )SQL"
    );

    conn_->prepare(
        "vault_permissions_exists",
        R"SQL(
            SELECT EXISTS(
                SELECT 1
                FROM vault_permissions
                WHERE role_id = $1
                  AND global_name = $2
            )
        )SQL"
    );

    conn_->prepare(
        "vault_permissions_list_by_role",
        R"SQL(
            SELECT
                role_id,
                global_name,
                files_permissions::text       AS files_permissions,
                directories_permissions::text AS directories_permissions,
                sync_permissions::text        AS sync_permissions,
                roles_permissions::text       AS roles_permissions,
                keys_permissions::text        AS keys_permissions
            FROM vault_permissions
            WHERE role_id = $1
            ORDER BY global_name
        )SQL"
    );

    conn_->prepare(
        "vault_permissions_list_all",
        R"SQL(
            SELECT
                role_id,
                global_name,
                files_permissions::text       AS files_permissions,
                directories_permissions::text AS directories_permissions,
                sync_permissions::text        AS sync_permissions,
                roles_permissions::text       AS roles_permissions,
                keys_permissions::text        AS keys_permissions
            FROM vault_permissions
            ORDER BY role_id, global_name
        )SQL"
    );
}
