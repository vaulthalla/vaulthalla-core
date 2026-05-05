Brother, #7 is the right instinct. Not because dashboards are sexy by themselves — most dashboards are just confetti with axes — but because Vaulthalla is infrastructure. A serious health/observability center makes every other feature feel safer: FUSE, S3/R2, cache, sync, shares, encryption, DB, runtime services, the whole Norse machinery.

After checking the current stat surface, here is the reality:

Vaulthalla already has a decent core. `vault::model::Stat` currently wraps `Capacity` and emits `latest_sync_event`; the capacity model already computes logical size, physical size, cache size, free space, file count, directory count, average file size, largest file, and top file extensions by size.

Cache stats are also real, not vapor. `CacheStats` tracks hits, misses, evictions, inserts, invalidations, bytes read/written, used/capacity bytes, and operation latency, with derived hit rate/free/avg/max latency helpers.

The web admin dashboard currently renders FS cache and HTTP cache cards, and the vault page renders capacity plus latest sync health.

So the move is not “build a stats dashboard.” The move is:

> Turn the existing stats islands into an Admin Health Center that answers: **Is my data safe? Is Vaulthalla fast? Is something wrong right now?**

## Maintainer note:

Each phase of this roadmap is going to be a relatively large 'plumb it end-to-end' task where codex should implement the feature under the correct area (a lot will go under /core/include/stats/ but some will naturally settle into various modules inside /core/ . hpp goes in /core/include and  cpp goes in /core/src/ . Keep the new dashboard components thematically inline with existing stats dashboard components. Components may fall under the top level admin dashboard or descend into per vault or user usage as needed.

## Proposed roadmap: Admin Health Center / Stats Expansion

I would ship this as a sequence of Codex-sized vertical slices, keeping historical snapshots last until live truth is strong.

## Phase 0 — Stabilize the stats contract

Goal: stop stuffing everything into one ambiguous `stats.vault` blob.

Current websocket handlers are:

```text
stats.vault
stats.fs.cache
stats.http.cache
```

registered in the websocket handler surface.

Keep them, but add a cleaner grouped API:

```text
stats.dashboard.overview
stats.system.health
stats.system.runtime
stats.system.cache
stats.system.threadpools
stats.system.db
stats.system.fuse
stats.system.operations
stats.system.connections
stats.system.storage
stats.system.retention
stats.vault.overview
stats.vault.sync
stats.vault.activity
stats.vault.security
stats.vault.shares
stats.vault.recovery
stats.vault.operations
stats.vault.storage
stats.vault.retention
```

Do not delete existing commands yet. Let the current UI keep breathing while the new surface grows.

Backend model structure:

```text
core/include/stats/model/SystemHealth.hpp
core/include/stats/model/RuntimeStats.hpp
core/include/stats/model/ThreadPoolStats.hpp
core/include/stats/model/DbStats.hpp
core/include/stats/model/FuseStats.hpp
core/include/stats/model/OperationStats.hpp
core/include/stats/model/ConnectionStats.hpp
core/include/stats/model/StorageBackendStats.hpp
core/include/stats/model/RetentionStats.hpp
core/include/vault/model/stats/Overview.hpp
core/include/vault/model/stats/SyncHealth.hpp
core/include/vault/model/stats/Activity.hpp
core/include/vault/model/stats/Security.hpp
core/include/vault/model/stats/ShareStats.hpp
core/include/vault/model/stats/RecoveryReadiness.hpp
```

Frontend model structure:

```text
web/src/models/stats/systemHealth.ts
web/src/models/stats/runtimeStats.ts
web/src/models/stats/threadPoolStats.ts
web/src/models/stats/dbStats.ts
web/src/models/stats/fuseStats.ts
web/src/models/stats/operationStats.ts
web/src/models/stats/connectionStats.ts
web/src/models/stats/storageBackendStats.ts
web/src/models/stats/retentionStats.ts
web/src/models/stats/vaultOverview.ts
web/src/models/stats/vaultSync.ts
web/src/models/stats/vaultActivity.ts
web/src/models/stats/vaultSecurity.ts
web/src/models/stats/vaultShares.ts
web/src/models/stats/vaultRecovery.ts
```

Codex task:

```text
Add typed backend/frontend stats models and websocket commands without changing dashboard UI yet.
Preserve existing stats.vault, stats.fs.cache, and stats.http.cache behavior.
Add unit/type coverage where existing patterns allow.
```

Acceptance criteria:

```text
- Existing admin dashboard still works.
- Existing vault stats dashboard still works.
- New websocket command names compile and return placeholder/partial payloads.
- WebSocketCommandMap has typed entries for every new stats command.
```

## Phase 1 — System Health Center MVP

This should be the first visible needle mover.

The CLI already has `vh status`, and it is doing exactly the kind of runtime aggregation the web dashboard needs: runtime services, protocol service readiness, dependency sanity, FUSE session presence, and shell admin UID status.

Do not duplicate that logic in a dumb way. Extract the status-building logic into a reusable runtime/system stats model, then let both CLI and websocket use it.

Backend target:

```text
core/include/stats/model/SystemHealth.hpp
core/src/stats/model/SystemHealth.cpp
core/src/protocols/ws/handler/Stats.cpp
core/src/protocols/shell/commands/system.cpp
```

Metrics to expose:

```text
overall_status: healthy | degraded | critical
runtime_all_running
service_count
services[]
  - entry_name
  - service_name
  - running
  - interrupted
protocols
  - protocol_service_running
  - websocket_configured
  - websocket_ready
  - http_preview_configured
  - http_preview_ready
deps
  - storage_manager
  - api_key_manager
  - auth_manager
  - session_manager
  - secrets_manager
  - sync_controller
  - fs_cache
  - shell_usage_manager
  - http_cache_stats
  - fuse_session
shell
  - admin_uid_bound
```

This is immediately useful because `runtime::Deps` already exposes a `SanityStatus`, and `ProtocolService` already exposes a runtime protocol status.

Frontend target:

```text
web/src/app/(app)/(admin)/dashboard/page.tsx
web/src/components/stats/SystemHealth.tsx
web/src/components/stats/ServiceStatusGrid.tsx
web/src/components/stats/ProtocolHealthCard.tsx
web/src/components/stats/DependencySanityCard.tsx
```

UI layout:

```text
Admin Dashboard
├── Health Summary Strip
│   ├── Overall: healthy/degraded/critical
│   ├── Services running
│   ├── Protocols ready
│   ├── FUSE mounted/session present
│   └── Last refresh
├── Runtime Services
├── Protocols
├── Dependency Sanity
├── FS Cache
└── HTTP Cache
```

Codex task:

```text
Extract CLI status internals into reusable SystemHealth model.
Expose stats.system.health over websocket with admin-only access.
Add SystemHealth component to admin dashboard above existing cache cards.
```

Acceptance criteria:

```text
- Web dashboard shows the same health truth as vh status.
- Degraded protocol/dependency states render visibly.
- No shell command regression.
- Existing CacheStats cards remain.
```

My hot take: this alone makes the product feel 2x more mature.

## Phase 2 — Thread pool and background service pressure

Vaulthalla has multiple thread pools — FUSE, sync, thumbnail, HTTP, stats — and `ThreadPoolManager` already has queue depth and worker count accessors.

Expose this. It is rare, industrial, and genuinely useful.

Backend metrics:

```text
stats.system.threadpools

pools[]
  - name: fuse | sync | thumb | http | stats
  - queue_depth
  - worker_count
  - borrowed_worker_count
  - idle_worker_count, if cheap
  - pressure_ratio = queue_depth / worker_count
  - status: idle | normal | pressured | saturated
```

Probably add a small snapshot method to `ThreadPoolManager` instead of letting stats code poke all pools manually:

```cpp
ThreadPoolManager::snapshot()
ThreadPool::snapshot()
```

Add only what is already cheap:

```text
queueDepth()
workerCount()
hasIdleWorker()
hasBorrowedWorker()
```

Avoid per-job history for now. That becomes a telemetry backend, and we are not trying to accidentally build Prometheus with a beard.

Frontend:

```text
web/src/components/stats/ThreadPoolStats.tsx
```

UI:

```text
Pool       Workers   Queue   Pressure
FUSE       8         0       idle
Sync       8         12      pressured
Thumb      7         2       normal
HTTP       7         0       idle
Stats      7         0       idle
```

Codex task:

```text
Add thread pool snapshot structs and websocket stats.system.threadpools.
Render queue depth, worker count, and pressure status on admin dashboard.
```

Acceptance criteria:

```text
- No new locks on hot paths beyond existing queueDepth/worker checks.
- Dashboard refreshes every 5–10s.
- Saturated pool state is obvious.
```

## Phase 3 — FUSE operations stats

This is the one that makes Vaulthalla feel like it came out of a systems lab instead of a SaaS template.

FUSE operations are centralized in `core/src/fuse/Bridge.cpp`: `getattr`, `readdir`, `lookup`, `create`, `open`, `read`, `write`, `mkdir`, `rename`, `unlink`, `rmdir`, `fsync`, `statfs`, etc.

Add a `FuseStats` model with atomic counters and latency buckets, similar in spirit to `CacheStats`.

Backend model:

```text
core/include/stats/model/FuseStats.hpp
core/src/stats/model/FuseStats.cpp
```

Metrics:

```text
total_ops
ops_by_type
  - getattr
  - lookup
  - readdir
  - open
  - read
  - write
  - create
  - mkdir
  - rename
  - unlink
  - rmdir
  - fsync
  - statfs
errors_by_errno
read_bytes
write_bytes
open_handles_current
open_handles_peak
latency_by_type
  - count
  - total_us
  - max_us
  - avg_ms
```

Instrumentation strategy:

Add a scoped timer/helper:

```cpp
struct ScopedFuseOp {
    FuseStats* stats;
    FuseOp op;
    steady_clock::time_point start;
    ~ScopedFuseOp();
    void success(bytes = 0);
    void error(errno);
};
```

Then add it to each FUSE op.

For read/write bytes:

```text
read(): record bytes returned by pread
write(): record bytes returned by pwrite
```

For open handles, you already call `storageManager->registerOpenHandle(ino)` and `closeOpenHandle(ino)` in FUSE open/release, so either surface existing storage manager open handle state or mirror into `FuseStats`.

Add to `runtime::Deps`:

```cpp
std::shared_ptr<stats::model::FuseStats> fuseStats;
```

Expose:

```text
stats.system.fuse
```

Frontend:

```text
web/src/components/stats/FuseStats.tsx
```

UI:

```text
FUSE Filesystem
├── Ops/sec
├── Read/write bytes
├── Open handles
├── Error rate
├── Top operations
└── Slowest operation type
```

Do not overdo percentiles yet. `max` and `avg` are enough for MVP. p95 requires histograms or rolling windows, and that belongs in a later histogram pass.

Codex task:

```text
Add FuseStats atomic model, wire it into runtime deps, instrument FUSE Bridge operations, and expose stats.system.fuse over websocket.
```

Acceptance criteria:

```text
- All FUSE operations increment counters.
- Errors are tracked by errno.
- Read/write bytes are tracked.
- Dashboard displays live FUSE op mix and health.
- No meaningful overhead or blocking in FUSE hot path.
```

## Phase 4 — Vault Sync Health 2.0

The sync schema is richer than the current UI uses. You already have `sync_event`, `sync_conflicts`, `sync_throughput`, heartbeat, retry attempt, status, bytes up/down, failed ops, conflicts, divergence flags, local/remote state hashes, and retention cleanup.

The model also already exposes status, trigger, retry attempt, heartbeat, failed ops, conflicts, bytes up/down, throughputs, and divergence data.

So build `stats.vault.sync` as a proper rollup, not just “latest event.”

Metrics:

```text
current_state: idle | running | stalled | error
latest_event
last_success_at
last_sync_at
pending_or_running_count
oldest_running_age_seconds
stalled_count
error_count_24h
error_count_7d
failed_ops_24h
conflicts_open
conflicts_24h
retries_24h
bytes_up_24h
bytes_down_24h
avg_throughput_bytes_per_sec_24h
peak_throughput_bytes_per_sec_24h
divergence_detected
last_error
```

Queries needed:

```text
db/query/sync/Event.hpp/.cpp
db/query/sync/Stats.hpp/.cpp maybe cleaner
```

Suggested SQL shape:

```sql
SELECT count(*)
FROM sync_event
WHERE vault_id = $1
  AND status IN ('pending', 'running');

SELECT *
FROM sync_event
WHERE vault_id = $1
ORDER BY timestamp_begin DESC
LIMIT 1;

SELECT max(timestamp_end)
FROM sync_event
WHERE vault_id = $1
  AND status = 'success';

SELECT count(*)
FROM sync_event
WHERE vault_id = $1
  AND status = 'error'
  AND timestamp_begin > now() - interval '24 hours';
```

Frontend:

Replace/extend `LatestSyncHealth` with:

```text
VaultSyncHealth
├── Current state badge
├── Last success
├── Oldest running/stalled age
├── Errors 24h/7d
├── Throughput up/down
├── Conflicts
└── Divergence
```

Codex task:

```text
Add SyncHealth rollup queries and model.
Expose stats.vault.sync.
Update vault dashboard to consume SyncHealth instead of only latest_sync_event.
Keep latest_sync_event compatibility inside stats.vault.
```

Acceptance criteria:

```text
- Vault dashboard answers “is sync healthy?” without reading raw event details.
- Stalled/error/divergence states scream visually.
- Last successful sync is visible.
```

## Phase 5 — Vault Activity and Mutation Stats

This answers “what happened recently?”

Some of this can come from existing tables:

```text
fs_entry.created_at / updated_at
files_trashed.trashed_at
operations table
share_link.created_at / revoked_at
share_access_event
file_activity / audit_log, once consistently written
```

The files schema already has `files_trashed`, `operations`, `file_activity`, and `audit_log`.
Link sharing has `share_link`, `share_session`, `share_upload`, and `share_access_event`.

Metrics:

```text
stats.vault.activity

last_activity_at
uploads_24h
uploads_7d
deletes_24h
deletes_7d
renames_24h
moves_24h
copies_24h
restores_24h, if restore exists later
bytes_added_24h
bytes_removed_24h
top_active_users
top_touched_paths
```

Caveat: if upload/delete/move/rename events are not reliably recorded in `file_activity` or `operations`, do not fake it from `fs_entry.updated_at`. Codex should first add event writes to the FS mutation paths.

Backend implementation order:

```text
1. Ensure fs.upload.finish records file_activity.
2. Ensure fs.entry.delete records file_activity and/or audit_log.
3. Ensure move/rename/copy operations get completed status and completed_at.
4. Build rollup queries from those event tables.
```

Frontend:

```text
web/src/components/vault/VaultStatsDashboard/VaultActivity/Component.tsx
```

UI:

```text
Activity
├── Last activity
├── Uploads 24h / 7d
├── Deletes 24h / 7d
├── Moves/renames 24h
├── Bytes added/removed
└── Recent mutation timeline
```

Codex task:

```text
Wire mutation event recording for upload/delete/move/rename/copy, then expose vault activity rollups.
```

Acceptance criteria:

```text
- Activity numbers come from explicit events, not guesses.
- Mutations through web and FUSE both contribute where possible.
- Dashboard shows a recent activity timeline.
```

## Phase 6 — Share Observatory Lite

You already built robust link sharing, so dashboard it.

Metrics:

```text
stats.vault.shares

active_links
expired_links
revoked_links
links_created_24h
links_revoked_24h
public_links
email_validated_links
downloads_24h
uploads_24h
denied_attempts_24h
rate_limited_attempts_24h
top_links_by_access
recent_share_events
```

Source tables:

```text
share_link
share_session
share_upload
share_access_event
```

The share schema already stores access counts, download/upload counts, max constraints, revoked/disabled/expires fields, and access events with status and bytes transferred.

Frontend:

```text
web/src/components/vault/VaultStatsDashboard/ShareStats/Component.tsx
web/src/components/stats/ShareSystemStats.tsx
```

UI:

```text
Sharing
├── Active links
├── Downloads/uploads 24h
├── Denied/rate-limited attempts
├── Most active link
└── Recent share access events
```

Codex task:

```text
Add vault share stats model/query and render ShareStats on vault dashboard.
Optionally add global share summary to admin dashboard.
```

Acceptance criteria:

```text
- Admin can see whether public sharing is healthy or being abused.
- Revoked/expired/denied/rate-limited numbers are visible.
```

## Phase 7 — DB Health

This is admin-only and should be pragmatic, not DBA cosplay.

Metrics:

```text
stats.system.db

connected
database_name
db_size_bytes
connections_active
connections_idle
connections_max, if available
cache_hit_ratio
slow_query_count, if pg_stat_statements exists
deadlocks
temp_bytes
oldest_transaction_age_seconds
largest_tables[]
index_bloat_estimate, nice-to-have
```

MVP queries:

```sql
SELECT pg_database_size(current_database());

SELECT state, count(*)
FROM pg_stat_activity
WHERE datname = current_database()
GROUP BY state;

SELECT
  sum(blks_hit)::float / nullif(sum(blks_hit + blks_read), 0) AS cache_hit_ratio
FROM pg_stat_database
WHERE datname = current_database();
```

Do not require `pg_stat_statements` for MVP. Detect it and show “not enabled” if absent.

Backend:

```text
core/include/stats/model/DbStats.hpp
core/src/stats/model/DbStats.cpp
core/src/db/query/stats/DbStats.cpp
```

Frontend:

```text
web/src/components/stats/DbHealth.tsx
```

Codex task:

```text
Add DB health stats with safe pg_stat queries and admin-only websocket command.
```

Acceptance criteria:

```text
- Works on stock local PostgreSQL.
- Does not require extensions.
- Extension-only fields degrade gracefully.
```

## Phase 8 — Vault Security / Integrity

This is where the dashboard becomes trust infrastructure.

Metrics:

```text
stats.vault.security

encryption_status: encrypted | mixed | unknown
current_key_version
key_created_at
key_age_days
trashed_key_versions_count
files_current_key_version
files_legacy_key_version
checksum_mismatch_count
unauthorized_access_attempts_24h
last_permission_change
last_share_policy_change
```

Source tables already include:

```text
vault_keys
vault_keys_trashed
files.encrypted_with_key_version
files.content_hash
audit_log
share_access_event denied/failed
role assignment tables
```

For encryption status, keep it simple:

```text
fully_encrypted:
  all files have encrypted_with_key_version = current vault_keys.version

mixed:
  some files have older key versions

unknown:
  unable to determine
```

Do not claim integrity checks unless you actually run checksum verification. Add:

```text
integrity_check_status: not_run | running | passed | failed
last_integrity_check_at
checksum_mismatch_count
```

only after a real integrity job exists.

Codex task:

```text
Add vault security rollup for key age/version coverage and denied access attempts.
Do not implement integrity check claims until a real verifier exists.
```

Acceptance criteria:

```text
- Dashboard shows key age and mixed/legacy encryption state.
- Denied share/RBAC attempts are visible.
- No fake “integrity passed” badge.
```

## Phase 8A — Backup / Recovery Readiness

This is the biggest missing trust metric: **is this vault recoverable if something goes wrong?**

Vaulthalla already has `backup_policy` state, so expose it as read-only vault recovery telemetry. Do not perform backups from this command.

Backend source:

```text
backup_policy
  - vault_id
  - backup_interval
  - last_backup_at
  - last_success_at
  - retention
  - retry_count
  - enabled
  - last_error
  - status
```

Expose:

```text
stats.vault.recovery
```

Metrics:

```text
backup_enabled
backup_status: idle | syncing | error | disabled | unknown
backup_interval_seconds
retention_seconds
last_backup_at
last_success_at
last_success_age_seconds
next_expected_backup_at
backup_stale
missed_backup_count_estimate
retry_count
last_error
recovery_readiness: ready | stale | failing | disabled | unknown
time_since_last_verified_good_state
```

Status rules:

```text
ready:
  enabled = true
  last_success_at exists
  last_success_age <= backup_interval * 2
  status != error

stale:
  enabled = true
  last_success_at missing or older than backup_interval * 2

failing:
  status = error or last_error is present and no newer success

disabled:
  enabled = false

unknown:
  no backup_policy row exists
```

Frontend:

```text
web/src/models/stats/vaultRecovery.ts
web/src/components/vault/VaultStatsDashboard/RecoveryReadiness/Component.tsx
```

UI:

```text
Recovery Readiness
├── Readiness: ready/stale/failing/disabled/unknown
├── Last successful backup
├── Time since last verified good state
├── Next expected backup
├── Retry count
├── Retention
└── Last error
```

Codex task:

```text
Add backup_policy-based vault recovery readiness stats.
Expose stats.vault.recovery.
Render RecoveryReadiness on the vault dashboard.
Do not perform backups from this command; this is read-only telemetry.
```

Acceptance criteria:

```text
- Operators can instantly see whether a vault has a recent successful backup.
- Missing/disabled/stale backup state is visually obvious.
- No fake “recoverable” badge without last_success_at evidence.
```

## Phase 8B — Transfer / Operation Queue Health

This answers “is work stuck?” It should cover long-running or failed filesystem/share operations, not just completed activity counts.

Backend sources:

```text
operations
  - operation
  - target
  - source_path
  - destination_path
  - created_at
  - status
  - completed_at
  - error

share_upload
  - share_id
  - share_session_id
  - target_path
  - expected_size_bytes
  - received_size_bytes
  - status
  - error
  - started_at
  - completed_at
```

Expose:

```text
stats.system.operations
stats.vault.operations
```

Metrics:

```text
pending_operations
in_progress_operations
failed_operations_24h
cancelled_operations_24h
oldest_pending_operation_age_seconds
oldest_in_progress_operation_age_seconds
operations_by_type
  - move
  - copy
  - rename
operations_by_status
  - pending
  - in_progress
  - success
  - error
  - cancelled

active_share_uploads
stalled_share_uploads
failed_share_uploads_24h
upload_bytes_expected_active
upload_bytes_received_active
oldest_active_upload_age_seconds
recent_operation_errors[]
```

Stalled heuristics:

```text
operation stale:
  status in pending/in_progress
  created_at older than configurable threshold, e.g. 15 minutes

share upload stale:
  status in pending/receiving
  started_at older than configurable threshold
  received_size_bytes not advancing if instrumentation exists; otherwise age-only
```

Frontend:

```text
web/src/models/stats/operationStats.ts
web/src/components/stats/OperationQueueStats.tsx
web/src/components/vault/VaultStatsDashboard/OperationQueue/Component.tsx
```

UI:

```text
Operation Queue
├── Pending
├── In progress
├── Failed 24h
├── Oldest pending
├── Active uploads
├── Stalled uploads
└── Recent errors
```

Codex task:

```text
Add operation/share-upload queue health rollups.
Expose stats.system.operations and stats.vault.operations.
Render global operation pressure on the admin dashboard and vault-specific operation health on vault dashboards.
```

Acceptance criteria:

```text
- Operators can see stuck moves/copies/renames/uploads.
- Failed/cancelled operations are visible.
- Metrics come from operation/upload tables, not guesses.
```

## Phase 8C — Connection and Session Health

This answers “who is connected, and are sessions behaving?”

Backend sources / instrumentation seams:

```text
ConnectionLifecycleManager
  - sweep_interval
  - unauthenticated_session_timeout
  - idle_timeout

Session
  - mode: unauthenticated | human | share_pending | share
  - connectionOpenedAt
  - userAgent
  - ipAddress
  - share session state
```

Expose:

```text
stats.system.connections
```

Metrics:

```text
active_ws_sessions_total
active_human_sessions
active_share_sessions
active_share_pending_sessions
active_unauthenticated_sessions
oldest_session_age_seconds
oldest_unauthenticated_session_age_seconds
idle_timeout_minutes
unauthenticated_timeout_seconds
sweep_interval_seconds
sessions_swept_24h, if instrumented
connections_opened_24h, if instrumented
connections_closed_24h, if instrumented
connection_errors_24h, if instrumented
sessions_by_mode
top_user_agents
top_remote_ips_limited
```

Implementation notes:

```text
If there is no central session registry yet, add a lightweight stats/registry seam in the websocket layer rather than scraping logs.
Do not expose sensitive full IP/user-agent data unless existing admin UI already treats this as acceptable.
Prefer counts/top limited summaries.
```

Frontend:

```text
web/src/models/stats/connectionStats.ts
web/src/components/stats/ConnectionStats.tsx
```

UI:

```text
Connection Health
├── Active sessions
├── Human sessions
├── Share sessions
├── Unauthenticated sessions
├── Oldest session
├── Idle timeout
├── Sweep interval
└── Recent close/error counts if instrumented
```

Codex task:

```text
Add websocket/session health stats.
Expose stats.system.connections.
Render ConnectionStats on the admin dashboard.
Keep the payload admin-only and avoid leaking unnecessary sensitive detail.
```

Acceptance criteria:

```text
- Operators can distinguish normal connected users from suspicious unauthenticated buildup.
- Share traffic and human dashboard traffic are separated.
- Idle/unauth timeout settings are visible.
```

## Phase 8D — Storage Backend / Provider Health

This answers “is the storage backend actually healthy?”

Capacity stats show logical/physical/cache/free size, but they do not directly show backend/provider health. Vaulthalla supports local and S3-compatible vaults, so operators need to know whether backing providers are configured, available, and close to write failure.

Backend sources / instrumentation seams:

```text
vault
  - type: local | s3
  - quota
  - is_active
  - allow_fs_write

s3
  - bucket
  - encrypt_upstream
  - api_key_id

storage::Engine
  - type()
  - getVaultSize()
  - getCacheSize()
  - freeSpace()
```

Expose:

```text
stats.system.storage
stats.vault.storage
```

Metrics:

```text
vault_count_total
local_vault_count
s3_vault_count
active_vault_count
inactive_vault_count

per_vault:
  vault_id
  vault_name
  type: local | s3
  active
  allow_fs_write
  quota_bytes
  free_space_bytes
  cache_size_bytes
  vault_size_bytes
  backend_status: healthy | degraded | error | unknown
  min_free_space_ok
  upstream_encryption_enabled, for s3
  bucket, for s3 admin display if acceptable
```

Optional instrumentation metrics:

```text
provider_ops_total
provider_errors_total
provider_error_rate
provider_latency_avg_ms
provider_latency_max_ms
last_provider_error
last_provider_success_at
```

Implementation notes:

```text
Do not fake S3 latency if storage provider operations are not currently instrumented.
First version may expose configuration/availability/free-space/min-free-space status.
If adding provider op counters is clean, instrument S3/local engine operation boundaries similarly to FuseStats.
```

Frontend:

```text
web/src/models/stats/storageBackendStats.ts
web/src/components/stats/StorageBackendStats.tsx
web/src/components/vault/VaultStatsDashboard/StorageBackend/Component.tsx
```

UI:

```text
Storage Backend
├── Local vs S3 vaults
├── Active/inactive vaults
├── Backend status
├── Free space / quota
├── S3 upstream encryption
├── Provider error rate, if instrumented
└── Last provider error, if available
```

Codex task:

```text
Add storage backend/provider health stats.
Expose system and per-vault storage backend health.
Render StorageBackendStats on admin and vault dashboards.
Do not invent provider latency/error metrics unless instrumented.
```

Acceptance criteria:

```text
- Operators can see whether local/S3 backing storage is configured and healthy.
- S3 vaults expose upstream encryption and bucket/provider status.
- Free-space danger is visible before writes fail.
```

## Phase 8E — Retention and Cleanup Pressure

This answers “is old state piling up?” A storage engine dashboard should show whether trash, audit logs, sync events, share events, and cache retention are under control.

Backend sources:

```text
files_trashed
audit_log
file_activity
sync_event
cache_index
share_access_event
config retention values
```

Expose:

```text
stats.system.retention
stats.vault.retention
```

Metrics:

```text
trashed_files_count
trashed_bytes_total
oldest_trashed_age_seconds
trashed_files_past_retention_count
trashed_bytes_past_retention

sync_events_total
sync_events_past_retention_count
sync_event_retention_days
sync_event_max_entries

audit_log_entries_total
oldest_audit_log_age_seconds
audit_log_entries_past_retention_count

share_access_events_total
oldest_share_access_event_age_seconds

cache_entries_total
cache_bytes_total
cache_eviction_candidates
cache_entries_expired

cleanup_status: healthy | warning | overdue | unknown
```

Status rules:

```text
healthy:
  no major category past retention threshold

warning:
  some records past retention but below critical threshold

overdue:
  large backlog past retention
  or oldest retained item far exceeds configured retention

unknown:
  retention config or source tables unavailable
```

Frontend:

```text
web/src/models/stats/retentionStats.ts
web/src/components/stats/RetentionPressure.tsx
web/src/components/vault/VaultStatsDashboard/RetentionPressure/Component.tsx
```

UI:

```text
Retention / Cleanup
├── Trash size
├── Trash past retention
├── Sync event backlog
├── Audit log age
├── Cache cleanup pressure
└── Cleanup status
```

Codex task:

```text
Add retention and cleanup pressure stats from existing metadata tables and config retention settings.
Expose system and per-vault retention stats.
Render retention pressure where it belongs: global admin for system-wide cleanup and vault dashboard for vault-specific trash/cache pressure.
```

Acceptance criteria:

```text
- Operators can see if trash/audit/sync/cache cleanup is falling behind.
- Metrics are based on real metadata tables and configured retention policies.
- No destructive cleanup is performed by stats commands.
```

## Phase 9 — Historical snapshots and trends

Only after the live dashboard is good.

Add a table:

```sql
CREATE TABLE IF NOT EXISTS stats_snapshot (
    id BIGSERIAL PRIMARY KEY,
    scope VARCHAR(24) NOT NULL CHECK (scope IN ('system', 'vault')),
    vault_id INTEGER REFERENCES vault(id) ON DELETE CASCADE,
    snapshot_type VARCHAR(32) NOT NULL,
    payload JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
```

Or more normalized, but JSONB snapshots are fine for dashboard trend history.

Snapshot types:

```text
vault.capacity
vault.sync
vault.activity
system.cache
system.fuse
system.threadpools
system.db
```

Cadence:

```text
- every 5 minutes for runtime/cache/fuse
- hourly for vault capacity/activity
- daily compact retention after 30 days
```

This enables:

```text
capacity growth
cache hit-rate trend
sync error trend
FUSE ops trend
database size trend
```

Codex task:

```text
Add lightweight stats snapshot table and background collector service.
Render simple 24h/7d trend lines after live dashboard is complete.
```

Acceptance criteria:

```text
- Stats snapshots never block hot paths.
- Retention is configurable.
- Dashboard can show trend lines without querying raw operational tables heavily.
```

## What I would remove or defer from the old roadmap

### Defer: compression ratio

Unless Vaulthalla is actually compressing vault contents, this becomes a “decorative lie.” The `Capacity` model has `compression_ratio`, but serialization currently does not emit it, and I did not see evidence here that it is actually calculated.

Keep the field if you want, but do not put it on the dashboard until it is real.

### Defer: encryption overhead %

Same issue. The model has overhead fields, but the current serialized capacity payload does not expose them and the constructor does not compute them.

Better first version:

```text
logical_size
physical_size
cache_size
physical/logical ratio
```

Call it “storage ratio,” not “encryption overhead,” unless you can isolate encryption overhead from backing layout/cache/provider behavior.

### Defer: cost per GB

Cool later, but not now. It requires provider-aware pricing configuration, and anything half-baked here will look unserious.

### Defer: write amplification

Very cool graybeard metric, but only after FUSE stats, sync stats, and provider operation stats are clean.

### Defer: user active days

Useful, but it gets surveillance-y fast. Build invites first, then user security/session stats, then admin activity reporting.

### Defer: p95 latency

For cache/FUSE/preview, p95 needs histogram/rolling bucket support. Current cache stats track count/total/max, not percentile buckets.

MVP:

```text
avg_ms
max_ms
count
```

Then add histograms later.

## The “ship this first” dashboard

If you want the first PR sequence to be elite but sane, I would ship these in order:

```text
1. SystemHealth card
2. ThreadPoolStats card
3. FuseStats card
4. VaultSyncHealth rollup
5. VaultActivity rollup
6. ShareStats rollup
7. DbHealth card
8. VaultSecurity rollup
9. RecoveryReadiness rollup
10. OperationQueue stats
11. ConnectionStats
12. StorageBackendStats
13. RetentionPressure stats
14. Historical snapshots
```

Rationale:

```text
- Phases 1–3 prove runtime health.
- Phases 4–6 prove vault behavior.
- Phases 7–8 prove infrastructure/security posture.
- Phases 8A–8E prove recoverability, stuck-work visibility, client/session visibility, backend health, and cleanup health.
- Historical snapshots should remain last because trends are less useful until live truth is solid.
```

## Recommended final layout

### Admin dashboard

```text
Admin Dashboard
├── Health Command Center
│   ├── Overall status
│   ├── Services running
│   ├── Protocols ready
│   ├── FUSE status
│   └── DB status
├── Runtime Services
├── Thread Pools
├── FUSE Filesystem
├── Connection Health
├── Operation Queue
├── Storage Backend Health
├── FS Cache
├── HTTP Preview Cache
├── Database Health
├── Global Share Activity
├── Retention / Cleanup Pressure
└── Trends
```

### Vault dashboard

```text
Vault Dashboard
├── Vault Hero
├── Capacity & Composition
├── Storage Backend
├── Sync Health
├── Recovery Readiness
├── Activity
├── Operation Queue
├── Sharing
├── Security / Integrity
├── Retention / Cleanup Pressure
└── Trends
```

## Codex execution plan

Here is the practical PR plan I would hand to Codex.

### PR 1 — SystemHealth

```text
Extract reusable SystemHealth from vh status.
Expose stats.system.health.
Render SystemHealth on admin dashboard.
```

### PR 2 — ThreadPoolStats

```text
Add ThreadPoolSnapshot / ThreadPoolManagerSnapshot.
Expose stats.system.threadpools.
Render queue depth and pressure.
```

### PR 3 — FuseStats

```text
Add FuseStats atomic model.
Wire into runtime deps.
Instrument FUSE Bridge operations.
Expose stats.system.fuse.
Render FUSE operations card.
```

### PR 4 — VaultSyncHealth

```text
Add sync rollup queries.
Expose stats.vault.sync.
Upgrade VaultStatsDashboard sync card.
```

### PR 5 — VaultActivity

```text
Ensure file mutations record activity.
Add vault activity rollup.
Render activity card.
```

### PR 6 — ShareStats

```text
Add share stats rollup from share_link/share_access_event/share_upload.
Render vault share observatory card.
```

### PR 7 — DbHealth

```text
Add DB health queries.
Expose stats.system.db.
Render DB card with graceful extension detection.
```

### PR 8 — VaultSecurity

```text
Add vault key age/version coverage/security rollup.
Render security card.
Avoid fake integrity claims.
```

### PR 8A — RecoveryReadiness

```text
Add backup_policy-based vault recovery readiness stats.
Expose stats.vault.recovery.
Render RecoveryReadiness on the vault dashboard.
```

### PR 8B — OperationQueueStats

```text
Add operation/share-upload queue health rollups.
Expose stats.system.operations and stats.vault.operations.
Render operation queue health on admin and vault dashboards.
```

### PR 8C — ConnectionStats

```text
Add websocket/session lifecycle stats.
Expose stats.system.connections.
Render connection/session health on the admin dashboard.
```

### PR 8D — StorageBackendStats

```text
Add local/S3 storage backend health stats.
Expose stats.system.storage and stats.vault.storage.
Render backend health on admin and vault dashboards.
```

### PR 8E — RetentionPressure

```text
Add trash/audit/sync/cache retention pressure stats.
Expose stats.system.retention and stats.vault.retention.
Render cleanup pressure on admin and vault dashboards.
```

### PR 9 — Snapshot service

```text
Add stats_snapshot table.
Add background snapshot collector.
Render basic trends.
```

## The most important design constraint

Do not make the dashboard “pretty numbers.” Make it a triage surface.

Every card should answer one of these:

```text
Healthy or not?
Why?
Since when?
What should I check next?
```

That is how Vaulthalla escapes Nextcloud-alternative energy and becomes “storage infrastructure with a command center.” This is absolutely the next needle mover.
