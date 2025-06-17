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

CREATE TYPE role_name AS ENUM ('Admin', 'User', 'Guest', 'Moderator', 'SuperAdmin');

CREATE TABLE roles
(
    id          SERIAL PRIMARY KEY,
    name        role_name UNIQUE NOT NULL,
    description TEXT
);

CREATE TABLE user_roles
(
    user_id INTEGER REFERENCES users (id) ON DELETE CASCADE,
    role_id INTEGER REFERENCES roles (id) ON DELETE CASCADE,
    PRIMARY KEY (user_id, role_id)
);

CREATE TABLE refresh_tokens (
                                  jti UUID PRIMARY KEY,
                                  user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                                  token_hash TEXT NOT NULL,
                                  created_at TIMESTAMP DEFAULT NOW(),
                                  expires_at TIMESTAMP DEFAULT NOW() + INTERVAL '7 days',
                                  last_used TIMESTAMP DEFAULT NOW(),
                                  ip_address TEXT,
                                  user_agent TEXT,
                                  revoked BOOLEAN DEFAULT FALSE
);

-- API KEYS

CREATE TABLE api_keys
(
    id         SERIAL PRIMARY KEY,
    user_id    INTEGER REFERENCES users (id) ON DELETE CASCADE,
    type       VARCHAR(50) NOT NULL, -- 'S3', etc.
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
    type       VARCHAR(50) NOT NULL,
    name       VARCHAR(150) UNIQUE NOT NULL,
    is_active  BOOLEAN   DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
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
    created_at  TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
);


CREATE TABLE user_storage_volumes
(
    user_id           INTEGER REFERENCES users (id) ON DELETE CASCADE,
    storage_volume_id INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    PRIMARY KEY (user_id, storage_volume_id)
);

-- PERMISSIONS AND ROLES

CREATE TYPE permission_name AS ENUM (
    'ManageUsers',
    'ManageRoles',
    'ManageStorage',
    'ManageFiles',
    'ViewAuditLog',
    'UploadFile',
    'DownloadFile',
    'DeleteFile',
    'ShareFile',
    'LockFile'
    );

CREATE TABLE permissions
(
    id          SERIAL PRIMARY KEY,
    name        permission_name UNIQUE NOT NULL,
    description TEXT
);

CREATE TABLE role_permissions
(
    role_id           INTEGER REFERENCES roles (id) ON DELETE CASCADE,
    permission_id     INTEGER REFERENCES permissions (id) ON DELETE CASCADE,
    storage_volume_id INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE DEFAULT NULL,
    PRIMARY KEY (role_id, permission_id, storage_volume_id)
);

-- FILES AND METADATA

CREATE TABLE files
(
    id                         SERIAL PRIMARY KEY,
    storage_volume_id          INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    parent_id                  INTEGER REFERENCES files (id) ON DELETE CASCADE,
    name                       VARCHAR(500) NOT NULL,
    is_directory               BOOLEAN   DEFAULT FALSE,
    owner_id                   INTEGER REFERENCES users (id),
    created_by                 INTEGER REFERENCES users (id),
    created_at                 TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at                 TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    current_version_size_bytes BIGINT    DEFAULT 0,
    is_trashed                 BOOLEAN   DEFAULT FALSE,
    trashed_at                 TIMESTAMP,
    trashed_by                 INTEGER REFERENCES users (id),
    full_path                  TEXT,
    UNIQUE (storage_volume_id, parent_id, name)
);

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
    id      SERIAL PRIMARY KEY,
    file_id INTEGER REFERENCES files (id) ON DELETE CASCADE,
    key     VARCHAR(255)       NOT NULL UNIQUE,
    type    file_metadata_type NOT NULL,
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

CREATE TABLE file_shares
(
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER REFERENCES files (id) ON DELETE CASCADE,
    shared_by  INTEGER REFERENCES users (id),
    expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE file_shares_users
(
    id                  SERIAL PRIMARY KEY,
    file_share_id       INTEGER REFERENCES file_shares (id) ON DELETE CASCADE,
    shared_with_user_id INTEGER REFERENCES users (id),
    permissions         INTEGER NOT NULL DEFAULT 0, -- bitmask
    UNIQUE (file_share_id, shared_with_user_id)
);

CREATE TABLE public_file_shares
(
    id          SERIAL PRIMARY KEY,
    file_id     INTEGER REFERENCES files (id) ON DELETE CASCADE,
    shared_by   INTEGER REFERENCES users (id),
    share_token VARCHAR(100) UNIQUE NOT NULL,
    permissions INTEGER             NOT NULL DEFAULT 0, -- bitmask
    expires_at  TIMESTAMP,
    created_at  TIMESTAMP                    DEFAULT CURRENT_TIMESTAMP
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
    updated_at        TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- GROUPS (TEAM SHARING)

CREATE TABLE groups
(
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(150) UNIQUE NOT NULL,
    description TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE group_memberships
(
    group_id  INTEGER REFERENCES groups (id) ON DELETE CASCADE,
    user_id   INTEGER REFERENCES users (id) ON DELETE CASCADE,
    joined_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (group_id, user_id)
);

CREATE TABLE group_storage_volumes
(
    group_id          INTEGER REFERENCES groups (id) ON DELETE CASCADE,
    storage_volume_id INTEGER REFERENCES storage_volumes (id) ON DELETE CASCADE,
    assigned_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (group_id, storage_volume_id)
);

-- FILE TAGGING

CREATE TABLE file_tags
(
    id   SERIAL PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL
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
