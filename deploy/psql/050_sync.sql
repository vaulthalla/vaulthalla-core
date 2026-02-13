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
    CHECK (conflict_policy IN ('overwrite', 'keep_both', 'ask'))
    );

CREATE TABLE IF NOT EXISTS rsync
(
    sync_id         INTEGER PRIMARY KEY REFERENCES sync (id) ON DELETE CASCADE,
    strategy        VARCHAR(8) DEFAULT 'cache'
    CHECK (strategy IN ('cache', 'sync', 'mirror')),
    conflict_policy VARCHAR(12) DEFAULT 'keep_local'
    CHECK (conflict_policy IN ('keep_local', 'keep_remote', 'ask'))
    );

CREATE TABLE IF NOT EXISTS sync_conflicts
(
    id            SERIAL PRIMARY KEY,
    sync_id       INTEGER NOT NULL REFERENCES sync (id) ON DELETE CASCADE,
    file_id       INTEGER NOT NULL REFERENCES files (fs_entry_id) ON DELETE CASCADE,
    conflict_type VARCHAR(12) DEFAULT 'content'
    CHECK (conflict_type IN ('content', 'metadata', 'both')),
    resolved_at   TIMESTAMP DEFAULT NULL,
    resolution    VARCHAR(12) DEFAULT 'unresolved'
    CHECK (resolution IN ('unresolved', 'kept_local', 'kept_remote')),
    created_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE (sync_id, file_id)
    );

-- -----------------------------------
-- Sync Event (per-run health + audit)
-- -----------------------------------
CREATE TABLE IF NOT EXISTS sync_event
(
    id               SERIAL PRIMARY KEY,
    vault_id          INTEGER NOT NULL REFERENCES vault (id) ON DELETE CASCADE,
    sync_id           INTEGER NOT NULL REFERENCES sync (id) ON DELETE CASCADE,

    -- core timing
    timestamp_begin   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    timestamp_end     TIMESTAMP DEFAULT NULL,

    -- run state
    status            VARCHAR(12) NOT NULL DEFAULT 'running'
    CHECK (status IN ('running', 'success', 'stalled', 'error', 'cancelled')),
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
    local_change_seq_begin   BIGINT DEFAULT NULL,
    local_change_seq_end     BIGINT DEFAULT NULL,
    remote_change_seq_begin  BIGINT DEFAULT NULL,
    remote_change_seq_end    BIGINT DEFAULT NULL,
    local_state_hash         TEXT DEFAULT NULL,
    remote_state_hash        TEXT DEFAULT NULL,

    -- attribution (multi-worker + debugging)
    worker_id         TEXT DEFAULT NULL,
    build_version     TEXT DEFAULT NULL,
    git_sha           TEXT DEFAULT NULL,
    config_hash       TEXT DEFAULT NULL
    );

-- -----------------------------------
-- Throughput metrics (per-run buckets)
-- -----------------------------------
CREATE TABLE IF NOT EXISTS sync_throughput
(
    id               SERIAL PRIMARY KEY,
    sync_event_id    INTEGER NOT NULL REFERENCES sync_event (id) ON DELETE CASCADE,

    metric_type      VARCHAR(12) NOT NULL
    CHECK (metric_type IN ('upload', 'download', 'rename', 'copy', 'delete')),

    num_ops          BIGINT NOT NULL DEFAULT 0,
    size_bytes       BIGINT NOT NULL DEFAULT 0,

    timestamp_begin  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    timestamp_end    TIMESTAMP DEFAULT NULL,

    UNIQUE (sync_event_id, metric_type)
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

CREATE INDEX IF NOT EXISTS idx_sync_throughput_event
    ON sync_throughput (sync_event_id);

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
