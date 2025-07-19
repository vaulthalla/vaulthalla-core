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
    vault_id        INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    interval        BIGINT NOT NULL DEFAULT 300, -- 5 minutes in seconds
    enabled         BOOLEAN NOT NULL DEFAULT TRUE,
    last_sync_at    TIMESTAMP DEFAULT NULL,
    last_success_at TIMESTAMP DEFAULT NULL,
    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (vault_id)  -- Only one sync per vault
);

CREATE TABLE fsync
(
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    conflict_policy VARCHAR(12) DEFAULT 'keep_both' CHECK (conflict_policy IN ('overwrite', 'keep_both', 'ask'))
);

CREATE TABLE rsync
(
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    strategy                 VARCHAR(8)      DEFAULT 'cache' CHECK (strategy IN ('cache', 'sync', 'mirror')),
    conflict_policy VARCHAR(12)      DEFAULT 'keep_local' CHECK (conflict_policy IN ('keep_local', 'keep_remote', 'ask'))
);

CREATE TABLE sync_conflicts
(
    id          SERIAL PRIMARY KEY,
    sync_id     INTEGER NOT NULL REFERENCES sync (id) ON DELETE CASCADE,
    file_id     INTEGER NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    conflict_type VARCHAR(12) DEFAULT 'content' CHECK (conflict_type IN ('content', 'metadata', 'both')),
    resolved_at TIMESTAMP DEFAULT NULL,
    resolution   VARCHAR(12) DEFAULT 'unresolved' CHECK (resolution IN ('unresolved', 'kept_local', 'kept_remote')),
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (sync_id, file_id)  -- Ensure no duplicate conflicts for the same file in a sync
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
