CREATE TABLE fs_entry
(
    id               SERIAL PRIMARY KEY,
    vault_id         INTEGER REFERENCES vault (id) ON DELETE CASCADE DEFAULT NULL,
    parent_id        INTEGER REFERENCES fs_entry (id) ON DELETE CASCADE DEFAULT NULL,
    name             VARCHAR(500) NOT NULL,
    created_by       INTEGER REFERENCES users (id),
    created_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_modified_by INTEGER REFERENCES users (id),
    path             TEXT         NOT NULL,   -- Full path for easy access

    -- FUSE compatibility
    abs_path         TEXT NOT NULL,   -- Absolute path for FUSE operations
    uuid             UUID DEFAULT gen_random_uuid(), -- Unique identifier for FUSE operations
    inode            BIGINT UNIQUE,           -- bidirectional lookup and faster indexing
    mode             INTEGER   DEFAULT 0755,  -- Emulate POSIX permissions
    owner_uid        INTEGER,                 -- Linux UID for access checks
    group_gid        INTEGER,                 -- Linux GID for group access checks
    is_hidden        BOOLEAN   DEFAULT FALSE,
    is_system        BOOLEAN   DEFAULT FALSE, -- Mark internal-only entries (e.g. keys)

    UNIQUE (parent_id, name) -- Ensure no duplicate names in the same directory
);

CREATE TABLE directories
(
    fs_entry_id        INTEGER PRIMARY KEY REFERENCES fs_entry (id) ON DELETE CASCADE,
    file_count         INTEGER   DEFAULT 0,
    subdirectory_count INTEGER   DEFAULT 0, -- Count of immediate subdirectories
    size_bytes         BIGINT    DEFAULT 0, -- Total size of all files in this directory
    last_modified      TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE files
(
    fs_entry_id   INTEGER PRIMARY KEY REFERENCES fs_entry (id) ON DELETE CASCADE,
    size_bytes    BIGINT DEFAULT 0,
    mime_type     VARCHAR(255),
    content_hash  VARCHAR(128),
    encryption_iv TEXT
);

CREATE TABLE files_trashed
(
    fs_entry_id      INTEGER,   -- from fs_entry.id, no FK
    vault_id         INTEGER,   -- from fs_entry.vault_id, no FK
    parent_id        INTEGER,   -- from fs_entry.parent_id, no FK
    name             VARCHAR(500) NOT NULL,
    created_by       INTEGER REFERENCES users (id),
    created_at       TIMESTAMP,
    updated_at       TIMESTAMP,
    last_modified_by INTEGER REFERENCES users (id),
    path             TEXT         NOT NULL,

    size_bytes       BIGINT    DEFAULT 0,
    mime_type        VARCHAR(255),
    content_hash     VARCHAR(128),

    trashed_at       TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    trashed_by       INTEGER REFERENCES users (id),
    deleted_at       TIMESTAMP, -- NULL if not permanently deleted
    encryption_iv    TEXT
);

CREATE TABLE cache_index
(
    id            SERIAL PRIMARY KEY,
    vault_id      INTEGER     NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    file_id       INTEGER     NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    path          TEXT        NOT NULL, -- relative path inside cache
    type          VARCHAR(12) NOT NULL CHECK (type IN ('thumbnail', 'file')),
    size          BIGINT      NOT NULL,
    last_accessed TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (vault_id, path, type)       -- Ensure no duplicate entries for same vault, path, and type
);

CREATE TABLE operations
(
    id               SERIAL PRIMARY KEY,
    operation        VARCHAR(12) NOT NULL CHECK (operation IN ('move', 'copy', 'rename')),
    target           VARCHAR(12) NOT NULL CHECK (target IN ('file', 'directory')),
    fs_entry_id      INTEGER REFERENCES fs_entry (id) ON DELETE CASCADE, -- fs_entry.vault_id used for vault batching
    source_path      TEXT        NOT NULL CHECK (source_path LIKE '/%'),
    destination_path TEXT        NOT NULL CHECK (destination_path LIKE '/%'),
    executed_by      INTEGER REFERENCES users (id),
    created_at       TIMESTAMP            DEFAULT CURRENT_TIMESTAMP,
    status           VARCHAR(20) NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'in_progress', 'success', 'error', 'cancelled')),
    completed_at     TIMESTAMP            DEFAULT NULL,
    error            TEXT
);

CREATE TABLE file_xattrs
(
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER      NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    namespace  VARCHAR(64)  NOT NULL DEFAULT 'user',
    key        VARCHAR(255) NOT NULL,
    value      BYTEA        NOT NULL, -- Store as raw bytes (can be stringified at API layer)
    created_at TIMESTAMP             DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP             DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (file_id, namespace, key)
);

