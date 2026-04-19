# Release Changelog Roadmap (Pipeline Completion)

Scope: finish changelog automation quality and workflow on top of the **existing** `tools/release` architecture before full API integration.

Status baseline:

- Version pipeline is production-usable (`check/sync/set-version/bump`).
- Changelog primitives exist (`git_collect`, `categorize`, `scoring`, `snippets`, `context_builder`, `render_raw`).
- Debug harness exists (`python -m tools.release.debug.release_context`).
- Main CLI has no changelog draft flow yet.

---

## Phase 0: Baseline + Guardrails (Before Any New Feature Work)

Goal:

- lock down a reproducible baseline for collector output quality and drift.

Concrete tasks:

1. Define 3-5 representative release ranges (small/medium/large diff spans).
2. Capture baseline `ReleaseContext` JSON snapshots from debug harness for each range.
3. Record current weak spots per snapshot:
   - wrong category assignment
   - noisy files promoted
   - missing important snippets
   - weak reason text.

Expected outputs:

- baseline fixture directory (JSON snapshots + short notes) under `tools/release` test/docs area.
- explicit quality checklist for ranking/categorization/snippets.

Verification:

- rerun debug command against each range and confirm snapshot reproducibility with same git state.

Modules expected to change:

- none required for setup; primarily test/docs artifacts.

Must remain unchanged unless necessary:

- `version/*` code path and existing `check/sync/set-version/bump` behavior.

---

## Phase 1: Collector Quality Improvements (Pre-OpenAI Required)

Goal:

- improve relevance/precision of categorized files and selected snippets.

Concrete tasks:

1. Refine path categorization rules in `changelog/categorize.py`:
   - tighten deploy/bin edge cases
   - reduce false meta/default fallthrough where possible.
2. Tune file scoring in `changelog/scoring.py`:
   - adjust noise penalties and category-specific boosts based on Phase 0 fixtures.
3. Improve snippet extraction in `changelog/snippets.py`:
   - better candidate file caps per category where warranted
   - improve `build_snippet_reason(...)` with hunk-aware signals.
4. Add collector-focused tests/fixtures for:
   - category assignment
   - file ranking ordering
   - snippet selection determinism.

Expected outputs:

- improved `ReleaseContext` fidelity (files/snippets closer to human relevance).
- deterministic behavior documented by fixtures/tests.

Verification:

- compare before/after fixture outputs and confirm improved ranking/snippet quality.
- no regressions to git collection API functions.

Modules expected to change:

- `tools/release/changelog/categorize.py`
- `tools/release/changelog/scoring.py`
- `tools/release/changelog/snippets.py`
- tests/fixtures alongside changelog modules.

Must remain unchanged unless necessary:

- `tools/release/changelog/git_collect.py` raw git boundary.
- `tools/release/version/*`.

---

## Phase 2: Raw Renderer Improvements (Pre-OpenAI Required)

Goal:

- make deterministic, high-signal non-AI raw changelog output usable as a first draft.

Concrete tasks:

1. Enhance `changelog/render_raw.py` structure:
   - stable section ordering
   - consistent top-N policies by category/files/snippets
   - clear headings and summary lines.
2. Add explicit rendering contract:
   - required sections
   - max lengths
   - fallback behavior on empty categories/snippets.
3. Keep debug render and raw release render separated but consistent in data semantics.

Expected outputs:

- deterministic raw changelog format from existing `ReleaseContext`.
- easier diffability and repeatability for CI/local checks.

Verification:

- golden output tests for representative fixture contexts.
- byte-for-byte stability with unchanged input context.

Modules expected to change:

- `tools/release/changelog/render_raw.py`
- render tests/fixtures.

Must remain unchanged unless necessary:

- collection internals in `git_collect.py`.
- CLI version commands.

---

## Phase 3: CLI Changelog Draft Workflow (Pre-OpenAI Required)

Goal:

- expose changelog drafting from the main `python -m tools.release` CLI.

Concrete tasks:

1. Add new CLI subcommand(s) in `tools/release/cli.py` (example shape):
   - `changelog draft`
   - optional args: `--since-tag`, `--format raw|json`, `--output`.
2. Wire command to:
   - `build_release_context(...)`
   - `render_release_changelog(...)` / JSON output path.
3. Add dry-run/no-write semantics by default (output only).

