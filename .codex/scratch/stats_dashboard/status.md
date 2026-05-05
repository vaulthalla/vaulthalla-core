# Stats Dashboard Status

## Current Phase

- Phase 8E - Retention and Cleanup Pressure
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
- Phase 8D: StorageBackendStats, `stats.system.storage` and `stats.vault.storage`, admin and vault dashboard cards.

## Latest Phase Summary

Phase 8E adds retention and cleanup pressure:

- `stats.system.retention` for admin-only system-wide cleanup pressure.
- `stats.vault.retention` for View/ViewStats-authorized per-vault cleanup pressure.
- rollups from `files_trashed`, `sync_event`, `audit_log`, `share_access_event`, and `cache_index`.
- config-backed retention windows for trash, sync events, audit logs, and cache expiry/size.
- no destructive cleanup is performed by stats commands.

The admin dashboard now renders Retention / Cleanup after Database Health.
The vault dashboard now renders Retention / Cleanup after Security / Integrity.

## Checkpoint

- Commit SHA: pending
- Push target: `origin/stats-dashboards`.
