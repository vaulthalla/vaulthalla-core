# Stats Dashboard Status

## Current Phase

- Phase 8B - Operation Queue Health
- Status: committed and pushed.

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

## Latest Phase Summary

Phase 8B adds operation/share-upload queue health:

- `stats.system.operations` for global admin queue pressure.
- `stats.vault.operations` for vault-scoped queue pressure.
- filesystem operation rollups from `operations`.
- share upload active/stalled/failed rollups from `share_upload`.
- recent queue error rows from failed/cancelled operations and uploads.

The admin dashboard now renders Operation Queue after FUSE. The vault dashboard renders Operation Queue after Activity.

## Checkpoint

- Commit SHA: `f6ea80df`
- Push target: `origin/stats-dashboards`.
