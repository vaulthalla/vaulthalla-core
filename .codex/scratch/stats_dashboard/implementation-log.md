# Stats Dashboard Implementation Log

## Phase 5 - Vault Activity and Mutation Stats

### Backend Files Added

- `core/include/stats/model/VaultActivity.hpp`
- `core/src/stats/model/VaultActivity.cpp`
- `core/include/db/query/vault/Activity.hpp`
- `core/src/db/query/vault/Activity.cpp`
- `core/src/db/preparedStatements/vault/activity.cpp`

### Backend Files Changed

- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/src/db/preparedStatements/fs/files.cpp`
- `core/src/db/preparedStatements/sync/operations.cpp`
- `core/src/db/query/fs/File.cpp`
- `core/include/db/query/sync/Operation.hpp`
- `core/src/db/query/sync/Operation.cpp`
- `core/src/sync/model/Operation.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `core/src/protocols/ws/handler/fs/Storage.cpp`
- `core/src/protocols/ws/Router.cpp`

### Frontend Files Added

- `web/src/models/stats/vaultActivity.ts`
- `web/src/components/vault/VaultStatsDashboard/VaultActivity/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.vault.activity`

### Dashboard Integration

- Vault dashboard order now includes:
  - Capacity
  - Sync Health
  - Activity

### Architectural Decisions

- Delete metrics use `files_trashed`, not `file_activity`, because deleting file rows cascades `file_activity`.
- Move/rename/copy stats are recorded in `operations` around web filesystem mutation commands.
- Operation failures are marked `error`, matching the database schema.
- File create/modify activity is recorded inside `File::upsertFile` and `File::updateFile` so web, share, and FUSE write paths contribute where they use those seams.
- Websocket `Router.cpp` was made namespace-explicit to fix a unity-build ambiguity exposed by adding new source files.

### Deferred TODOs

- Avoid duplicate rows before adding FUSE rename/move operation records.
- Add focused unit/integration coverage for activity recording.
- Keep share observability as Phase 6 rather than overloading the activity card.

## Phase 6 - Share Observatory Lite

### Backend Files Added

- `core/include/stats/model/VaultShareStats.hpp`
- `core/src/stats/model/VaultShareStats.cpp`
- `core/include/db/query/share/Stats.hpp`
- `core/src/db/query/share/Stats.cpp`
- `core/src/db/preparedStatements/share/stats.cpp`

### Backend Files Changed

- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `core/src/protocols/shell/commands/vault/listinfo.cpp`
- `core/src/protocols/shell/commands/vault/role.cpp`

### Frontend Files Added

- `web/src/models/stats/vaultShareStats.ts`
- `web/src/components/vault/VaultStatsDashboard/ShareStats/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.vault.shares`

### Dashboard Integration

- Vault dashboard order now includes:
  - Capacity
  - Sync Health
  - Activity
  - Share Observatory

### Architectural Decisions

- Share stats are read-only and query existing `share_link`, `share_upload`, and `share_access_event` tables.
- The payload intentionally excludes full IP/user-agent fields; this card surfaces operator posture without leaking unnecessary sensitive detail.
- Download counts use successful `share_access_event` rows whose event type starts with `share.download`.
- Upload counts use completed `share_upload` rows joined through `share_link` to the vault.
- Reconfigured unity builds exposed existing unqualified `Vault` ambiguity in shell vault commands, so the affected shell references now use `::vh::vault::model::Vault`.

### Deferred TODOs

- Add seeded DB tests for share rollup queries.
- Add an optional global share activity card later if the admin dashboard needs cross-vault share posture.

## Phase 7 - DB Health

### Backend Files Added

- `core/include/stats/model/DbStats.hpp`
- `core/src/stats/model/DbStats.cpp`
- `core/include/db/query/stats/DbStats.hpp`
- `core/src/db/query/stats/DbStats.cpp`
- `core/src/db/preparedStatements/stats/dbStats.cpp`

### Backend Files Changed

- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `core/include/db/query/share/Stats.hpp`
- `core/src/db/query/share/Stats.cpp`
- `core/include/db/query/vault/Activity.hpp`
- `core/src/db/query/vault/Activity.cpp`

### Frontend Files Added

- `web/src/models/stats/dbStats.ts`
- `web/src/components/stats/DbHealth.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/app/(app)/(admin)/dashboard/page.tsx`

