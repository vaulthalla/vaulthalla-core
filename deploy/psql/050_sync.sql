-- ##################################
-- Sync & Backup Schema
-- ##################################

CREATE TABLE IF NOT EXISTS backup_policy
(
    id              SERIAL PRIMARY KEY,
    vault_id        INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    backup_interval BIGINT NOT NULL DEFAULT 86400,
    last_backup_at  TIMESTAMP DEFAULT NULL,
    last_success_at TIMESTAMP DEFAULT NULL,
    retention       INTERVAL DEFAULT NULL,
    retry_count     INTEGER NOT NULL DEFAULT 0,
    enabled         BOOLEAN NOT NULL DEFAULT FALSE,
    last_error      TEXT DEFAULT NULL,
    status          VARCHAR(12) DEFAULT 'idle'
    CHECK (status IN ('idle', 'syncing', 'error'))
    );

CREATE TABLE IF NOT EXISTS sync
(
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

CREATE TABLE IF NOT EXISTS fsync
(
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    conflict_policy VARCHAR(12) DEFAULT 'keep_both'
    CHECK (conflict_policy IN ('overwrite', 'keep_both', 'keep_newer', 'keep_older', 'ask'))
    );

CREATE TABLE IF NOT EXISTS rsync
(
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    strategy        VARCHAR(8) DEFAULT 'cache'
    CHECK (strategy IN ('cache', 'sync', 'mirror')),
    conflict_policy VARCHAR(12) DEFAULT 'keep_local'
    CHECK (conflict_policy IN ('keep_local', 'keep_remote', 'keep_newer', 'keep_older', 'ask'))
    );

-- -----------------------------------
-- Sync Event (per-run health + audit)
-- -----------------------------------
CREATE TABLE IF NOT EXISTS sync_event
(
    id               SERIAL PRIMARY KEY,
    vault_id          INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,

    -- stable identity for a "run"
    run_uuid          UUID NOT NULL DEFAULT gen_random_uuid(),

    -- core timing
    timestamp_begin   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    timestamp_end     TIMESTAMP DEFAULT NULL,

    -- run state
    status            VARCHAR(12) NOT NULL DEFAULT 'running'
    CHECK (status IN ('pending', 'running', 'success', 'stalled', 'error', 'cancelled')),
    trigger           VARCHAR(12) NOT NULL DEFAULT 'schedule'
    CHECK (trigger IN ('schedule', 'manual', 'startup', 'webhook', 'retry')),
    retry_attempt     INTEGER NOT NULL DEFAULT 0,

    -- stall/error diagnostics
    heartbeat_at      TIMESTAMP DEFAULT NULL,
    stall_reason      TEXT DEFAULT NULL,
    error_code        TEXT DEFAULT NULL,
    error_message     TEXT DEFAULT NULL,

    -- summary counters (so dashboards don't need 4 joins)
    num_ops_total     BIGINT NOT NULL DEFAULT 0,
    num_failed_ops    BIGINT NOT NULL DEFAULT 0,
    num_conflicts     BIGINT NOT NULL DEFAULT 0,
    bytes_up          BIGINT NOT NULL DEFAULT 0,
    bytes_down        BIGINT NOT NULL DEFAULT 0,

    -- divergence / watermarks (optional but lets "diverged" be provable)
    divergence_detected      BOOLEAN NOT NULL DEFAULT FALSE,
    local_state_hash         TEXT DEFAULT NULL,
    remote_state_hash        TEXT DEFAULT NULL,

    -- attribution (multi-worker + debugging)
    config_hash       TEXT DEFAULT NULL,

    UNIQUE (vault_id, run_uuid)
    );

CREATE TABLE IF NOT EXISTS sync_conflicts
(
    id            SERIAL PRIMARY KEY,
    event_id      INTEGER NOT NULL REFERENCES sync_event (id) ON DELETE CASCADE,
    file_id       INTEGER NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,

    conflict_type VARCHAR(12) DEFAULT 'content'
    CHECK (conflict_type IN ('content', 'metadata', 'encryption')),
    resolved_at   TIMESTAMP DEFAULT NULL,

    resolution    VARCHAR(24) DEFAULT 'unresolved'
    CHECK (resolution IN ('unresolved', 'kept_local', 'kept_remote', 'fixed_remote_encryption')),

    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (event_id, file_id)
    );

CREATE TABLE IF NOT EXISTS sync_conflict_artifacts (
                                                       id              SERIAL PRIMARY KEY,
                                                       conflict_id     INTEGER NOT NULL REFERENCES sync_conflicts(id) ON DELETE CASCADE,
    side            VARCHAR(12) NOT NULL
    CHECK (side IN ('local','upstream')),

    -- snapshot metadata for diff UI
    size_bytes      BIGINT,
    mime_type       VARCHAR(255),
    content_hash    VARCHAR(128),
    last_modified   TIMESTAMP,
    encryption_iv   TEXT,
    key_version     INTEGER,

    -- where to fetch bytes
    local_backing_path TEXT,     -- optional (for cached copy)

    created_at      TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

    UNIQUE (conflict_id, side)
    );

-- -----------------------------------
-- Throughput metrics (per-run buckets)
-- -----------------------------------
CREATE TABLE IF NOT EXISTS sync_throughput
(
    id               SERIAL PRIMARY KEY,

    vault_id          INTEGER NOT NULL,
    run_uuid          UUID NOT NULL,

    -- enforce association to a real sync_event run
    FOREIGN KEY (vault_id, run_uuid)
    REFERENCES sync_event (vault_id, run_uuid)
    ON DELETE CASCADE,

    metric_type      VARCHAR(12) NOT NULL
    CHECK (metric_type IN ('upload', 'download', 'rename', 'copy', 'delete')),

    num_ops          BIGINT NOT NULL DEFAULT 0,
    size_bytes       BIGINT NOT NULL DEFAULT 0,
    duration_ms      BIGINT NOT NULL DEFAULT 0,

    UNIQUE (vault_id, run_uuid, metric_type)
    );

-- ##################################
-- Indexes (dashboards + health queries)
-- ##################################

CREATE INDEX IF NOT EXISTS idx_sync_event_vault_begin
    ON sync_event (vault_id, timestamp_begin DESC);

CREATE INDEX IF NOT EXISTS idx_sync_event_vault_status_begin
    ON sync_event (vault_id, status, timestamp_begin DESC);

CREATE INDEX IF NOT EXISTS idx_sync_event_status_heartbeat
    ON sync_event (status, heartbeat_at);

-- common lookup for a specific run
CREATE INDEX IF NOT EXISTS idx_sync_event_vault_uuid
    ON sync_event (vault_id, run_uuid);

-- throughput lookups by run
CREATE INDEX IF NOT EXISTS idx_sync_throughput_run
    ON sync_throughput (vault_id, run_uuid);

-- ##################################
-- Triggers
-- ##################################

CREATE OR REPLACE FUNCTION update_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DO $$ BEGIN
CREATE TRIGGER update_sync_timestamp
    BEFORE UPDATE ON sync
    FOR EACH ROW
    EXECUTE FUNCTION update_timestamp();
EXCEPTION
    WHEN duplicate_object THEN NULL;
END $$;
