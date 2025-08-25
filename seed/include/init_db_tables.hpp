#pragma once

#include "database/Transactions.hpp"

namespace vh::database::seed {

inline void init_auth() {
    Transactions::exec("init_db_tables::init_auth", [&](pqxx::work& txn) {
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS users
(
    id               SERIAL             PRIMARY KEY,
    linux_uid        INTEGER            UNIQUE,
    name             VARCHAR(50)        UNIQUE NOT NULL,
    email            VARCHAR(255)       UNIQUE DEFAULT NULL,
    password_hash    VARCHAR(255)       NOT NULL,
    created_at       TIMESTAMP          DEFAULT CURRENT_TIMESTAMP,
    last_login       TIMESTAMP,
    last_modified_by INTEGER            REFERENCES users (id) ON DELETE SET NULL,
    updated_at       TIMESTAMP          DEFAULT CURRENT_TIMESTAMP,
    is_active        BOOLEAN            DEFAULT TRUE
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS refresh_tokens
(
    jti         UUID PRIMARY KEY,
    user_id     INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    token_hash  TEXT    NOT NULL,
    created_at  TIMESTAMP DEFAULT NOW(),
    expires_at  TIMESTAMP DEFAULT NOW() + INTERVAL '7 days',
    last_used   TIMESTAMP DEFAULT NOW(),
    ip_address  TEXT,
    user_agent  TEXT,
    revoked     BOOLEAN   DEFAULT FALSE
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS groups
(
    id           SERIAL PRIMARY KEY,
    linux_gid    INTEGER UNIQUE,
    name         VARCHAR(50) UNIQUE NOT NULL,
    description  TEXT,
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS group_members
(
    group_id   INTEGER REFERENCES groups (id) ON DELETE CASCADE,
    user_id    INTEGER REFERENCES users (id) ON DELETE CASCADE,
    joined_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (group_id, user_id)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS internal_secrets
(
    key    VARCHAR(50) PRIMARY KEY,
    value  BYTEA        NOT NULL,
    iv     BYTEA        NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
)
        )");
    });
}

inline void init_vaults() {
    Transactions::exec("init_db_tables::init_vaults", [&](pqxx::work& txn) {
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS api_keys
(
    id                          SERIAL PRIMARY KEY,
    user_id                     INTEGER REFERENCES users (id) ON DELETE CASCADE,
    name                        VARCHAR(100) NOT NULL,
    provider                    VARCHAR(50)  NOT NULL,
    access_key                  TEXT         NOT NULL,
    encrypted_secret_access_key BYTEA        NOT NULL,
    iv                          BYTEA        NOT NULL,
    region                      VARCHAR(20)  NOT NULL,
    endpoint                    TEXT      DEFAULT NULL,
    created_at                  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (user_id, name, access_key)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS vault
(
    id             SERIAL PRIMARY KEY,
    type           VARCHAR(12)         NOT NULL CHECK (type IN ('local', 's3')),
    name           VARCHAR(100) UNIQUE NOT NULL,
    description    TEXT                         DEFAULT NULL,
    quota          BIGINT              NOT NULL DEFAULT (0),
    owner_id       INTEGER             REFERENCES users (id) ON DELETE SET NULL,
    mount_point    CHAR(33)                NOT NULL,
    allow_fs_write BOOLEAN                      DEFAULT FALSE,
    is_active      BOOLEAN                      DEFAULT TRUE,
    created_at     TIMESTAMP                    DEFAULT CURRENT_TIMESTAMP,
    updated_at     TIMESTAMP                    DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (name, owner_id)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS vault_keys
(
    vault_id      SERIAL PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    encrypted_key BYTEA NOT NULL,
    iv            BYTEA NOT NULL,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    version       INTEGER NOT NULL DEFAULT 1,

    UNIQUE (vault_id)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS vault_keys_trashed
(
    id                     SERIAL PRIMARY KEY,
    vault_id               INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    version                INTEGER NOT NULL DEFAULT 1,
    encrypted_key          BYTEA NOT NULL,
    iv                     BYTEA NOT NULL,
    created_at             TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    trashed_at             TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    rotation_completed_at  TIMESTAMP DEFAULT NULL,

    UNIQUE (vault_id, version)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS s3
(
    vault_id     INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    api_key_id   INTEGER REFERENCES api_keys (id) ON DELETE CASCADE,
    bucket       TEXT NOT NULL,

    UNIQUE (api_key_id, bucket) -- Ensure unique bucket per API key
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS vault_usage
(
    vault_id    INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    total_bytes BIGINT    DEFAULT 0 NOT NULL,
    quota_bytes BIGINT    DEFAULT NULL,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS usage_log
(
    id          SERIAL PRIMARY KEY,
    vault_id    INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    measured_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_bytes BIGINT  NOT NULL
);
        )");
    });
}

inline void init_files() {
    Transactions::exec("init_db_tables::init_files", [&](pqxx::work& txn) {

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS fs_entry (
    id               SERIAL PRIMARY KEY,
    vault_id         INTEGER REFERENCES vault (id) ON DELETE CASCADE DEFAULT NULL,
    parent_id        INTEGER REFERENCES fs_entry (id) ON DELETE CASCADE DEFAULT NULL,
    name             VARCHAR(500) NOT NULL,
    base32_alias     CHAR(33) UNIQUE,
    inode            BIGINT UNIQUE,
    created_by       INTEGER REFERENCES users (id),
    created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_modified_by INTEGER REFERENCES users (id),
    path             TEXT         NOT NULL,
    mode             INTEGER   DEFAULT 0755,
    owner_uid        INTEGER,
    group_gid        INTEGER,
    is_hidden        BOOLEAN   DEFAULT FALSE,
    is_system        BOOLEAN   DEFAULT FALSE,

    UNIQUE (parent_id, name)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS directories (
    fs_entry_id        INTEGER PRIMARY KEY REFERENCES fs_entry (id) ON DELETE CASCADE,
    file_count         INTEGER DEFAULT 0,
    subdirectory_count INTEGER DEFAULT 0,
    size_bytes         BIGINT DEFAULT 0,
    last_modified      TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS files (
    fs_entry_id   INTEGER PRIMARY KEY REFERENCES fs_entry (id) ON DELETE CASCADE,
    size_bytes    BIGINT DEFAULT 0,
    mime_type     VARCHAR(255),
    content_hash  VARCHAR(128),
    encryption_iv TEXT,
    encrypted_with_key_version INTEGER DEFAULT 1
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS files_trashed (
    id           SERIAL PRIMARY KEY,
    vault_id     INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    backing_path TEXT NOT NULL,
    base32_alias CHAR(33) NOT NULL,
    trashed_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    trashed_by   INTEGER REFERENCES users (id),
    deleted_at   TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS cache_index (
    id            SERIAL PRIMARY KEY,
    vault_id      INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    file_id       INTEGER NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    path          TEXT NOT NULL,
    type          VARCHAR(12) NOT NULL CHECK (type IN ('thumbnail', 'file')),
    size          BIGINT NOT NULL,
    last_accessed TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (vault_id, path, type)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS operations (
    id               SERIAL PRIMARY KEY,
    operation        VARCHAR(12) NOT NULL CHECK (operation IN ('move', 'copy', 'rename')),
    target           VARCHAR(12) NOT NULL CHECK (target IN ('file', 'directory')),
    fs_entry_id      INTEGER REFERENCES fs_entry (id) ON DELETE CASCADE,
    source_path      TEXT NOT NULL CHECK (source_path LIKE '/%'),
    destination_path TEXT NOT NULL CHECK (destination_path LIKE '/%'),
    executed_by      INTEGER REFERENCES users (id),
    created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status           VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'in_progress', 'success', 'error', 'cancelled')),
    completed_at     TIMESTAMP DEFAULT NULL,
    error            TEXT
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_xattrs (
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    namespace  VARCHAR(64) NOT NULL DEFAULT 'user',
    key        VARCHAR(255) NOT NULL,
    value      BYTEA NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (file_id, namespace, key)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_versions (
    id             SERIAL PRIMARY KEY,
    file_id        INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    version_number INTEGER NOT NULL,
    content_hash   VARCHAR(128) NOT NULL,
    size_bytes     BIGINT NOT NULL,
    mime_type      VARCHAR(255),
    path           TEXT NOT NULL,
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by     INTEGER REFERENCES users (id),
    UNIQUE (file_id, version_number)
);
        )");

        txn.exec(R"(
DO $$ BEGIN
    CREATE TYPE file_metadata_type AS ENUM ('string', 'integer', 'boolean', 'timestamp', 'float');
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_metadata (
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    key        VARCHAR(255) NOT NULL,
    type       file_metadata_type NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (file_id, key)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS fs_mutation_log (
    id          SERIAL PRIMARY KEY,
    fs_entry_id INTEGER REFERENCES fs_entry (id) ON DELETE CASCADE,
    action      VARCHAR(32) NOT NULL,
    actor_uid   INTEGER,
    timestamp   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    note        TEXT
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_metadata_string (
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            TEXT NOT NULL
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_metadata_integer (
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            BIGINT NOT NULL
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_metadata_boolean (
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            BOOLEAN NOT NULL
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_metadata_timestamp (
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            TIMESTAMP NOT NULL
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_metadata_float (
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            DOUBLE PRECISION NOT NULL
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_locks (
    file_id    INTEGER PRIMARY KEY REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    locked_by  INTEGER REFERENCES users (id),
    locked_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS thumbnails (
    vault_id     INTEGER REFERENCES vault (id),
    path         TEXT NOT NULL,
    updated_at   TIMESTAMP NOT NULL,
    content_hash TEXT NOT NULL,
    width        INTEGER,
    height       INTEGER,
    cached_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (vault_id, path)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS audit_log (
    id             SERIAL PRIMARY KEY,
    user_id        INTEGER REFERENCES users (id),
    action         VARCHAR(100) NOT NULL,
    target_file_id INTEGER REFERENCES files (fs_entry_id),
    timestamp      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ip_address     VARCHAR(50),
    user_agent     TEXT
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_activity (
    id        SERIAL PRIMARY KEY,
    file_id   INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    user_id   INTEGER REFERENCES users (id),
    action    VARCHAR(100) NOT NULL,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_tags (
    id         SERIAL PRIMARY KEY,
    name       VARCHAR(100) UNIQUE NOT NULL,
    color      VARCHAR(20) DEFAULT '#FFFFFF',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_tag_assignments (
    file_id     INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    tag_id      INTEGER REFERENCES file_tags (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (file_id, tag_id)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS file_search_index (
    file_id       INTEGER PRIMARY KEY REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    search_vector tsvector
);
        )");

        // Indexes (grouped for logical clarity)
        txn.exec(R"(
CREATE INDEX IF NOT EXISTS idx_fs_entry_vault_path ON fs_entry (vault_id, path text_pattern_ops);
CREATE INDEX IF NOT EXISTS idx_fs_entry_parent ON fs_entry (parent_id);
CREATE INDEX IF NOT EXISTS idx_files_trashed_backing_path_not_deleted ON files_trashed (backing_path) WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_files_trashed_entry_not_deleted ON files_trashed (id) WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_files_trashed_deleted_at ON files_trashed (deleted_at);
CREATE INDEX IF NOT EXISTS idx_files_trashed_vault_trashed_at ON files_trashed (vault_id, trashed_at DESC) WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS idx_cache_index_size ON cache_index (size DESC);
CREATE INDEX IF NOT EXISTS idx_cache_index_vault_type_size ON cache_index (vault_id, type, size DESC);
CREATE INDEX IF NOT EXISTS file_search_vector_idx ON file_search_index USING GIN (search_vector);
CREATE INDEX IF NOT EXISTS idx_file_xattrs_file_id ON file_xattrs (file_id);
CREATE INDEX IF NOT EXISTS idx_file_xattrs_key ON file_xattrs (key);
CREATE INDEX IF NOT EXISTS idx_file_metadata_key ON file_metadata (key);
        )");
    });
}

inline void init_sync() {
    Transactions::exec("init_db_tables::init_sync", [&](pqxx::work& txn) {
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS backup_policy (
    id              SERIAL PRIMARY KEY,
    vault_id        INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    backup_interval BIGINT NOT NULL DEFAULT 86400,
    last_backup_at  TIMESTAMP DEFAULT NULL,
    last_success_at TIMESTAMP DEFAULT NULL,
    retention       INTERVAL DEFAULT NULL,
    retry_count     INTEGER NOT NULL DEFAULT 0,
    enabled         BOOLEAN NOT NULL DEFAULT FALSE,
    last_error      TEXT DEFAULT NULL,
    status          VARCHAR(12) DEFAULT 'idle' CHECK (status IN ('idle', 'syncing', 'error'))
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS sync (
    id              SERIAL PRIMARY KEY,
    vault_id        INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    interval        BIGINT NOT NULL DEFAULT 300,
    enabled         BOOLEAN NOT NULL DEFAULT TRUE,
    last_sync_at    TIMESTAMP DEFAULT NULL,
    last_success_at TIMESTAMP DEFAULT NULL,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (vault_id)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS fsync (
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    conflict_policy VARCHAR(12) DEFAULT 'keep_both' CHECK (conflict_policy IN ('overwrite', 'keep_both', 'ask'))
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS rsync (
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    strategy        VARCHAR(8) DEFAULT 'cache' CHECK (strategy IN ('cache', 'sync', 'mirror')),
    conflict_policy VARCHAR(12) DEFAULT 'keep_local' CHECK (conflict_policy IN ('keep_local', 'keep_remote', 'ask'))
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS sync_conflicts (
    id             SERIAL PRIMARY KEY,
    sync_id        INTEGER NOT NULL REFERENCES sync (id) ON DELETE CASCADE,
    file_id        INTEGER NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    conflict_type  VARCHAR(12) DEFAULT 'content' CHECK (conflict_type IN ('content', 'metadata', 'both')),
    resolved_at    TIMESTAMP DEFAULT NULL,
    resolution     VARCHAR(12) DEFAULT 'unresolved' CHECK (resolution IN ('unresolved', 'kept_local', 'kept_remote')),
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (sync_id, file_id)
);
        )");

        txn.exec(R"(
CREATE OR REPLACE FUNCTION update_timestamp()
    RETURNS TRIGGER AS $$
    BEGIN
        NEW.updated_at = CURRENT_TIMESTAMP;
        RETURN NEW;
    END;
    $$ LANGUAGE plpgsql;
        )");

        txn.exec(R"(
DO $$ BEGIN
    CREATE TRIGGER update_sync_timestamp
    BEFORE UPDATE ON sync
    FOR EACH ROW
    EXECUTE FUNCTION update_timestamp();
EXCEPTION
    WHEN duplicate_object THEN null;
END $$;
        )");
    });
}

inline void init_acl() {
    Transactions::exec("init_db_tables::init_acl", [&](pqxx::work& txn) {
        // Permissions and Roles
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS permission (
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) NOT NULL,
    description  TEXT,
    category     VARCHAR(12) NOT NULL CHECK (category IN ('user', 'vault')),
    bit_position INTEGER NOT NULL CHECK (bit_position >= 0 AND bit_position < 64),
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (name, category)
);
        )");

        txn.exec(R"(
CREATE TABLE IF NOT EXISTS role (
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(50) UNIQUE NOT NULL,
    type        VARCHAR(12) NOT NULL CHECK (type IN ('user', 'vault')),
    description TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
        )");

        // Permissions assigned to roles
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS permissions (
    id          SERIAL PRIMARY KEY,
    role_id     INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    permissions BIT(16) NOT NULL,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (role_id)
);
        )");

        // Assignments: user-level
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS user_role_assignments (
    id          SERIAL PRIMARY KEY,
    user_id     INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    role_id     INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (user_id, role_id)
);
        )");

        // Assignments: vault-level (user/group)
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS vault_role_assignments (
    id           SERIAL PRIMARY KEY,
    vault_id     INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    subject_type VARCHAR(10) NOT NULL CHECK (subject_type IN ('user', 'group')),
    subject_id   INTEGER NOT NULL,
    role_id      INTEGER NOT NULL REFERENCES role (id) ON DELETE CASCADE,
    assigned_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (vault_id, subject_type, subject_id)
);
        )");

        // Overrides
        txn.exec(R"(
CREATE TABLE IF NOT EXISTS vault_permission_overrides (
    id            SERIAL PRIMARY KEY,
    assignment_id INTEGER NOT NULL REFERENCES vault_role_assignments (id) ON DELETE CASCADE,
    permission_id INTEGER NOT NULL REFERENCES permission (id) ON DELETE CASCADE,
    scope         VARCHAR(10) NOT NULL CHECK (scope IN ('vault', 'file', 'directory')),
    enabled       BOOLEAN DEFAULT FALSE,
    regex         TEXT NOT NULL,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (assignment_id, permission_id, regex)
);
        )");

        // Indexes
        txn.exec(R"(
CREATE UNIQUE INDEX IF NOT EXISTS idx_permission_name_category
    ON permission (LOWER(name), category);
        )");

        txn.exec(R"(
CREATE INDEX IF NOT EXISTS idx_vault_role_assignments_vault_subject
    ON vault_role_assignments (vault_id, subject_type, subject_id);
        )");

        txn.exec(R"(
CREATE INDEX IF NOT EXISTS idx_vault_permission_overrides_perm
    ON vault_permission_overrides (permission_id);
        )");

        txn.exec(R"(
CREATE INDEX IF NOT EXISTS idx_vault_permission_overrides_assignment_scope
    ON vault_permission_overrides (assignment_id, scope);
        )");
    });
}

inline void init_tables_if_not_exists() {
    init_auth();
    init_vaults();
    init_files();
    init_sync();
    init_acl();
}

}