### Websocket Commands Added

- `stats.system.db`

### Dashboard Integration

- Admin dashboard order now includes:
  - System Health
  - Thread Pools
  - FUSE Operations
  - Database Health
  - FS Cache
  - HTTP Cache

### Architectural Decisions

- DB health uses stock PostgreSQL catalog/stat views and does not require extensions.
- `pg_stat_statements` is detected, not assumed; slow-query count stays unavailable when the extension is not enabled.
- Failed DB collection returns a critical disconnected payload with an error instead of throwing away the entire health surface.
- The new `vh::db::query::stats` namespace required fully qualified `::vh::stats::model` references in older DB query headers to avoid shadowing.

### Deferred TODOs

- Add focused DB stats query tests.
- Add safe index bloat estimates later if the formula is backed by PostgreSQL metadata available in stock installs.

## Phase 8 - Vault Security / Integrity

### Backend Files Added

- `core/include/stats/model/VaultSecurity.hpp`
- `core/src/stats/model/VaultSecurity.cpp`
- `core/include/db/query/vault/Security.hpp`
- `core/src/db/query/vault/Security.cpp`
- `core/src/db/preparedStatements/vault/security.cpp`

### Backend Files Changed

- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`

### Frontend Files Added

- `web/src/models/stats/vaultSecurity.ts`
- `web/src/components/vault/VaultStatsDashboard/VaultSecurity/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.vault.security`

### Dashboard Integration

- Vault dashboard order now includes:
  - Capacity
  - Sync Health
  - Activity
  - Share Observatory
  - Security / Integrity

### Architectural Decisions

- Security posture is read-only and derives from existing key, file, share audit, and role policy tables.
- `encryption_status` is `unknown` when no current vault key exists, `mixed` when legacy/unknown file key versions remain, and `encrypted` only when current key coverage is complete.
- Share access pressure uses denied/failed/rate-limited `share_access_event` rows and keeps sensitive IP/user-agent data out of this payload.
- Permission-change timestamps are rollups from vault role assignments/overrides and share policy metadata.
- Integrity verifier state is exposed as `not_available`; the dashboard does not manufacture a checksum health claim.

### Deferred TODOs

- Add seeded DB coverage for vault security rollup edge cases.
- Add real checksum/integrity verification before reporting pass/fail integrity health.

## Phase 8A - Recovery Readiness

### Backend Files Added

- `core/include/stats/model/VaultRecovery.hpp`
- `core/src/stats/model/VaultRecovery.cpp`
- `core/include/db/query/vault/Recovery.hpp`
- `core/src/db/query/vault/Recovery.cpp`
- `core/src/db/preparedStatements/vault/recovery.cpp`

### Backend Files Changed

- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `core/src/protocols/shell/commands/vault/create.cpp`

### Frontend Files Added

- `web/src/models/stats/vaultRecovery.ts`
- `web/src/components/vault/VaultStatsDashboard/RecoveryReadiness/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.vault.recovery`

### Dashboard Integration

- Vault dashboard order now includes:
  - Capacity
  - Sync Health
  - Recovery Readiness
  - Activity
  - Share Observatory
  - Security / Integrity

### Architectural Decisions

- Recovery readiness is read-only telemetry from `backup_policy`; no backup job is started by the stats command.
- `recovery_readiness` is `unknown` with no policy row, `disabled` when the policy is disabled, `failing` for policy error or unresolved latest error, `stale` for missing/old success, and `ready` only with recent `last_success_at` evidence.
- Nullable `backup_stale` and missed-backup estimates keep no-policy state honest rather than presenting a false recoverability badge.
- The schema does not enforce one `backup_policy` row per vault, so the query uses the latest row by id.
- Reconfigure exposed a shell unity-build namespace ambiguity in `vault/create.cpp`; RBAC references now use fully qualified `::vh::rbac`.

### Deferred TODOs

- Add seeded DB tests for recovery status edge cases.
- Add verified-good backup checks only after a real backup verification process exists.

## Phase 8B - Operation Queue Health

### Backend Files Added

- `core/include/stats/model/OperationStats.hpp`
- `core/src/stats/model/OperationStats.cpp`
- `core/include/db/query/stats/OperationStats.hpp`
- `core/src/db/query/stats/OperationStats.cpp`
- `core/src/db/preparedStatements/stats/operationStats.cpp`

### Backend Files Changed

- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`

