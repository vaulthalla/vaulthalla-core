# Stats Dashboard Buildout Context

This file mirrors the ignored scratch roadmap/status notes for durable checkpoint context.

## Current Branch

- Branch: `stats-dashboards`
- Upstream: `origin/stats-dashboards`

## Completed Checkpoints

### Phase 1 - System Health Center MVP

- Commit: `587d5b16`
- Backend: shared `SystemHealth` model, `vh status` reuse, `stats.system.health`.
- Frontend: `SystemHealth` model/store/component on admin dashboard.

### Phase 2 - Thread Pool Pressure

- Commit: `0e5400fe`
- Backend: `ThreadPool`/`ThreadPoolManager` snapshots, `stats.system.threadpools`.
- Frontend: `ThreadPoolStats` model/store/component on admin dashboard.

### Phase 3 / Phase 4 - FUSE Telemetry and Vault Sync Health

- Commit: `3d3432dd`
- Backend: `FuseStats`, FUSE operation instrumentation, `stats.system.fuse`, `VaultSyncHealth`, sync rollup queries, `stats.vault.sync`.
- Frontend: `FuseStats` admin card, `VaultSyncHealth` vault dashboard card.
- Environment hardening in same checkpoint: dev/test PostgreSQL lifecycle fixes and direct integration test isolation.

## Phase 5 - Vault Activity and Mutation Stats

- Status: committed and pushed.
- Commit: `e236717c`
- Push target: `origin/stats-dashboards`
- Websocket command: `stats.vault.activity`.
- Backend surfaces:
  - `stats/model/VaultActivity`
  - `db/query/vault/Activity`
  - prepared vault activity rollup queries
  - file activity insert seam for file create/modify
  - operation status recording seam for web move/rename/copy
- Frontend surfaces:
  - `web/src/models/stats/vaultActivity.ts`
  - `web/src/components/vault/VaultStatsDashboard/VaultActivity/Component.tsx`
  - `statsStore.getVaultActivity`
  - `WebSocketCommandMap['stats.vault.activity']`
- Dashboard integration: vault dashboard renders Activity after Sync Health.
- Architectural decisions:
  - Activity metrics come from `file_activity`, `files_trashed`, and `operations`.
  - Delete counts/bytes come from `files_trashed` because `file_activity` rows cascade when the file row is deleted.
  - Move/rename/copy operation rows are written as short-lived `in_progress` records and marked `success`/`error` after the filesystem call.
  - FUSE create/update flows contribute through `File::upsertFile`/`File::updateFile`; FUSE move/rename is not double-recorded in `operations` in this phase.
  - `sync::model::Operation::Status::Failed` now serializes to `error` to match the schema check constraint; parser still accepts legacy `failed`.
- Validation:
  - `git diff --check`: passed
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after fixing websocket Router unity namespace ambiguity
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Deferred TODOs:
  - Add richer FUSE rename/move operation attribution only if it can avoid duplicate web operation rows.
  - Add explicit activity tests around mutation recording.
  - Operation status history for sync-processed pending operations remains existing behavior.

## Phase 6 - Share Observatory Lite

- Status: committed and pushed.
- Commit: `36f35f23`
- Push target: `origin/stats-dashboards`
- Websocket command: `stats.vault.shares`.
- Backend surfaces:
  - `stats/model/VaultShareStats`
  - `db/query/share/Stats`
  - prepared share rollup queries
  - vault View/ViewStats permission check through the existing stats handler pattern
- Frontend surfaces:
  - `web/src/models/stats/vaultShareStats.ts`
  - `web/src/components/vault/VaultStatsDashboard/ShareStats/Component.tsx`
  - `statsStore.getVaultShareStats`
  - `WebSocketCommandMap['stats.vault.shares']`
- Dashboard integration: vault dashboard renders Share Observatory after Activity.
- Architectural decisions:
  - Link posture metrics come from `share_link`.
  - Completed upload count comes from `share_upload` joined to `share_link`.
  - Download/denied/rate-limited/failed attempts and recent events come from `share_access_event`.
  - Stats payload omits IP addresses and user agents to avoid exposing unnecessary sensitive detail in this rollup.
  - Reconfigure exposed a pre-existing unity-build ambiguity in shell vault commands; unqualified `Vault` references were made explicit.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Deferred TODOs:
  - Add dedicated share stats tests with seeded links/uploads/audit events.
  - Add optional global admin share activity card in a later dashboard layout pass if desired.

