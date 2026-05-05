# Stats Dashboard Status

## Current Phase

- Phase 8C - Connection Health
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

## Latest Phase Summary

Phase 8C adds websocket/session health:

- `stats.system.connections` for admin-only live session telemetry.
- active session counts from `auth::session::Manager::getActive()`.
- mode split for human, share, share-pending, and unauthenticated sessions.
- lifecycle timeout settings from `ConnectionLifecycleManager`.
- sensitive IP/user-agent top lists intentionally unavailable for now.

The admin dashboard now renders Connection Health after FUSE and before Operation Queue.

## Checkpoint

- Commit SHA: pending
- Push target: `origin/stats-dashboards`.
