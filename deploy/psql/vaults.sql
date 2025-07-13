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
    s3_api_key_id INTEGER REFERENCES s3_api_keys (api_key_id) ON DELETE CASCADE,
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
    api_key_id    INTEGER REFERENCES s3_api_keys (api_key_id) ON DELETE CASCADE,
    bucket_id     INTEGER REFERENCES s3_buckets (id) ON DELETE CASCADE,
);

CREATE TABLE backup_policy
(
    vault_id        INTEGER PRIMARY KEY REFERENCES vault (id) ON DELETE CASCADE,
    backup_interval INTERVAL DEFAULT '1 day',
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
    backup_id INTEGER REFERENCES backu_policy (id) ON DELETE CASCADE,
    type      VARCHAR(12) NOT NULL CHECK (type IN ('local', 's3')),
    path      TEXT DEFAULT NULL,
    bucket_id INTEGER REFERENCES s3_buckets (id) ON DELETE CASCADE,
    prefix    TEXT DEFAULT NULL
);

CREATE TABLE sync
(
    id              SERIAL PRIMARY KEY,
    interval        INTERVAL DEFAULT '5 minute',
    conflict_policy VARCHAR(12)      DEFAULT 'keep_local' CHECK (conflict_policy IN ('keep_local', 'keep_remote', 'ask')),
    strategy        VARCHAR(12)      DEFAULT 'sync' CHECK (strategy IN ('sync', 'mirror', 'merge')),
    enabled         BOOLEAN NOT NULL DEFAULT FALSE,
    last_sync_at    TIMESTAMP        DEFAULT NULL,
    last_success_at TIMESTAMP        DEFAULT NULL,
    created_at      TIMESTAMP        DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP        DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE v2v_sync
(
    sync_id INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    src     INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    dst     INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,

    UNIQUE (src, dst)
);

CREATE TABLE proxy_sync
(
    sync_id                  INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    vault_id                 INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    cache_thumbnails         BOOLEAN NOT NULL DEFAULT TRUE,
    cache_full_size_objects  BOOLEAN NOT NULL DEFAULT TRUE,
    max_cache_size           BIGINT NOT NULL DEFAULT 10 * 1024 * 1024 * 1024, -- 10 GB

    UNIQUE (vault_id) -- Only one proxy sync per vault
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

