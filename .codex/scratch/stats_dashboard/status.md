# Stats Dashboard Status

## Current Phase

- Phase 8D - Storage Backend Health
- Status: validated locally; checkpoint commit pending.

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

## Latest Phase Summary

Phase 8D adds storage backend health:

- `stats.system.storage` for admin-only local/S3 vault backend rollups.
- `stats.vault.storage` for View/ViewStats-authorized per-vault backend status.
- runtime snapshots from active `storage::Manager` engines.
- local/S3 counts, active/inactive counts, vault size/cache/free-space signals, S3 bucket and upstream encryption configuration.
- provider operation/latency/error counters are explicitly unavailable until provider-level instrumentation exists.

The admin dashboard now renders Storage Backend after Operation Queue and before Database Health.
The vault dashboard now renders Storage Backend after Capacity and before Sync Health.

## Checkpoint

- Commit SHA: pending
- Push target: `origin/stats-dashboards`.
