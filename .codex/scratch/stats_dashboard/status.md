# Stats Dashboard Status

## Current Phase

- Phase 5 - Vault Activity and Mutation Stats
- Status: implemented locally, validation passed, pending checkpoint commit/push.

## Completed Phases

- Phase 1: SystemHealth, `stats.system.health`, admin dashboard card.
- Phase 2: ThreadPool snapshots, `stats.system.threadpools`, admin dashboard card.
- Phase 3: FuseStats, `stats.system.fuse`, admin dashboard card.
- Phase 4: VaultSyncHealth, `stats.vault.sync`, vault dashboard card.

## Latest Phase Summary

Phase 5 adds `stats.vault.activity` backed by real mutation state:

- `file_activity` for file creates/modifies.
- `files_trashed` for delete counts and removed bytes.
- `operations` for move/rename/copy status and recent operation activity.

The vault dashboard now renders Activity below Sync Health.

## Checkpoint

- Commit SHA: pending checkpoint commit.
- Push target: `origin/stats-dashboards`.
