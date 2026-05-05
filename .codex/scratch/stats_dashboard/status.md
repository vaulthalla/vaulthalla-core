# Stats Dashboard Status

## Current Phase

- Phase 9 - Historical Snapshots and Trends
- Status: implemented and validated; checkpoint commit pending.

## Completed Phases

- Phase 1: SystemHealth, `stats.system.health`, admin dashboard card.
- Phase 2: ThreadPool snapshots, `stats.system.threadpools`, admin dashboard card.
- Phase 3: FuseStats, `stats.system.fuse`, admin dashboard card.
- Phase 4: VaultSyncHealth, `stats.vault.sync`, vault dashboard card.
- Phase 5: VaultActivity, `stats.vault.activity`, vault dashboard card.
- Phase 6: VaultShareStats, `stats.vault.shares`, vault dashboard card.
- Phase 7: DbStats, `stats.system.db`, admin dashboard card.
- Phase 8: VaultSecurity, `stats.vault.security`, vault dashboard card.
- Phase 8A: VaultRecovery, `stats.vault.recovery`, vault dashboard card.
- Phase 8B: OperationStats, `stats.system.operations` and `stats.vault.operations`, admin and vault dashboard cards.
- Phase 8C: ConnectionStats, `stats.system.connections`, admin dashboard card.
- Phase 8D: StorageBackendStats, `stats.system.storage` and `stats.vault.storage`, admin and vault dashboard cards.
- Phase 8E: RetentionStats, `stats.system.retention` and `stats.vault.retention`, admin and vault dashboard cards.

## Latest Phase Summary

Phase 9 adds historical dashboard snapshots and trend cards:

- `stats_snapshot` JSONB table for persisted dashboard snapshots.
- Configurable background `StatsSnapshotService` for runtime and vault snapshot cadence/retention.
- `stats.system.trends` for admin-only system trend lines.
- `stats.vault.trends` for View/ViewStats-authorized vault trend lines.
- System trends cover DB size/cache, FUSE ops/errors, thread pool pressure, and cache hit rates.
- Vault trends cover capacity growth, sync errors/failed ops/traffic, and activity mutation/byte trends.

The admin dashboard now renders Trends after Retention / Cleanup.
The vault dashboard now renders Trends after Retention / Cleanup.

## Checkpoint

- Commit SHA: pending checkpoint commit.
- Push target: `origin/stats-dashboards`.

## Phase 9 - Historical Snapshots and Trends

### Backend Files Added

- `deploy/psql/080_stats.sql`
- `core/include/stats/model/StatsTrends.hpp`
- `core/src/stats/model/StatsTrends.cpp`
- `core/include/db/query/stats/Snapshot.hpp`
- `core/src/db/query/stats/Snapshot.cpp`
- `core/src/db/preparedStatements/stats/snapshot.cpp`
- `core/include/stats/SnapshotService.hpp`
- `core/src/stats/SnapshotService.cpp`

### Backend Files Changed

- `core/include/config/Config.hpp`
- `core/src/config/Config.cpp`
- `core/include/config/config_yaml.hpp`
- `core/include/db/DBConnection.hpp`
- `core/src/db/Connection.cpp`
- `core/include/runtime/Manager.hpp`
- `core/src/runtime/Manager.cpp`
- `core/include/protocols/ws/handler/Stats.hpp`
- `core/src/protocols/ws/handler/Stats.cpp`
- `core/src/protocols/ws/Handler.cpp`
- `deploy/config/config.yaml`
- `deploy/config/config_template.yaml.in`

### Frontend Files Added

- `web/src/models/stats/statsTrends.ts`
- `web/src/components/stats/StatsTrends.tsx`
- `web/src/components/vault/VaultStatsDashboard/Trends/Component.tsx`

### Frontend Files Changed

- `web/src/util/webSocketCommands.ts`
- `web/src/stores/statsStore.ts`
- `web/src/app/(app)/(admin)/dashboard/page.tsx`
- `web/src/components/vault/VaultStatsDashboard/Component.tsx`

### Websocket Commands Added

- `stats.system.trends`
- `stats.vault.trends`

### Dashboard Integration

- Admin dashboard order now includes Trends after Retention / Cleanup.
- Vault dashboard order now includes Trends after Retention / Cleanup.

### Architectural Decisions

- Snapshots are stored as JSONB in `stats_snapshot` with system/vault scope checks and created-at indexes.
- `StatsSnapshotService` is a runtime `AsyncService` and never touches FUSE or request hot paths.
- Runtime snapshots default to every 300 seconds and include `system.threadpools`, `system.fuse`, `system.cache`, and `system.db`.
- Vault snapshots default to every 3600 seconds and include `vault.capacity`, `vault.sync`, and `vault.activity`.
- Snapshot retention defaults to 30 days and is configurable under `stats_snapshots`.
- Trend websocket commands query only the snapshot table, not raw operational tables.
- Trend cards show 24h and 7d deltas and render a no-data state until snapshots exist.

### Deferred TODOs

- Add downsampled daily compaction if raw snapshots become too large for long retention windows.
- Add seeded DB tests for trend extraction once snapshot fixtures exist.
