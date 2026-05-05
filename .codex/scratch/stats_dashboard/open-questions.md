# Stats Dashboard Open Questions

## Phase 5

- Should FUSE rename/move operations also write `operations` rows, or should they remain represented through file create/modify/delete activity until a duplicate-safe mutation event API exists?
- Should `operations` rows created for sync processing be marked completed after `Local::processOperations()` succeeds? This predates Phase 5 and is not changed here.
- Should `VaultActivity` eventually split human uploads from share uploads? Phase 6 will add share-specific observability.

## Phase 6

- Should public share observability get a global admin card in addition to the per-vault Share Observatory card?
- Should top share events eventually expose redacted/top-limited remote IP or user-agent summaries, or keep those details out of dashboard stats unless explicitly requested?

## Phase 7

- Should slow-query thresholds stay fixed at mean execution time >= 1s, or become configurable from admin settings?
- Should index bloat be estimated in a later phase, or deferred until historical DB snapshots exist?

## Phase 8

- Should vault security eventually expose redacted/top-limited denied access source summaries, or keep those out of dashboard telemetry unless explicitly requested?
- What process should own checksum verification so `integrity_check_status` can move from `not_available` to a real pass/fail signal?

## Phase 8A

- Should `backup_policy` be constrained to one row per vault, or is latest-row-by-id the intended policy selection rule?
- What future process should write a distinct backup verification timestamp so "verified good state" can mean more than last successful backup completion?
