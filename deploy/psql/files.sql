CREATE TABLE files
(
    id                         SERIAL PRIMARY KEY,
    storage_volume_id          INTEGER REFERENCES volume (id) ON DELETE CASCADE,
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

CREATE TABLE file_locks
(
    file_id    INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    locked_by  INTEGER REFERENCES users (id),
    locked_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);

CREATE TABLE file_icons
(
    file_id        INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    icon_path      TEXT,
    thumbnail_path TEXT,
    preview_path   TEXT,
    generated_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

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

CREATE TABLE file_activity
(
    id        SERIAL PRIMARY KEY,
    file_id   INTEGER REFERENCES files (id) ON DELETE CASCADE,
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
    file_id     INTEGER REFERENCES files (id) ON DELETE CASCADE,
    tag_id      INTEGER REFERENCES file_tags (id) ON DELETE CASCADE,
    assigned_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (file_id, tag_id)
);

CREATE TABLE file_search_index
(
    file_id       INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,
    search_vector tsvector
);

CREATE INDEX idx_files_full_path ON files (full_path);
CREATE INDEX idx_file_xattrs_file_id ON file_xattrs (file_id);
CREATE INDEX idx_file_xattrs_key ON file_xattrs (key);
CREATE INDEX file_search_vector_idx ON file_search_index USING GIN (search_vector);
