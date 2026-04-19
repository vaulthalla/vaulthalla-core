# Release Tooling Map (`tools/release/`)

## Purpose

`tools/release` keeps versioned artifacts aligned and builds release/changelog context from git history.

## Command Surface

```bash
python3 -m tools.release check
python3 -m tools.release sync [--dry-run] [--debian-revision N]
python3 -m tools.release set-version X.Y.Z [--dry-run] [--debian-revision N]
python3 -m tools.release bump {major|minor|patch} [--dry-run] [--debian-revision N]
```

## Version Authority + Managed Targets

Canonical source: top-level `VERSION`.

Managed sync targets:

- `core/meson.build`
- `web/package.json`
- `debian/changelog`

`check` fails when any managed file drifts from `VERSION`.

## Internal Package Layout

- `version/`:
  - semver model (`Version`)
  - adapters for each managed file format
  - release-state validation
  - update application orchestration
- `changelog/`:
  - git collection
  - categorization/scoring/snippet extraction
  - release context assembly + rendering helpers
- `debug/`:
  - release context debug harness (`python3 -m tools.release.debug.release_context`)

## Typical Flows

When repo drift exists:

1. `python3 -m tools.release check`
2. `python3 -m tools.release sync --dry-run`
3. `python3 -m tools.release sync`

When cutting a new version:

1. Ensure clean synced state (`check` passes)
2. `set-version` or `bump`
3. update changelog entries and packaging/release metadata as needed