Expected outputs:

- one-command local draft generation without using debug module directly.

Verification:

- command works for tag and no-tag repos.
- output matches `render_raw` contract from Phase 2.

Modules expected to change:

- `tools/release/cli.py`
- `tools/release/changelog/__init__.py` (if export updates needed)
- potentially `tools/release/__main__.py` only if entry wiring changes.

Must remain unchanged unless necessary:

- existing behavior of `check/sync/set-version/bump`.
- `version/*` internals.

---

## Phase 4: Deterministic AI Payload Builder (Pre-OpenAI Required)

Goal:

- produce a strict, model-ready payload schema separate from human render output.

Concrete tasks:

1. Introduce a new changelog payload module (e.g., `changelog/payload.py`):
   - deterministic field ordering
   - bounded list sizes and truncation rules
   - explicit schema version key.
2. Build payload from `ReleaseContext` (not from markdown text).
3. Include stable metadata:
   - version, previous_tag, head_sha, commit_count
   - category summaries/files/snippets
   - normalization notes/limits applied.
4. Add JSON schema tests/golden snapshots.

Expected outputs:

- deterministic AI input object that is safe for local/offline inspection.

Verification:

- schema validation + snapshot tests.
- same input context yields byte-stable serialized JSON.

Modules expected to change:

- new `tools/release/changelog/payload.py` (or equivalent)
- `tools/release/changelog/__init__.py` exports
- tests for schema/snapshots.

Must remain unchanged unless necessary:

- `render_raw.py` human-facing format semantics.
- git collection boundaries.

---

## Phase 5: Local OpenAI Draft Generation (First Integration Step)

Goal:

- add optional local command that takes deterministic payload and returns draft text.

Concrete tasks:

1. Add integration module isolated from collector logic (e.g., `tools/release/changelog/ai_draft.py`).
2. Add CLI command (or `--ai` mode) that:
   - builds deterministic payload
   - sends to model
   - returns draft markdown/text.
3. Add strict fallback modes:
   - missing API key
   - model timeout/error
   - deterministic raw output fallback.

Expected outputs:

- local AI-assisted draft command with clear failure boundaries.

Verification:

- manual integration test with known payload fixture.
- graceful fallback when AI unavailable.

Modules expected to change:

- new AI module(s)
- `tools/release/cli.py` changelog command wiring
- docs for env/config usage.

Must remain unchanged unless necessary:

- collector/scoring heuristics from earlier phases unless bugs are discovered.

---

## Phase 6: Optional Refinement Passes (Post-Integration)

Goal:

- improve draft quality/consistency after initial AI integration is stable.

Possible tasks:

1. second-pass refinement prompt against generated draft.
2. category-specific style passes (core/web/deploy/debian tone shaping).
3. policy checks (forbidden claims, missing high-impact files).

Expected outputs:

- improved readability while preserving factual grounding from deterministic payload.

Verification:

- regression suite comparing factual coverage and hallucination rate vs Phase 5 baseline.

Modules expected to change:

- AI modules/prompts only.

Should remain unchanged:

- payload schema contract
- collector and raw renderer, except for bug fixes.

---

## Dependency Chain (Strict Order)

1. Phase 0 baseline
2. Phase 1 collector quality
3. Phase 2 raw renderer
4. Phase 3 CLI draft workflow
5. Phase 4 deterministic AI payload
6. Phase 5 local OpenAI draft
7. Phase 6 optional refinements

Critical rule: **do not start Phase 5 before Phase 4 is stable and tested**.

---

## Pre-OpenAI Build List (Explicit)

Must be complete before OpenAI integration:

- collector quality tuning with fixtures (Phase 1)
- deterministic raw renderer contract (Phase 2)
- main CLI draft workflow (Phase 3)
- deterministic AI payload builder + schema tests (Phase 4)

Not required before OpenAI integration:

- stylistic refinement passes (Phase 6)

---

## Progress Tracker

- [ ] Phase 0 baseline + fixtures
- [ ] Phase 1 collector quality
- [ ] Phase 2 raw renderer contract
- [ ] Phase 3 CLI changelog draft command
- [ ] Phase 4 deterministic AI payload builder
- [ ] Phase 5 local OpenAI draft integration
- [ ] Phase 6 optional refinement passes
