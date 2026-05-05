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