## Phase 7 - DB Health

- Status: committed and pushed.
- Commit: `b681356c`
- Push target: `origin/stats-dashboards`
- Websocket command: `stats.system.db`.
- Backend surfaces:
  - `stats/model/DbStats`
  - `db/query/stats/DbStats`
  - prepared DB health queries for size, connection state, cache hit ratio, deadlocks, temp bytes, oldest transaction age, largest tables, and extension detection
- Frontend surfaces:
  - `web/src/models/stats/dbStats.ts`
  - `web/src/components/stats/DbHealth.tsx`
  - `statsStore` DB stats wrapper, refresh, and polling helpers
  - `WebSocketCommandMap['stats.system.db']`
- Dashboard integration: admin dashboard renders Database Health after FUSE Operations and before cache cards.
- Architectural decisions:
  - DB health works on stock PostgreSQL and does not require `pg_stat_statements`.
  - `pg_stat_statements` is detected through `pg_extension`; slow-query count is `null`/not enabled when the extension is unavailable.
  - If DB stat collection throws, the stats payload returns `connected=false`, `status=critical`, and an error string instead of inventing healthy data.
  - Creating `vh::db::query::stats` exposed older relative `stats::model` lookup in DB query headers, so those references were made fully qualified.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after qualifying shadowed stats model namespaces
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Deferred TODOs:
  - Add focused DB stats query tests.
  - Add richer bloat/index-health estimates only if backed by safe PostgreSQL metadata.

## Phase 8 - Vault Security / Integrity

- Status: committed and pushed.
- Commit: `f5ff4219`
- Push target: `origin/stats-dashboards`
- Websocket command: `stats.vault.security`.
- Backend surfaces:
  - `stats/model/VaultSecurity`
  - `db/query/vault/Security`
  - prepared vault security rollup query for key version posture, file key coverage, denied share access, and policy-change timestamps
  - vault View/ViewStats permission check through the existing per-vault stats handler pattern
- Frontend surfaces:
  - `web/src/models/stats/vaultSecurity.ts`
  - `web/src/components/vault/VaultStatsDashboard/VaultSecurity/Component.tsx`
  - `statsStore.getVaultSecurity`
  - `WebSocketCommandMap['stats.vault.security']`
- Dashboard integration: vault dashboard renders Security / Integrity after Share Observatory.
- Architectural decisions:
  - Encryption posture is based on `vault_keys`, `vault_keys_trashed`, and `files.encrypted_with_key_version`.
  - Denied/rate-limited access signals come from `share_access_event` without exposing full IP or user-agent values.
  - Last permission and share policy change timestamps come from vault role assignment/override and share role/link metadata.
  - Integrity verification is reported honestly as `not_available`; no checksum pass/fail claim is made without a verifier.
  - Unity-build helper names are security-specific to avoid collisions with other vault query helper functions.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after fixing security query unity helper-name collisions
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2 after rerunning sequentially behind `make test`; an earlier concurrent run raced the test DB setup and hit the known password-auth initialization failure
- Known failures: none currently.
- Push result: succeeded, with GitHub remote moved warning.
- Deferred TODOs:
  - Add seeded DB tests for vault security rollups.
  - Add a real integrity verifier before reporting checksum health as passed/failed.

## Phase 8A - Recovery Readiness

- Status: committed and pushed.
- Commit: `e0d97240`
- Push target: `origin/stats-dashboards`
- Websocket command: `stats.vault.recovery`.
- Backend surfaces:
  - `stats/model/VaultRecovery`
  - `db/query/vault/Recovery`
  - prepared backup policy query reading `backup_policy`
  - vault View/ViewStats permission check through the existing per-vault stats handler pattern
- Frontend surfaces:
  - `web/src/models/stats/vaultRecovery.ts`
  - `web/src/components/vault/VaultStatsDashboard/RecoveryReadiness/Component.tsx`
  - `statsStore.getVaultRecovery`
  - `WebSocketCommandMap['stats.vault.recovery']`
