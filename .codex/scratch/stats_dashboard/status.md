# Stats Dashboard Status

## Current Phase

- Phase 7 - DB Health
- Status: committed and pushed.

## Completed Phases

- Phase 1: SystemHealth, `stats.system.health`, admin dashboard card.
- Phase 2: ThreadPool snapshots, `stats.system.threadpools`, admin dashboard card.
- Phase 3: FuseStats, `stats.system.fuse`, admin dashboard card.
- Phase 4: VaultSyncHealth, `stats.vault.sync`, vault dashboard card.
- Phase 5: VaultActivity, `stats.vault.activity`, vault dashboard card.
- Phase 6: VaultShareStats, `stats.vault.shares`, vault dashboard card.

## Latest Phase Summary

Phase 7 adds `stats.system.db` backed by safe PostgreSQL metadata:

- `pg_database_size`, `pg_stat_database`, and `pg_stat_activity` for live DB health signals.
- `pg_stat_user_tables` for largest table rollups.
- `pg_extension` detection for `pg_stat_statements` without requiring the extension.

The admin dashboard now renders Database Health below FUSE Operations.

## Checkpoint

- Commit SHA: `b681356c`
- Push target: `origin/stats-dashboards`.