### Frontend Files Added

- `web/src/models/stats/operationStats.ts`
- `web/src/components/stats/OperationQueueStats.tsx`
- `web/src/components/vault/VaultStatsDashboard/OperationQueue/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/app/(app)/(admin)/dashboard/page.tsx`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.system.operations`
- `stats.vault.operations`

### Dashboard Integration

- Admin dashboard order now includes Operation Queue after FUSE Operations and before Database Health.
- Vault dashboard order now includes Operation Queue after Activity and before Share Observatory.

### Architectural Decisions

- System and vault operation stats share one `OperationStats` payload shape; `vault_id` is null for system scope.
- Filesystem queue metrics come from the `operations` table.
- Share upload queue metrics come from `share_upload`; vault scope joins through `share_link`.
- Stalled heuristics use a 15-minute age-only threshold because there is no upload progress timestamp instrumentation yet.
- Recent errors combine failed/cancelled operations and failed/cancelled uploads, limited to the latest rows.

### Deferred TODOs

- Add seeded DB tests for system/vault operation queue rollups.
- Add progress-stalled upload detection if upload progress instrumentation is introduced later.

## Phase 8C - Connection Health

### Backend Files Added

- `core/include/stats/model/ConnectionStats.hpp`
- `core/src/stats/model/ConnectionStats.cpp`

### Backend Files Changed

- `core/include/protocols/ws/ConnectionLifecycleManager.hpp`
- `core/src/protocols/ws/ConnectionLifecycleManager.cpp`
- `core/include/runtime/Manager.hpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `core/tests/unit/test_share_queries.cpp`

### Frontend Files Added

- `web/src/models/stats/connectionStats.ts`
- `web/src/components/stats/ConnectionStats.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/app/(app)/(admin)/dashboard/page.tsx`

### Websocket Commands Added

- `stats.system.connections`

### Dashboard Integration

- Admin dashboard order now includes Connection Health after FUSE Operations and before Operation Queue.

### Architectural Decisions

- Connection stats are read-only and derive from `auth::session::Manager::getActive()`.
- The payload separates human, share pending, share, and unauthenticated sessions.
- Timeout settings are read from `ConnectionLifecycleManager` through small accessors.
- IP and user-agent top lists are explicitly unavailable/empty in this phase to avoid unnecessary sensitive detail.
- 24h opened/closed/swept/error counters remain null because no lifecycle counters exist yet.
- Reconfigure exposed an incomplete-type unit-test dependency; `test_share_queries.cpp` now includes `share/Principal.hpp` directly.

### Deferred TODOs

- Add lifecycle counters for opened/closed/swept/error windows if needed.
- Add redacted/top-limited IP/user-agent summaries only after an explicit privacy choice.

## Phase 8D - Storage Backend Health

### Backend Files Added

- `core/include/stats/model/StorageBackendStats.hpp`
- `core/src/stats/model/StorageBackendStats.cpp`

### Backend Files Changed

- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `core/include/vault/model/Vault.hpp`
- `core/src/vault/model/Vault.cpp`

### Frontend Files Added

- `web/src/models/stats/storageBackendStats.ts`
- `web/src/components/stats/StorageBackendStats.tsx`
- `web/src/components/vault/VaultStatsDashboard/StorageBackend/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/app/(app)/(admin)/dashboard/page.tsx`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.system.storage`
- `stats.vault.storage`

### Dashboard Integration

- Admin dashboard order now includes Storage Backend after Operation Queue and before Database Health.
- Vault dashboard order now includes Storage Backend after Capacity and before Sync Health.

### Architectural Decisions

- Storage backend stats derive from live `storage::Manager` engines rather than new database tables.
- Per-vault snapshots include vault type, active state, `allow_fs_write`, quota, vault size, cache size, computed free-space signal, backend status, and S3 bucket/encryption config.
- `allow_fs_write` already existed in the schema but was not present in the runtime `Vault` model; the model now carries and serializes it.
- Provider operation/error/latency fields are returned as null because local/S3 provider operations are not instrumented yet.
- Backend status is `error` for missing/exceptional engines, `degraded` for inactive vaults, low free space, or incomplete S3 config, and `healthy` otherwise.

### Deferred TODOs

- Add provider operation counters and latency/error instrumentation only when storage engine boundaries are instrumented.
- Add focused backend unit tests for storage backend status classification.
