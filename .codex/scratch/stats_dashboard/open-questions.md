# Stats Dashboard Open Questions

## Phase 5

- Should FUSE rename/move operations also write `operations` rows, or should they remain represented through file create/modify/delete activity until a duplicate-safe mutation event API exists?
- Should `operations` rows created for sync processing be marked completed after `Local::processOperations()` succeeds? This predates Phase 5 and is not changed here.
- Should `VaultActivity` eventually split human uploads from share uploads? Phase 6 will add share-specific observability.