CREATE TABLE file_versions
(
    id             SERIAL PRIMARY KEY,
    file_id        INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    version_number INTEGER      NOT NULL,
    content_hash   VARCHAR(128) NOT NULL,
    size_bytes     BIGINT       NOT NULL,
    mime_type      VARCHAR(255),
    path           TEXT         NOT NULL,
    created_at     TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by     INTEGER REFERENCES users (id),
    UNIQUE (file_id, version_number)
);

CREATE TYPE file_metadata_type AS ENUM ('string', 'integer', 'boolean', 'timestamp', 'float');

CREATE TABLE file_metadata
(
    id         SERIAL PRIMARY KEY,
    file_id    INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    key        VARCHAR(255)       NOT NULL,
    type       file_metadata_type NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (file_id, key)
);

CREATE TABLE fs_mutation_log
(
    id          SERIAL PRIMARY KEY,
    fs_entry_id INTEGER REFERENCES fs_entry (id) ON DELETE CASCADE,
    action      VARCHAR(32) NOT NULL, -- create, modify, chmod, move, delete
    actor_uid   INTEGER,              -- calling Linux UID
    timestamp   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    note        TEXT
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

CREATE TABLE file_locks
(
    file_id    INTEGER PRIMARY KEY REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    locked_by  INTEGER REFERENCES users (id),
    locked_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);

CREATE TABLE thumbnails
(
    vault_id     INTEGER REFERENCES vault (id),
    path         TEXT      NOT NULL,
    updated_at   TIMESTAMP NOT NULL,
    content_hash TEXT      NOT NULL,
    width        INTEGER,
    height       INTEGER,
    cached_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (vault_id, path)
);


CREATE TABLE audit_log
(
    id             SERIAL PRIMARY KEY,
    user_id        INTEGER REFERENCES users (id),
    action         VARCHAR(100) NOT NULL,
    target_file_id INTEGER REFERENCES files (fs_entry_id),
    timestamp      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    ip_address     VARCHAR(50),
    user_agent     TEXT
);

CREATE TABLE file_activity
(
    id        SERIAL PRIMARY KEY,
    file_id   INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    user_id   INTEGER REFERENCES users (id),
    action    VARCHAR(100) NOT NULL, -- 'upload', 'download', 'rename', 'move', etc.
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE file_tags
(
    id         SERIAL PRIMARY KEY,
    name       VARCHAR(100) UNIQUE NOT NULL,
    color      VARCHAR(20) DEFAULT '#FFFFFF', -- Hex color code
    created_at TIMESTAMP   DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE file_tag_assignments
(
    file_id     INTEGER REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    tag_id      INTEGER REFERENCES file_tags (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (file_id, tag_id)
);

CREATE TABLE file_search_index
(
    file_id       INTEGER PRIMARY KEY REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    search_vector tsvector
);

-- ===============================
-- 🔧 FS Entry Optimization
-- ===============================

-- Fast lookup by vault and full path (e.g. for resolving UI paths)
CREATE INDEX idx_fs_entry_vault_path
    ON fs_entry (vault_id, path text_pattern_ops);

-- Fast lookup of direct children within a directory
CREATE INDEX idx_fs_entry_parent
    ON fs_entry (parent_id);

-- Optional: Deep LIKE (e.g. recursive trash listings)
-- CREATE EXTENSION IF NOT EXISTS pg_trgm;
-- CREATE INDEX idx_fs_entry_path_trgm ON fs_entry USING gin (path gin_trgm_ops);


-- ===============================
-- 🗑️ Trashed Files Optimization
-- ===============================

-- Primary access for listing trashed files and status checks
CREATE INDEX idx_files_trashed_vault_path_not_deleted
    ON files_trashed (vault_id, path) WHERE deleted_at IS NULL;

-- Lookup by entry ID for trash checks, soft delete
CREATE INDEX idx_files_trashed_entry_not_deleted
    ON files_trashed (fs_entry_id) WHERE deleted_at IS NULL;

-- Optional: fast purging / retention enforcement
CREATE INDEX idx_files_trashed_deleted_at
    ON files_trashed (deleted_at);

-- Optional: support sorted trash listings or dashboards
CREATE INDEX idx_files_trashed_vault_trashed_at
    ON files_trashed (vault_id, trashed_at DESC) WHERE deleted_at IS NULL;


-- ===============================
-- ⚡ Cache Indexes
-- ===============================

CREATE INDEX idx_cache_index_size
    ON cache_index (size DESC);

CREATE INDEX idx_cache_index_vault_type_size
    ON cache_index (vault_id, type, size DESC);


-- ===============================
-- 🧠 Full-text Search
-- ===============================

CREATE INDEX file_search_vector_idx
    ON file_search_index
    USING GIN (search_vector);


-- ===============================
-- 🧬 XAttr Lookups
-- ===============================

CREATE INDEX idx_file_xattrs_file_id
    ON file_xattrs (file_id);

CREATE INDEX idx_file_xattrs_key
    ON file_xattrs (key);


-- ===============================
-- 📁 Metadata Lookups
-- ===============================

CREATE INDEX idx_file_metadata_key
    ON file_metadata (key);
