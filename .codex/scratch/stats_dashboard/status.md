# Stats Dashboard Status

## Current Phase

- Phase 6 - Share Observatory Lite
- Status: committed and pushed.

## Completed Phases

- Phase 1: SystemHealth, `stats.system.health`, admin dashboard card.
- Phase 2: ThreadPool snapshots, `stats.system.threadpools`, admin dashboard card.
- Phase 3: FuseStats, `stats.system.fuse`, admin dashboard card.
- Phase 4: VaultSyncHealth, `stats.vault.sync`, vault dashboard card.
- Phase 5: VaultActivity, `stats.vault.activity`, vault dashboard card.

## Latest Phase Summary

Phase 6 adds `stats.vault.shares` backed by real share state:

- `share_link` for active/expired/revoked link posture and public/email-validated exposure.
- `share_upload` for completed upload counts.
- `share_access_event` for downloads, denied/rate-limited/failed attempts, top access signals, and recent share events.

The vault dashboard now renders Share Observatory below Activity.

## Checkpoint

- Commit SHA: `36f35f23`
- Push target: `origin/stats-dashboards`.