- Dashboard integration: vault dashboard renders Recovery Readiness immediately after Sync Health and before Activity.
- Architectural decisions:
  - Readiness is based only on `backup_policy` state; the command does not trigger backup work.
  - Missing policy returns `unknown`, disabled policy returns `disabled`, stale success windows return `stale`, and error state/unresolved latest error returns `failing`.
  - `backup_stale` and missed backup estimates are nullable/unknown when there is no policy row rather than pretending the vault is recoverable.
  - If multiple `backup_policy` rows exist for a vault, the query uses the latest row by id because the schema does not enforce uniqueness.
  - Reconfigure exposed another existing shell unity-build ambiguity; `vault/create.cpp` RBAC references were fully qualified.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after qualifying shell vault create RBAC references
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Push result: succeeded, with GitHub remote moved warning.
- Deferred TODOs:
  - Add seeded DB tests for recovery readiness status rules.
  - Add backup verification signals only when a real backup verification process exists.

## Phase 8B - Operation Queue Health

- Status: committed and pushed.
- Commit: `f6ea80df`
- Push target: `origin/stats-dashboards`
- Websocket commands: `stats.system.operations`, `stats.vault.operations`.
- Backend surfaces:
  - `stats/model/OperationStats`
  - `db/query/stats/OperationStats`
  - prepared operation/share-upload rollup queries for system and vault scopes
  - admin-only system command and View/ViewStats vault-scoped command
- Frontend surfaces:
  - `web/src/models/stats/operationStats.ts`
  - `web/src/components/stats/OperationQueueStats.tsx`
  - `web/src/components/vault/VaultStatsDashboard/OperationQueue/Component.tsx`
  - `statsStore` system operation wrapper, polling helpers, and vault fetch helper
  - `WebSocketCommandMap['stats.system.operations']` and `['stats.vault.operations']`
- Dashboard integration:
  - Admin dashboard renders Operation Queue after FUSE Operations and before Database Health.
  - Vault dashboard renders Operation Queue after Activity and before Share Observatory.
- Architectural decisions:
  - Filesystem work comes from the existing `operations` table.
  - Share upload work comes from `share_upload`; vault filtering joins through `share_link`.
  - Stalled work uses an honest age-only threshold of 15 minutes because progress-change instrumentation is not present.
  - Recent errors combine failed/cancelled filesystem operations and failed/cancelled share uploads without mutating queue state.
  - Overall status is `critical` for stalled work, `warning` for active/recent failed work, and `healthy` when queues are clear.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Push result: succeeded, with GitHub remote moved warning.
- Deferred TODOs:
  - Add seeded operation/share-upload stats tests.
  - Add progress-staleness detection only if upload progress timestamps or instrumentation are added.

## Phase 8C - Connection Health

- Status: committed and pushed.
- Commit: `60b02079`
- Push target: `origin/stats-dashboards`
- Websocket command: `stats.system.connections`.
- Backend surfaces:
  - `stats/model/ConnectionStats`
  - timeout accessors on `ConnectionLifecycleManager`
  - `runtime::Manager::getConnectionLifecycleManager()`
  - admin-only websocket handler for live connection stats
- Frontend surfaces:
  - `web/src/models/stats/connectionStats.ts`
  - `web/src/components/stats/ConnectionStats.tsx`
  - `statsStore` connection stats wrapper, fetch, refresh, and polling helpers
  - `WebSocketCommandMap['stats.system.connections']`
- Dashboard integration: admin dashboard renders Connection Health after FUSE Operations and before Operation Queue.
- Architectural decisions:
  - Active session counts come from the existing `auth::session::Manager::getActive()` registry.
  - Session modes are split into unauthenticated, human, share pending, and ready share sessions.
  - Timeout settings come from the runtime `ConnectionLifecycleManager`.
  - Raw IP and user-agent top lists are intentionally returned as unavailable/empty to avoid leaking unnecessary sensitive detail.
  - 24h open/close/error counters are returned as null because no lifecycle event counter exists yet.
  - Reconfigure exposed a unit-test include dependency; `test_share_queries.cpp` now includes `share/Principal.hpp` explicitly.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after adding explicit test include
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Push result: succeeded, with GitHub remote moved warning.
- Deferred TODOs:
  - Add lightweight lifecycle counters if 24h opened/closed/swept/error metrics become necessary.
  - Add redacted/top-limited user-agent/IP summaries only with an explicit privacy decision.

