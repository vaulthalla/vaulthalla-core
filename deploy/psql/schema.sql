-- =========================
-- VAULTHALLA DATABASE SCHEMA
-- FINAL FORGED EDITION
-- =========================
-- Nextcloud replacement, forged by the gods
-- =========================

-- USERS AND ROLES

CREATE TABLE users
(
    id            SERIAL PRIMARY KEY,
    name          VARCHAR(50)         NOT NULL,
    email         VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255)        NOT NULL,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login    TIMESTAMP,
    is_active     BOOLEAN   DEFAULT TRUE
);

CREATE TABLE refresh_tokens
(
    jti        UUID PRIMARY KEY,
    user_id    INTEGER NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    token_hash TEXT    NOT NULL,
    created_at TIMESTAMP DEFAULT NOW(),
    expires_at TIMESTAMP DEFAULT NOW() + INTERVAL '7 days',
    last_used  TIMESTAMP DEFAULT NOW(),
    ip_address TEXT,
    user_agent TEXT,
    revoked    BOOLEAN   DEFAULT FALSE
);

-- PERMISSIONS AND ROLES

CREATE TABLE roles
(
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(50) UNIQUE NOT NULL,
    description TEXT,
    permissions BIT(16), -- Bitmask for role permissions
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE permissions
(
    id           SERIAL PRIMARY KEY,
    name         VARCHAR(50) UNIQUE NOT NULL,
    description  TEXT,
    bit_position INTEGER UNIQUE     NOT NULL CHECK (bit_position >= 0 AND bit_position < 16),
    created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE user_roles
(
    id          SERIAL PRIMARY KEY,
    user_id     INTEGER     NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    role_id     INTEGER     NOT NULL REFERENCES roles (id) ON DELETE CASCADE,
    scope       VARCHAR(10) NOT NULL CHECK (scope IN ('global', 'group', 'vault', 'volume')),
    scope_id   INTEGER,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- This ensures a user can have only ONE role where scope = 'global'
CREATE UNIQUE INDEX uniq_user_global_role
    ON user_roles (user_id) WHERE scope = 'global';

-- This ensures a user can have only ONE role per group, vault, or volume
CREATE FUNCTION enforce_one_global_role() RETURNS TRIGGER AS $$
BEGIN
    IF
TG_OP = 'DELETE' OR (TG_OP = 'UPDATE' AND OLD.scope = 'global') THEN
        -- Is this the user's only global role?
        IF NOT EXISTS (
            SELECT 1 FROM user_roles
            WHERE user_id = OLD.user_id
            AND scope = 'global'
            AND id != OLD.id
        ) THEN
            RAISE EXCEPTION 'User must always have exactly one global role.';
END IF;
END IF;

RETURN NEW;
END;
$$
LANGUAGE plpgsql;

CREATE TRIGGER trg_enforce_one_global_role
    BEFORE DELETE OR
UPDATE ON user_roles
    FOR EACH ROW EXECUTE FUNCTION enforce_one_global_role();

-- This function validates the scoped_id based on the scope
CREATE FUNCTION validate_user_role_scope() RETURNS trigger AS $$
BEGIN
    IF
NEW.scope = 'group' THEN
        IF NOT EXISTS (SELECT 1 FROM groups WHERE id = NEW.scoped_id) THEN
            RAISE EXCEPTION 'Invalid group ID';
END IF;
    ELSIF
NEW.scope = 'vault' THEN
        IF NOT EXISTS (SELECT 1 FROM vaults WHERE id = NEW.scoped_id) THEN
            RAISE EXCEPTION 'Invalid vault ID';
END IF;
    ELSIF
NEW.scope = 'volume' THEN
        IF NOT EXISTS (SELECT 1 FROM storage_volumes WHERE id = NEW.scoped_id) THEN
            RAISE EXCEPTION 'Invalid volume ID';
END IF;
    ELSIF
NEW.scope = 'global' THEN
        IF NEW.scoped_id IS NOT NULL THEN
            RAISE EXCEPTION 'Global scope must not have scoped_id';
END IF;
END IF;

RETURN NEW;
END;
$$
LANGUAGE plpgsql;

-- API KEYS

CREATE TABLE api_keys
(
    id         SERIAL PRIMARY KEY,
    user_id    INTEGER REFERENCES users (id) ON DELETE CASCADE,
    type       VARCHAR(50)  NOT NULL, -- 'S3', etc.
    name       VARCHAR(100) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE s3_api_keys
(
    api_key_id        INTEGER PRIMARY KEY REFERENCES api_keys (id) ON DELETE CASCADE,
    provider          VARCHAR(50) NOT NULL, -- 'AWS', 'Cloudflare R2', etc.
    access_key        TEXT        NOT NULL,
    secret_access_key TEXT        NOT NULL,
    region            VARCHAR(20) NOT NULL,
    endpoint          TEXT DEFAULT NULL
);

-- STORAGE BACKENDS AND VOLUMES

CREATE TABLE vaults
(
    id         SERIAL PRIMARY KEY,
    type       VARCHAR(50)         NOT NULL,
    name       VARCHAR(150) UNIQUE NOT NULL,
    is_active  BOOLEAN   DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Local Disk config
CREATE TABLE local_disk_vaults
(
    vault_id    INTEGER PRIMARY KEY REFERENCES vaults (id) ON DELETE CASCADE,
    mount_point TEXT NOT NULL
);

-- S3 config
CREATE TABLE s3_vaults
(
    vault_id   INTEGER REFERENCES vaults (id) ON DELETE CASCADE,
    api_key_id INTEGER REFERENCES s3_api_keys (api_key_id) ON DELETE CASCADE,
    bucket     TEXT NOT NULL
);

CREATE TABLE storage_volumes
(
    id          SERIAL PRIMARY KEY,
    vault_id    INTEGER      NOT NULL,
    name        VARCHAR(150) NOT NULL,
    path_prefix VARCHAR(255) DEFAULT '/',
    quota_bytes BIGINT       DEFAULT NULL,
    created_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);


CREATE TABLE user_storage_volumes
(
    user_id           INTEGER REFERENCES users (id) ON DELETE CASCADE,
    storage_volume_id INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    assigned_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, storage_volume_id)
);

-- GROUPS (TEAM SHARING)

CREATE TABLE groups
(
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(50) UNIQUE NOT NULL,
    description TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE group_members
(
    gid       INTEGER REFERENCES groups (id) ON DELETE CASCADE,
    uid       INTEGER REFERENCES users (id) ON DELETE CASCADE,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (gid, uid)
);

CREATE TABLE group_storage_volumes
(
    gid               INTEGER REFERENCES groups (id) ON DELETE CASCADE,
    storage_volume_id INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    assigned_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (gid, storage_volume_id)
);

-- FILES AND METADATA

CREATE TABLE files
(
    id                         SERIAL PRIMARY KEY,
    storage_volume_id          INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    parent_id                  INTEGER REFERENCES files (id) ON DELETE CASCADE,
    name                       VARCHAR(500) NOT NULL,
    is_directory               BOOLEAN   DEFAULT FALSE,
    mode                       INTEGER   DEFAULT 33188,
    uid                        INTEGER REFERENCES users (id),
    gid                        INTEGER REFERENCES groups (id),
    created_by                 INTEGER REFERENCES users (id),
    created_at                 TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at                 TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_modified_by           INTEGER REFERENCES users (id),
    current_version_size_bytes BIGINT    DEFAULT 0,
    is_trashed                 BOOLEAN   DEFAULT FALSE,
    trashed_at                 TIMESTAMP,
    trashed_by                 INTEGER REFERENCES users (id),
    full_path                  TEXT,
    UNIQUE (storage_volume_id, parent_id, name)
);

CREATE INDEX idx_files_full_path ON files (full_path);

CREATE TABLE file_xattrs
(
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER      NOT NULL REFERENCES files (id) ON DELETE CASCADE,
    namespace  VARCHAR(64)  NOT NULL DEFAULT 'user',
    key        VARCHAR(255) NOT NULL,
    value      BYTEA        NOT NULL, -- Store as raw bytes (can be stringified at API layer)
    created_at TIMESTAMP             DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP             DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (file_id, namespace, key)
);

CREATE INDEX idx_file_xattrs_file_id ON file_xattrs (file_id);
CREATE INDEX idx_file_xattrs_key ON file_xattrs (key);

CREATE TABLE file_versions
(
    id             SERIAL PRIMARY KEY,
    file_id        INTEGER REFERENCES files (id) ON DELETE CASCADE,
    version_number INTEGER      NOT NULL,
    content_hash   VARCHAR(128) NOT NULL,
    size_bytes     BIGINT       NOT NULL,
    mime_type      VARCHAR(255),
    storage_path   TEXT         NOT NULL,
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by     INTEGER REFERENCES users (id),
    UNIQUE (file_id, version_number)
);

CREATE TYPE file_metadata_type AS ENUM ('string', 'integer', 'boolean', 'timestamp', 'float');

CREATE TABLE file_metadata
(
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER REFERENCES files (id) ON DELETE CASCADE,
    key        VARCHAR(255)       NOT NULL,
    type       file_metadata_type NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (file_id, key)
);

CREATE TABLE file_metadata_string
(
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            TEXT NOT NULL
);

CREATE TABLE file_metadata_integer
(
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            BIGINT NOT NULL
);

CREATE TABLE file_metadata_boolean
(
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            BOOLEAN NOT NULL
);

CREATE TABLE file_metadata_timestamp
(
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            TIMESTAMP NOT NULL
);

CREATE TABLE file_metadata_float
(
    file_metadata_id INTEGER PRIMARY KEY REFERENCES file_metadata (id) ON DELETE CASCADE,
    value            DOUBLE PRECISION NOT NULL
);

-- FILE ACL

CREATE TABLE file_acl
(
    id           SERIAL PRIMARY KEY,           -- <-- NEW surrogate PK
    file_id      INTEGER REFERENCES files (id) ON DELETE CASCADE,
    subject_type TEXT CHECK (subject_type IN ('user', 'group', 'role', 'public')),
    subject_id   INTEGER,                      -- can be NULL if public (enforced via check later)
    permissions  BIT(16) NOT NULL,             -- Match bitset layout
    inherited    BOOLEAN DEFAULT FALSE,
    UNIQUE (file_id, subject_type, subject_id) -- <-- still enforce uniqueness
);

CREATE TABLE file_acl_index
(
    file_id           INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    has_override_acl  BOOLEAN   DEFAULT FALSE,
    subtree_acl_count INTEGER   DEFAULT 0,
    inherited_from_id INTEGER,
    full_path         TEXT,
    last_updated      TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_acl_index_path ON file_acl_index (full_path);

-- FILE SHARING

CREATE TYPE share_type AS ENUM ('user', 'public');

CREATE TABLE file_shares
(
    id          SERIAL PRIMARY KEY,
    file_acl_id INTEGER NOT NULL REFERENCES file_acl (id) ON DELETE CASCADE,
    shared_by   INTEGER NOT NULL REFERENCES users (id),
    share_token VARCHAR(100), -- nullable for user shares
    expires_at  TIMESTAMP,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    CHECK (
        (share_token IS NULL AND file_acl_id IS NOT NULL) OR
        (share_token IS NOT NULL AND file_acl_id IS NOT NULL)
        ),

    UNIQUE (share_token)
);

-- FILE LOCKS

CREATE TABLE file_locks
(
    file_id    INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    locked_by  INTEGER REFERENCES users (id),
    locked_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);

-- FILE REPRESENTATIONS

CREATE TABLE file_icons
(
    file_id        INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    icon_path      TEXT,
    thumbnail_path TEXT,
    preview_path   TEXT,
    generated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- FULL AUDIT LOGGING

CREATE TABLE audit_log
(
    id             SERIAL PRIMARY KEY,
    user_id        INTEGER REFERENCES users (id),
    action         VARCHAR(100) NOT NULL,
    target_file_id INTEGER REFERENCES files (id),
    timestamp      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ip_address     VARCHAR(50),
    user_agent     TEXT
);

-- FILE ACTIVITY LOG

CREATE TABLE file_activity
(
    id        SERIAL PRIMARY KEY,
    file_id   INTEGER REFERENCES files (id) ON DELETE CASCADE,
    user_id   INTEGER REFERENCES users (id),
    action    VARCHAR(100) NOT NULL, -- 'upload', 'download', 'rename', 'move', etc.
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- USER STORAGE USAGE

CREATE TABLE user_storage_usage
(
    user_id           INTEGER PRIMARY KEY REFERENCES users (id) ON DELETE CASCADE,
    storage_volume_id INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    total_bytes       BIGINT    DEFAULT 0,
    used_bytes        BIGINT    DEFAULT 0,
    created_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- FILE TAGGING

CREATE TABLE file_tags
(
    id         SERIAL PRIMARY KEY,
    name       VARCHAR(100) UNIQUE NOT NULL,
    color      VARCHAR(20) DEFAULT '#FFFFFF', -- Hex color code
    created_at TIMESTAMP   DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE file_tag_assignments
(
    file_id     INTEGER REFERENCES files (id) ON DELETE CASCADE,
    tag_id      INTEGER REFERENCES file_tags (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (file_id, tag_id)
);

-- SEARCH INDEX SUPPORT

CREATE TABLE file_search_index
(
    file_id       INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    search_vector tsvector
);

CREATE INDEX file_search_vector_idx ON file_search_index USING GIN (search_vector);

-- =========================
-- END OF MASTER SCHEMA
-- FINAL FORGED EDITION
-- =========================
