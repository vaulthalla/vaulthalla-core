# Release Tooling Map (`tools/release/`)

This is the current architecture and status map for release/version/changelog automation in this repository.

## Current Scope

`tools/release` currently does two real jobs:

1. Version-state management (authoritative `VERSION` sync and drift validation).
2. Changelog intelligence primitives (collect/categorize/score/snippet/render of release context).

It does **not** yet provide a full CLI changelog draft pipeline or OpenAI integration.

## Package Layout (Current Code)

Top level:

- `tools/release/__main__.py`: `python -m tools.release` entrypoint.
- `tools/release/cli.py`: CLI parser + `check/sync/set-version/bump`.

Version system:

- `tools/release/version/models.py`: `Version` semver model (`X.Y.Z`, bump helpers).
- `tools/release/version/adapters/`:
  - `version_file.py`: read/write `VERSION`.
  - `meson.py`: read/write version in `core/meson.build`.
  - `package_json.py`: read/write version in `web/package.json`.
  - `debian.py`: read/write Debian changelog header (`X.Y.Z-N`).
- `tools/release/version/validate.py`:
  - path model (`ReleasePaths`)
  - read-state model (`VersionReadResult`)
  - issue/state models (`ValidationIssue`, `ReleaseState`)
  - drift/structural validation and requirement helpers.
- `tools/release/version/sync.py`:
  - `resolve_debian_revision(...)`
  - `apply_version_update(...)` write orchestration.

Changelog system:

- `tools/release/changelog/models.py`: immutable data shapes (`CommitInfo`, `FileChange`, `DiffSnippet`, `CategoryContext`, `ReleaseContext`).
- `tools/release/changelog/git_collect.py`: raw git collection only (`get_latest_tag`, `get_commits_since_tag`, `get_release_file_stats`, `get_file_patch`, etc.).
- `tools/release/changelog/categorize.py`: path normalization, category assignment, subscopes, flags/themes.
- `tools/release/changelog/scoring.py`: file and hunk scoring heuristics.
- `tools/release/changelog/snippets.py`: patch hunk splitting + top snippet selection.
- `tools/release/changelog/context_builder.py`: orchestration into `ReleaseContext` (commit/file stats + snippets).
- `tools/release/changelog/render_raw.py`: raw markdown/debug/json rendering.

Debug surface:

- `tools/release/debug/release_context.py`: local harness to inspect release context and dump JSON.

## Command Surface (Current)

```bash
python3 -m tools.release check
python3 -m tools.release sync [--dry-run] [--debian-revision N]
python3 -m tools.release set-version X.Y.Z [--dry-run] [--debian-revision N]
python3 -m tools.release bump {major|minor|patch} [--dry-run] [--debian-revision N]
```

Debug-only (not wired into main CLI):

```bash
python3 -m tools.release.debug.release_context [--repo-root PATH] [--json]
```

## Canonical Version Authority + Managed Files

Canonical version source: top-level `VERSION`.

Managed files validated/synced against `VERSION`:

- `core/meson.build`
- `web/package.json`
- `debian/changelog` (upstream must match; Debian revision is preserved/overridden by sync args)

`check` behavior from `version/validate.py`:

- detects structural issues (missing files, read failures, invalid parse)
- detects drift mismatches
- emits actionable sync guidance (`python -m tools.release sync`)

## Current Changelog Collection Pipeline

Implemented flow in `changelog/context_builder.py`:

1. Resolve `previous_tag` via `git describe --tags --abbrev=0` when not provided.
2. Collect commits in range (`previous_tag..HEAD` or `HEAD`) via `git log`.
3. Gather per-file aggregate stats via `git diff --numstat`.
4. Build category contexts:
   - path -> category (`categorize.py`)
   - subscopes + flags + themes
   - file score ranking (`scoring.py`)
5. Extract snippets:
   - fetch patch per candidate file
   - split hunks + score hunks
   - keep top hunks per file/category (`snippets.py`)
6. Return `ReleaseContext` with final categories including snippets.

Raw renderers in `render_raw.py` provide:

- simple markdown-ish changelog block (`render_release_changelog`)
- human debug text (`render_debug_context`)
- JSON payload (`render_debug_json`)

## Current Debug Flow

`python -m tools.release.debug.release_context --json`:

1. Reads canonical version from `VERSION`.
2. Calls `build_release_context(version=..., repo_root=...)`.
3. Prints human debug sections by category.
4. Optionally emits full JSON (`dataclasses.asdict`) payload.

This is currently the best introspection path for tuning collector quality.

## Known Gaps / Limitations (Current)

1. Main CLI has no `changelog` command yet.
2. No deterministic changelog payload contract formalized for AI consumption (current JSON is debug-oriented).
3. Raw renderer is minimal:
   - no strong section templates
   - no deterministic ordering guarantees beyond category traversal
   - limited narrative synthesis.
4. Categorization/scoring/snippet logic is heuristic and not yet benchmarked by quality fixtures.
5. Snippet reasoning is shallow (`build_snippet_reason` ignores hunk content beyond score).
6. No stored golden tests for changelog context stability across representative commit sets.
7. No OpenAI client integration in `tools/release` yet (by design at current stage).

## Intended Progression Toward AI-Assisted Changelog Generation

Planned progression should remain staged:

1. Improve collector quality and deterministic context assembly.
2. Improve raw renderer quality and deterministic section structure.
3. Add CLI workflow for draft changelog generation from existing pipeline.
4. Add deterministic AI payload builder (separate from display renderer).
5. Integrate local OpenAI draft generation behind explicit command/flag.
6. Add optional refinement passes only after baseline deterministic quality is stable.

Roadmap tracking file:

- `.codex/scratch/release-changelog-roadmap.md`