## Phase 8D - Storage Backend Health

- Status: committed and pushed.
- Commit: `b2d1dcb0`
- Push target: `origin/stats-dashboards`
- Websocket commands: `stats.system.storage`, `stats.vault.storage`.
- Backend surfaces:
  - `stats/model/StorageBackendStats`
  - admin-only system storage stats command
  - View/ViewStats vault-scoped storage stats command
  - `vault::model::Vault` now carries the schema-backed `allow_fs_write` field
- Frontend surfaces:
  - `web/src/models/stats/storageBackendStats.ts`
  - `web/src/components/stats/StorageBackendStats.tsx`
  - `web/src/components/vault/VaultStatsDashboard/StorageBackend/Component.tsx`
  - `statsStore` system storage wrapper, polling helpers, and vault fetch helper
  - `WebSocketCommandMap['stats.system.storage']` and `['stats.vault.storage']`
- Dashboard integration:
  - Admin dashboard renders Storage Backend after Operation Queue and before Database Health.
  - Vault dashboard renders Storage Backend after Capacity and before Sync Health.
- Architectural decisions:
  - Storage backend health is a read-only live snapshot from `storage::Manager` engines.
  - Per-vault status includes type, active state, `allow_fs_write`, quota, vault/cache/free-space signals, backend status, and S3 bucket/encryption config.
  - Provider operation/error/latency fields are intentionally null until provider operation boundaries are instrumented.
  - Backend status is `error` for missing/exceptional engines, `degraded` for inactive vaults, low free-space signals, or incomplete S3 config, and `healthy` otherwise.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after adding `allow_fs_write` to `vault::model::Vault`
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Push result: succeeded, with GitHub remote moved warning.
- Deferred TODOs:
  - Add provider operation counters and latency/error instrumentation when storage engine boundaries are instrumented.
  - Add focused backend tests for storage backend status classification.

## Phase 8E - Retention and Cleanup Pressure

- Status: committed locally; push pending.
- Commit: `c4338666`
- Push target: `origin/stats-dashboards`
- Websocket commands: `stats.system.retention`, `stats.vault.retention`.
- Backend surfaces:
  - `stats/model/RetentionStats`
  - `db/query/stats/RetentionStats`
  - prepared retention rollup queries for system and vault scopes
  - admin-only system command and View/ViewStats vault-scoped command
- Frontend surfaces:
  - `web/src/models/stats/retentionStats.ts`
  - `web/src/components/stats/RetentionPressure.tsx`
  - `web/src/components/vault/VaultStatsDashboard/RetentionPressure/Component.tsx`
  - `statsStore` system retention wrapper, polling helpers, and vault fetch helper
  - `WebSocketCommandMap['stats.system.retention']` and `['stats.vault.retention']`
- Dashboard integration:
  - Admin dashboard renders Retention / Cleanup after Database Health.
  - Vault dashboard renders Retention / Cleanup after Security / Integrity.
- Architectural decisions:
  - Retention stats are read-only and use existing metadata tables only.
  - System rollups query `files_trashed`, `sync_event`, `audit_log`, `share_access_event`, and `cache_index`.
  - Vault rollups scope audit logs by `audit_log.target_file_id -> fs_entry.vault_id`.
  - Config supplies trash retention, sync event retention/max entries, audit log retention, cache expiry days, and cache max size.
  - Cache eviction candidates are expired entries unless indexed cache bytes exceed the configured max, in which case all indexed entries are candidates.
  - Cleanup status is `healthy`, `warning`, `overdue`, or `unknown`.
- Validation:
  - `git diff --check`: passed
  - `git -c core.filemode=true diff --summary`: passed, no filemode-only noise
  - `meson setup --reconfigure build`: passed
  - `meson compile -C build`: passed after making helpers unity-build safe and including `stats/model/RetentionStats.hpp` in the websocket handler
  - `make test`: passed
  - `pnpm --dir web typecheck`: passed
  - `pnpm --dir web lint`: passed
  - `pnpm --dir web test`: passed
  - `meson test -C build`: passed, 2/2
- Known failures: none currently.
- Push result: pending.
- Deferred TODOs:
  - Add seeded DB tests for system/vault retention rollups.
  - Add share-access-event retention configuration if share event cleanup needs an independent policy.
