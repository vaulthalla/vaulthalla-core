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

- Status: implemented and validated locally; commit pending.
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
