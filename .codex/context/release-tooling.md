# Release Tooling Map (`tools/release/`)

This file is the current system map for release, changelog, AI drafting, and packaging/deployment automation.

## Current Scope

`tools/release` now covers four production-use areas:

1. Version authority + drift enforcement (`check/sync/set-version/bump`).
2. Deterministic changelog intelligence (git collection, categorization, scoring, snippets, context assembly, raw rendering).
3. AI-assisted changelog drafting pipeline (payload -> optional triage -> draft -> optional polish).
4. Local Debian packaging orchestration including Next standalone artifact output (`build-deb`).

## Package Layout (Ground Truth)

Top-level:

- `tools/release/__main__.py`: module entrypoint.
- `tools/release/cli.py`: command parsing + thin orchestration.

Version subsystem (`tools/release/version/`):

- `models.py`: semantic `Version`.
- `adapters/`: `VERSION`, Meson, `package.json`, Debian changelog adapters.
- `validate.py`: `ReleaseState` + structural/drift checks.
- `sync.py`: file-write orchestration and Debian revision resolution.

Changelog subsystem (`tools/release/changelog/`):

- `git_collect.py`: git IO only.
- `categorize.py`, `scoring.py`, `snippets.py`: deterministic release intelligence primitives.
- `context_builder.py`: `ReleaseContext` orchestration.
- `render_raw.py`: deterministic human raw draft + debug renderers.
- `payload.py`: deterministic bounded AI payload projection (`vaulthalla.release.ai_payload.v1`).
- `ai/`:
  - `contracts/`: typed schemas/parsers for draft, triage, polish.
  - `prompts/`: stage prompt builders.
  - `providers/`: hosted OpenAI + OpenAI-compatible transports + preflight/model discovery.
  - `stages/`: draft, triage, polish orchestration.
  - `render/markdown.py`: final local markdown rendering from typed AI results.

Debug surface:

- `tools/release/debug/release_context.py`: inspect `ReleaseContext` as text/JSON.

Packaging subsystem:

- `tools/release/packaging/debian.py`: local build orchestration and artifact collection.

## Command Surface

```bash
python3 -m tools.release check
python3 -m tools.release sync [--dry-run] [--debian-revision N]
python3 -m tools.release set-version X.Y.Z [--dry-run] [--debian-revision N]
python3 -m tools.release bump {major|minor|patch} [--dry-run] [--debian-revision N]
python3 -m tools.release build-deb [--output-dir PATH] [--dry-run]

python3 -m tools.release changelog draft [--format raw|json] [--since-tag TAG] [--output PATH]
python3 -m tools.release changelog payload [--since-tag TAG] [--output PATH]
python3 -m tools.release changelog ai-check [--provider openai|openai-compatible] [--base-url URL] [--model MODEL]
python3 -m tools.release changelog ai-draft [--since-tag TAG] [--output PATH] [--save-json PATH] [--model MODEL] [--provider openai|openai-compatible] [--base-url URL] [--use-triage] [--save-triage-json PATH] [--polish] [--save-polish-json PATH]

python3 -m tools.release.debug.release_context [--repo-root PATH] [--json]
```

## Version Authority + Invariants

Canonical source of truth:

- top-level `VERSION`

Managed files:

- `core/meson.build`
- `web/package.json`
- `debian/changelog`

Debian invariant (critical):

- `VERSION` stores upstream semver only.
- `debian/changelog` stores full Debian version (`X.Y.Z-N`).
- Validation compares upstream only: `debian.upstream == VERSION`.
- Revision is preserved by default in `sync` and `bump`, unless explicitly overridden.

Guardrails:

- `check` fails on drift/structural issues.
- `bump` and `set-version` require synced state first.
- `sync` now re-validates synced state after write completion.

## Deterministic Changelog Pipeline

Current deterministic path:

1. Build release context (`context_builder.py`):
   - resolve previous tag (`--since-tag` override supported),
   - collect commits/file stats,
   - categorize/score files,
   - attach top snippets.
2. Render raw draft (`changelog draft --format raw`) or debug JSON (`--format json`).
3. Build bounded model-ready payload (`changelog payload`).

Raw renderer contract:

