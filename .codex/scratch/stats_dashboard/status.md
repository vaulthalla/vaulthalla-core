# Stats Dashboard Status

## Current Phase

- Phase 8 - Vault Security / Integrity
- Status: implemented and validated; checkpoint commit pending.

## Completed Phases

- Phase 1: SystemHealth, `stats.system.health`, admin dashboard card.
- Phase 2: ThreadPool snapshots, `stats.system.threadpools`, admin dashboard card.
- Phase 3: FuseStats, `stats.system.fuse`, admin dashboard card.
- Phase 4: VaultSyncHealth, `stats.vault.sync`, vault dashboard card.
- Phase 5: VaultActivity, `stats.vault.activity`, vault dashboard card.
- Phase 6: VaultShareStats, `stats.vault.shares`, vault dashboard card.
- Phase 7: DbStats, `stats.system.db`, admin dashboard card.

## Latest Phase Summary

Phase 8 adds `stats.vault.security` backed by existing security and share metadata:

- `vault_keys`, `vault_keys_trashed`, and `files.encrypted_with_key_version` for key coverage.
- `share_access_event` denied/rate-limited rows for recent unauthorized access pressure.
- vault/share role assignment and override timestamps for last policy changes.
- explicit `integrity_check_status: not_available` until a real verifier exists.

The vault dashboard now renders Security / Integrity after Share Observatory.

## Checkpoint

- Commit SHA: pending
- Push target: `origin/stats-dashboards`.
