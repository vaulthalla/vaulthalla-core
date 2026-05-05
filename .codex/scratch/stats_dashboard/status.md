# Stats Dashboard Status

## Current Phase

- Phase 8A - Recovery Readiness
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

## Latest Phase Summary

Phase 8A adds `stats.vault.recovery` backed by existing backup policy state:

- `backup_policy.enabled/status/interval` for backup mode.
- `last_backup_at` and `last_success_at` for freshness.
- `retention`, retry count, and last error for operator context.
- nullable stale/missed-backup fields when no real policy row exists.

The vault dashboard now renders Recovery Readiness after Sync Health.

## Checkpoint

- Commit SHA: `e0d97240`
- Push target: `origin/stats-dashboards`.
