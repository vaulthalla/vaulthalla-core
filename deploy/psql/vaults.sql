CREATE TABLE api_keys
(
    id         SERIAL PRIMARY KEY,
    user_id    INTEGER REFERENCES users (id) ON DELETE CASCADE,
    type       VARCHAR(50)         NOT NULL, -- 'S3', etc.
    name       VARCHAR(100) UNIQUE NOT NULL,
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

CREATE TABLE s3_buckets
(
    id         SERIAL PRIMARY KEY,
    api_key_id INTEGER REFERENCES s3_api_keys (api_key_id) ON DELETE CASCADE,
    name       TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    enabled    BOOLEAN DEFAULT TRUE,

    UNIQUE (api_key_id, name)
);

CREATE TABLE vault
(
    id          SERIAL PRIMARY KEY,
    type        VARCHAR(12)         NOT NULL CHECK (type IN ('local', 's3')),
    name        VARCHAR(100) UNIQUE NOT NULL,
    description TEXT      DEFAULT NULL,
    quota       BIGINT NOT NULL DEFAULT (0), -- 0 means no quota
    owner_id    INTEGER             REFERENCES users (id) ON DELETE SET NULL,
    is_active   BOOLEAN   DEFAULT TRUE,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE local
(
    vault_id    INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    mount_point TEXT NOT NULL
);

CREATE TABLE s3
(
    vault_id      INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    bucket_id     INTEGER REFERENCES s3_buckets (id) ON DELETE CASCADE
);

CREATE TABLE backup_policy
(
    id              SERIAL PRIMARY KEY,
    vault_id        INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    backup_interval BIGINT NOT NULL DEFAULT 86400, -- 1 day in seconds
    last_backup_at  TIMESTAMP        DEFAULT NULL,
    last_success_at TIMESTAMP        DEFAULT NULL,
    retention       INTERVAL DEFAULT NULL, -- how long to keep backups
    retry_count     INTEGER NOT NULL DEFAULT 0,
    enabled         BOOLEAN NOT NULL DEFAULT FALSE,
    last_error      TEXT DEFAULT NULL,
    status          VARCHAR(12) DEFAULT 'idle' CHECK (status IN ('idle', 'syncing', 'error'))
);

CREATE TABLE backup_targets
(
    id        SERIAL PRIMARY KEY,
    backup_id INTEGER REFERENCES backup_policy (id) ON DELETE CASCADE,
    type      VARCHAR(12) NOT NULL CHECK (type IN ('local', 's3')),
    path      TEXT DEFAULT NULL,
    bucket_id INTEGER REFERENCES s3_buckets (id) ON DELETE CASCADE,
    prefix    TEXT DEFAULT NULL
);

CREATE TABLE sync
(
    id              SERIAL PRIMARY KEY,
    interval        BIGINT NOT NULL DEFAULT 300, -- 5 minutes in seconds
    enabled         BOOLEAN NOT NULL DEFAULT TRUE,
    vault_id                 INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    strategy                 VARCHAR(8)      DEFAULT 'cache' CHECK (strategy IN ('cache', 'sync', 'mirror')),
    conflict_policy VARCHAR(12)      DEFAULT 'keep_local' CHECK (conflict_policy IN ('keep_local', 'keep_remote', 'ask')),
    last_sync_at    TIMESTAMP        DEFAULT NULL,
    last_success_at TIMESTAMP        DEFAULT NULL,
    created_at      TIMESTAMP        DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP        DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (vault_id)  -- Only one sync per vault
);

CREATE TABLE sync_conflicts
(
    id          SERIAL PRIMARY KEY,
    sync_id     INTEGER NOT NULL REFERENCES sync (id) ON DELETE CASCADE,
    file_id     INTEGER NOT NULL REFERENCES files (id) ON DELETE CASCADE,
    conflict_type VARCHAR(12) DEFAULT 'content' CHECK (conflict_type IN ('content', 'metadata', 'both')),
    resolved_at TIMESTAMP DEFAULT NULL,
    resolution   VARCHAR(12) DEFAULT 'unresolved' CHECK (resolution IN ('unresolved', 'kept_local', 'kept_remote')),
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (sync_id, file_id)  -- Ensure no duplicate conflicts for the same file in a sync
);

CREATE TABLE vault_usage
(
    vault_id    INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    total_bytes BIGINT    DEFAULT 0 NOT NULL, -- actual usage
    quota_bytes BIGINT    DEFAULT NULL,       -- soft or hard cap
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE usage_log
(
    id          SERIAL PRIMARY KEY,
    vault_id    INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    measured_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    total_bytes BIGINT  NOT NULL
);

-- Trigger to update the updated_at field on vault table

CREATE OR REPLACE FUNCTION update_timestamp()
RETURNS TRIGGER AS $$
BEGIN
   NEW.updated_at = CURRENT_TIMESTAMP;
RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_sync_timestamp BEFORE UPDATE ON sync
    FOR EACH ROW EXECUTE FUNCTION update_timestamp();