- stable category ordering (`CATEGORY_ORDER`, then alphabetical),
- stable top-N evidence selections,
- explicit empty/weak evidence language.

## AI Drafting Pipeline (Current)

Pipeline shape:

- `payload`
  -> optional `triage`
  -> `draft`
  -> optional `polish`
  -> local markdown render

Key properties:

- all model outputs are required as structured JSON and validated against typed contracts,
- markdown is rendered locally from validated objects,
- no repo mutation, no Debian changelog insertion, no commit/tag writes.

Default model tier:

- `gpt-5.4-mini` (override with `--model`).

## Provider Support + Local-Compatible Mode

Supported providers:

- `openai` (hosted, requires `OPENAI_API_KEY`)
- `openai-compatible` (custom `--base-url`, no-auth supported)

Preflight:

- `changelog ai-check` performs connectivity + `/models` discovery and model verification.
- `changelog ai-draft` automatically preflights when `--provider openai-compatible` is used.

Local dogfood defaults:

- default compatible base URL: `http://localhost:8888/v1`

## Debian Packaging Flow (`build-deb`)

`python3 -m tools.release build-deb`:

1. validates synced release state,
2. validates required Debian files (`debian/changelog`, `debian/control`, `debian/rules`, `debian/source/format`),
3. runs web install/build (`pnpm install --frozen-lockfile`, `pnpm build`),
4. packages Next standalone web artifact to:
   - `release/<package>-web_<version>_next-standalone.tar.gz`,
5. runs `dpkg-buildpackage -us -uc -b`,
6. copies Debian artifacts (`.deb`, `.changes`, `.buildinfo`, etc.) into output dir,
7. writes `release/build-deb.log` with web + Debian command output.

Debian packaging contract (current):

- Meson source directory for Debian build is `core/` (`debian/rules` uses `--sourcedirectory=core`).
- Non-Meson payloads (config/systemd/docs/udev/tmpfiles/symlink shims) are explicitly staged in `debian/rules` so `dh_install` finds expected paths.
- Standalone web archive creation preserves symlinks to handle pnpm link structures in CI.

## CI/CD Release Workflow (Phase 8 State)

Workflow:

- `.github/workflows/release.yml`
- triggers:
  - `workflow_dispatch`
  - tag push (`v*`)
- release job runs with `environment: Production` (mandatory deployment tracking).

CI sequence:

1. `python3 -m tools.release check`
2. core build + tests
3. release-tooling unit tests
4. web setup + `build_web` (includes private icon sync) + web checks
5. deterministic changelog evidence artifacts (`changelog.raw.md`, `changelog.payload.json`)
6. canonical package action (`.github/actions/package`) -> `python3 -m tools.release build-deb`
7. upload release artifacts
8. on tags: attach artifacts to GitHub Release

Canonical packaging entrypoint in CI:

- `.github/actions/package/action.yml`

## End-to-End Release Flow Snapshot

Current end-to-end spine:

1. version state validation/sync
2. release context generation
3. raw draft rendering + payload projection
4. optional AI triage/draft/polish
5. local markdown output
6. Debian + web artifact build (`build-deb`)
7. CI artifact upload + optional GitHub Release attachment
8. Production environment deployment tracking in GitHub Actions

## Sharp Edges / Learned Invariants (Today)

1. Debian version comparisons must use upstream only; full string comparison with revision is incorrect.
2. Release workflow must include web `build_web` semantics for private icon sync parity before web checks.
3. Debian Meson source root in packaging must be `core/`, not repository root.
4. Debian package staging must explicitly include non-Meson runtime payloads expected by `debian/install`.
5. Next standalone archive copy must preserve symlinks to avoid CI failures on pnpm-linked paths.
6. `environment: Production` in release workflow is intentional and required for deployment tracking.
7. `.github/actions/package` is the canonical CI packaging wrapper and should stay the single packaging entrypoint.

## Current Gaps / Deferred Work

1. AI-generated release notes are not yet first-class in CI release publishing flow.
2. External publishing channels (Nexus/APT promotion) remain deferred.
3. CI hardening around packaging edge-cases may still need iterative tuning as runner environments vary.
4. Changelog quality tuning (collector/scoring/snippet heuristics) is ongoing.
