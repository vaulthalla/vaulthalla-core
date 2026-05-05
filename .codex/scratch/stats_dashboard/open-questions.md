# Stats Dashboard Open Questions

## Phase 5

- Should FUSE rename/move operations also write `operations` rows, or should they remain represented through file create/modify/delete activity until a duplicate-safe mutation event API exists?
- Should `operations` rows created for sync processing be marked completed after `Local::processOperations()` succeeds? This predates Phase 5 and is not changed here.
- Should `VaultActivity` eventually split human uploads from share uploads? Phase 6 will add share-specific observability.

## Phase 6

- Should public share observability get a global admin card in addition to the per-vault Share Observatory card?
- Should top share events eventually expose redacted/top-limited remote IP or user-agent summaries, or keep those details out of dashboard stats unless explicitly requested?
