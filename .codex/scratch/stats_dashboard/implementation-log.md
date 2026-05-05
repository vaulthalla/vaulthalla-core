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
