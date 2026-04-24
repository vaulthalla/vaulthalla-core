# Vaulthalla Release Tooling Classifier Pipeline Forensics

Date: 2026-04-23

Scope inspected:
- `tools/release/changelog/categorize.py`
- `tools/release/changelog/context_builder.py`
- `tools/release/changelog/git_collect.py`
- `tools/release/changelog/models.py`
- `tools/release/changelog/payload.py`
- `tools/release/changelog/release_workflow.py`
- `tools/release/changelog/render_raw.py`
- `tools/release/changelog/scoring.py`
- `tools/release/changelog/snippets.py`

Adjacent call sites/contracts inspected:
- `tools/release/cli.py` (changelog/payload/ai-draft/ai-release/ai-compare flows)
- `tools/release/changelog/ai/stages/{triage,draft,polish}.py`
- `tools/release/changelog/ai/contracts/{triage,draft}.py`
- `tools/release/changelog/ai/prompts/{triage,draft}.py`
- representative fixtures/artifacts in `tools/release/tests/changelog/fixtures/` and `.changelog_scratch/comparisons/`

---

## 1) End-to-End Pipeline Overview

### A. Deterministic evidence build (classifier stack)
1. `build_release_context()` (`context_builder.py`) starts from VERSION + git range (`previous_tag..HEAD`).
2. `git_collect.py` gathers:
   - commits (`sha/subject/body`)
   - per-commit file lists and numstat
   - aggregate file stats over range
   - full file patch text on demand
3. `categorize.py` classifies paths into fixed categories (`debian/tools/deploy/web/core/meta`), subscopes, flags, and simple path-derived themes.
4. `scoring.py` computes:
   - file scores from churn + path/flag heuristics
   - hunk scores from keyword and line-count heuristics
5. `context_builder.py` assembles per-category `CategoryContext`:
   - commits, files (sorted by score), insertions/deletions, themes
6. `snippets.py` extracts top hunks from top files via score ranking and attaches generic reasons.
7. `ReleaseContext` is finalized (categories + snippets + cross-cutting notes).

### B. Deterministic reduction + render
8. `payload.py::build_ai_payload()` projects `ReleaseContext` into capped model-facing JSON:
   - top commits/files/snippets only
   - truncation metadata
   - signal strength and generation notes
9. `render_raw.py::render_release_changelog()` projects `ReleaseContext` into deterministic markdown draft:
   - evidence tier
   - themes
   - key commits
   - top files
   - snippets metadata

### C. AI consumption/orchestration
10. `cli.py` and `release_workflow.py` use payload/raw for downstream:
   - `changelog payload` -> payload JSON artifact
   - `changelog draft` -> raw markdown artifact
   - `changelog ai-draft` and release workflow:
     - optional triage stage consumes payload projection
     - draft stage consumes payload or triage IR
     - optional polish stage
11. `release_workflow.py` selects final release source (`openai/local/cached-draft/manual`), writes selected changelog, then refreshes `debian/changelog` by extracting bullets from selected markdown.

---

## 2) File-by-File Responsibilities

## `models.py`
- Purpose: immutable data contracts for deterministic classifier pipeline.
- Key classes:
  - `CommitInfo`
  - `FileChange`
  - `DiffSnippet`
  - `CategoryContext`
  - `ReleaseContext`
- Inputs: instantiated by collection/build modules.
- Outputs: canonical in-memory shape passed through render/payload.
- Stage ownership: schema backbone across all deterministic stages.

## `git_collect.py`
- Purpose: extract raw git evidence.
- Key functions:
  - `get_head_sha()`, `get_latest_tag()`
  - `get_commits_since_tag()`
  - `get_commit_file_summary()`
  - `get_release_file_stats()`
  - `get_file_patch()`
- Inputs: repo root + optional previous tag.
- Outputs:
  - commit list with per-commit files/churn/categories
  - file-level churn map
  - per-file patch text
- Stage ownership: raw evidence acquisition.

## `categorize.py`
- Purpose: deterministic path/text-based classification and tagging.
- Key functions:
  - `categorize_path()`
  - `infer_categories_from_text()` (metadata-only commit fallback)
  - `extract_subscopes()`
  - `detect_flags()`
  - `detect_themes_for_paths()`
  - `normalize_path()`
- Inputs: paths + commit text.
- Outputs:
  - category labels
  - flags/subscopes/themes
- Stage ownership: heuristic taxonomy assignment.

## `scoring.py`
- Purpose: assign heuristic priority weights.
- Key functions:
  - `score_file_change()`
  - `score_patch_hunk()`
  - `_is_noise_path()`
- Inputs: path/category/churn/flags/commit counts and patch hunks.
- Outputs: float scores driving ranking/selection.
- Stage ownership: deterministic ranking heuristic.

## `snippets.py`
- Purpose: select and package top diff hunks.
- Key functions:
  - `split_patch_into_hunks()`
  - `extract_relevant_snippets()`
  - `build_snippet_reason()`
- Inputs: category contexts + repo + previous tag.
- Outputs: `dict[category, list[DiffSnippet]]` sorted by hunk score.
- Stage ownership: patch-level evidence reduction.

## `context_builder.py`
- Purpose: orchestrate deterministic context construction.
- Key functions:
  - `build_release_context()`
  - `get_file_commit_counts()`
  - `build_category_contexts()`
- Inputs:
  - git outputs
  - category/score/snippet helpers
- Outputs: `ReleaseContext`.
- Stage ownership: aggregation/orchestration of deterministic classifier output.

## `payload.py`
- Purpose: produce bounded model-facing evidence payload.
- Key functions:
  - `build_ai_payload()`
  - `classify_signal_strength()`
  - `_build_category_payload()`
  - `_build_commits/_build_files/_build_snippets()`
  - `_build_snippet_excerpt()`
  - `render_ai_payload_json()`
- Inputs: `ReleaseContext` + caps/filter config.
- Outputs: compact JSON (`schema_version=vaulthalla.release.ai_payload.v1`) with truncation metadata.
- Stage ownership: deterministic payload shaping/reduction.

## `render_raw.py`
- Purpose: deterministic human-reviewable markdown and debug views from `ReleaseContext`.
- Key functions:
  - `render_release_changelog()`
  - `render_debug_context()`
  - `render_debug_json()`
  - `_render_category_section()`
  - `_evidence_tier()`, `_is_weak_signal()`
- Inputs: `ReleaseContext`.
- Outputs: markdown draft / debug text / debug json.
- Stage ownership: deterministic textual presentation.

## `release_workflow.py`
- Purpose: release orchestration and AI/manual source selection.
- Key functions:
  - `resolve_release_changelog()`
  - `_run_release_ai_pipeline()`
  - `validate_manual_changelog_current()`
  - `validate_cached_draft_current()`
  - `refresh_debian_changelog_entry()`
- Inputs:
  - AI payload and settings
  - profile/provider config
  - manual/cached artifacts
- Outputs:
  - selected release markdown source
  - updated debian/changelog top entry
- Stage ownership: workflow control + fallback policy + final Debian projection.

---

## 3) Data Model and Transformation Chain (with loss map)

## 3.1 Raw git evidence
Shape:
- commit log records (`sha`, `subject`, `body`)
- per-commit file list and numstat
- aggregate diff numstat by file
- per-file full patch

Owned by:
- `git_collect.py`

Loss:
- none at acquisition step (except numstat omits semantic diff context by design).

## 3.2 Categorized/flagged evidence (`ReleaseContext` build)
Transform:
- path -> category (`categorize_path`)
- path -> flags/subscopes/themes (`detect_flags`, `extract_subscopes`, `detect_themes_for_paths`)
- metadata-only commit fallback category inference from subject/body regex
- per-file and per-hunk heuristic scoring

Owned by:
- `categorize.py`, `scoring.py`, `context_builder.py`, `snippets.py`

Loss:
- semantic diff meaning not modeled; replaced by heuristic tags and scores.
- theme inference is path-keyword driven, not content-driven.
- snippet reason discards hunk text (`build_snippet_reason` ignores hunk argument) and emits template reasons.

## 3.3 Reduced category evidence (`CategoryContext` internals)
Transform:
- files sorted by `score`
- snippets extracted from top N files and top M hunks/file

Owned by:
- `context_builder.py`, `snippets.py`

Loss:
- long-tail files/hunks dropped before payload stage.
- hunk semantic detail not summarized beyond generic reason + raw patch segment.

## 3.4 Model-facing payload (`AI payload v1`)
Transform (`payload.py`):
- keep only top commits/files/snippets per category
- snippet patch converted to excerpt with line/char hard caps
- include summary counters (`insertions/deletions`, counts, signal strength)
- include truncation metadata

Owned by:
- `payload.py`

Loss (explicitly lossy):
- bounded caps drop evidence deterministically.
- commit bodies absent (subject-only).
- snippet excerpts truncated and decontextualized.
- only top files/snippets survive.

## 3.5 Triage prompt projection (secondary reduction)
Transform (`ai/prompts/triage.py::_build_compact_payload_projection`):
- keep limited metadata and generation truncation summary
- for snippets, keep `top_snippet_paths` only (path list), no snippet text/reason
- hosted compact mode tightens limits further

Owned by:
- `ai/prompts/triage.py`

Loss (major):
- snippet content removed entirely before triage.
- hosted mode aggressively compresses categories/points/snippets.
- model sees increasingly path-centric evidence tokens.

## 3.6 Triage IR -> Draft input
Transform:
- triage output schema requires/encourages:
  - `important_files`
  - `retained_snippets`
  - `caution_notes`
- `build_triage_ir_payload()` passes these directly to draft stage.

Owned by:
- `ai/contracts/triage.py`
- `ai/stages/triage.py`

Loss:
- free-form semantic synthesis now entirely model-generated from reduced evidence.
- structural fields can dominate narrative despite limited semantic grounding.

## 3.7 Raw renderer and final workflow projection
Transform:
- raw changelog renderer includes evidence tiers, deltas, scores, top files/snippets.
- release workflow may select AI/cached/manual source, then Debian projection extracts first bullets.

Owned by:
- `render_raw.py`
- `release_workflow.py`

Loss:
- Debian projection keeps first list-like lines only (max 8), further narrowing meaning and reinforcing whatever bullet style upstream produced.

---

## 4) Deterministic Heuristics vs Semantic Interpretation

## Deterministic (current code is strong here)
- Category order and output ordering are stable.
- File/snippet selection uses deterministic score + cap logic.
- Payload truncation is explicit and reproducible.
- Renderer output is byte-stable by tests.

## Heuristic pseudo-interpretation (currently in deterministic layer)
- `detect_themes_for_paths()` infers semantic themes from path substrings.
- `classify_signal_strength()` treats churn/counts/snippet counts as “strength”.
- `_evidence_tier()` and weak-signal labeling in renderer produce interpretation-like language.
- snippet reasons claim significance from score/flags, not hunk meaning.

## Model interpretation (triage/draft)
- triage prompt asks model to produce `key_points`, `important_files`, `retained_snippets`, cautions from already reduced evidence.
- because projection is compact and snippet text is stripped to path identifiers, triage synthesis tends toward path-level abstraction and caution language.
- draft prompt depends on triage IR fields; if IR is path-heavy, draft follows.

Bottom line:
- deterministic stack is doing more than “selecting evidence”; it already injects significance signals.
- model stages then amplify these signals under strict schema pressure.

---

## 5) Current Failure Modes (why path-heavy/caution-heavy output appears)

Observed tendency: overemphasis on “Important files”, “Retained snippet”, “Caution”, churn counts, and path-heavy summaries.

Code-level causes:

1. **Schema pressure creates recurring slots**
- triage schema requires category arrays that include `important_files`, `retained_snippets`, and `caution_notes`.
- top-level also includes `caution_notes` and `dropped_noise`.
- even when evidence is thin, those slots encourage model to fill structural placeholders with generalized caution/path references.

2. **Prompt projection removes semantic snippet substance**
- triage compact projection converts snippet evidence to `top_snippet_paths` (paths only), discarding excerpt text/reasons.
- hosted compact mode tightens counts further (e.g., retained snippets max 1), increasing compression pressure.
- result: model has identifiers, counts, and flags, but limited deep diff semantics.

3. **Heuristic metadata is richly represented; semantics are sparsely represented**
- payload and raw render expose counts, scores, flags, insertions/deletions, evidence tiers prominently.
- commit body/context and diff semantics are shallowly represented after caps/truncation.
- models latch onto strongly repeated quantitative/path fields.

4. **Snippet reasoning is generic**
- `build_snippet_reason()` does not use hunk text and produces template rationale.
- “retained snippet” language is therefore weakly semantic by construction.

5. **Renderer and downstream projection reinforce this style**
- `render_release_changelog()` includes explicit “Top files”, score and delta framing.
- Debian refresh extracts bullet lines directly, so path/caution-heavy bullets leak into final Debian text.

6. **Category-level weak-signal logic encourages cautious tone**
- deterministic weak-signal classification plus prompt rules (“if evidence weak, caution briefly”) systematically produce caution statements.

---

## 6) Redesign Seams (best insertion points, no implementation)

## Seam A: split forensic payload vs model-semantic payload
Attach at:
- `payload.py::build_ai_payload()` and call sites in `cli.py` / `release_workflow.py`.

Idea:
- keep current payload as forensic/debug artifact (`payload_forensics`).
- add separate semantic payload for model consumption with richer selective diff semantics and reduced score/churn leakage.

Why here:
- single central projection point before all AI stages.
- preserves deterministic forensic artifact guarantees.

## Seam B: introduce selective diff-backed category packets before triage
Attach at:
- after `context_builder.build_category_contexts()` and snippet extraction, before `build_ai_payload()`.

Idea:
- construct per-category semantic packets using chosen hunks/commits with concise, grounded evidence text (not just paths).
- keep deterministic selection, but hand off semantic extraction to model-friendly representation.

Why here:
- preserves current deterministic selection machinery.
- adds semantics before triage compression.

## Seam C: triage contract rewrite away from path-oriented fields
Attach at:
- `ai/contracts/triage.py` and `ai/prompts/triage.py`.

Idea:
- reduce or demote `important_files`/`retained_snippets` as first-class output.
- prioritize “grounded claims with evidence refs” schema.
- keep caution as optional exception, not quasi-required rhetorical lane.

Why here:
- this is where path-heavy structure is currently enforced.

## Seam D: projection cleanup in triage prompt builder
Attach at:
- `_build_compact_payload_projection()` in `ai/prompts/triage.py`.

Idea:
- stop stripping snippets to path-only in primary mode.
- pass compact semantic excerpt packets, not score-heavy metadata blobs.

Why here:
- largest semantic loss before triage happens in this function.

## Seam E: renderer role separation
Attach at:
- `render_raw.py` and use sites in `cli.py`.

Idea:
- keep current renderer for deterministic forensic/raw review.
- avoid using raw forensic style as implicit model template.

Why here:
- prevents accidental coupling between debug framing and outward summary style.

## Seam F: Debian bullet extraction guardrail
Attach at:
- `release_workflow.py::_extract_debian_bullets()`.

Idea:
- optional filtering/sanitizing of path-heavy or “Important files/Retained snippet” bullets before Debian projection.

Why here:
- prevents low-value structural artifacts from becoming canonical packaging notes.

---

## 7) Recommended Phased Attack Plan

## Phase 0 — Instrumentation and mapping (low risk)
1. Add comparative trace artifact that records:
   - selected files/hunks
   - what was dropped by each cap
   - what fields reached triage prompt projection
2. Measure field frequency in triage/draft outputs (`important_files`, `retained_snippets`, caution prevalence, path density).

Outcome:
- hard baseline before redesign.

## Phase 1 — Introduce semantic model payload alongside forensic payload
1. Keep existing `build_ai_payload()` for forensic/debug.
2. Add new semantic packet builder (parallel output) with selective diff-backed evidence summaries.
3. Route triage to semantic payload while continuing to write old forensic artifact.

Outcome:
- semantic gain without losing existing operations visibility.

## Phase 2 — Triage contract and prompt simplification
1. Replace path-centric triage fields with claim-centric evidence references.
2. Make caution fields optional/minimal and only for weak or conflicting evidence.
3. Rebalance hosted compact mode so compression preserves semantic anchors.

Outcome:
- triage IR becomes cleaner drafting substrate, not a metadata recap.

## Phase 3 — Downstream cleanup (draft/polish/debian projection)
1. Update draft prompt to consume claim/evidence packets directly.
2. Reduce leakage of score/churn/path noise into final markdown.
3. Guard Debian projection against low-signal structural bullets.

Outcome:
- end artifacts focus on change meaning rather than classifier internals.

## Phase 4 — Optional classifier expansion
1. If needed, improve deterministic selectors (better hunk selection, scope-specific scoring).
2. Keep deterministic modules focused on selection, not synthesis.

Outcome:
- stronger evidence quality without reintroducing pseudo-semantic heuristics into deterministic layer.

---

## Practical Answers for Future Redesign Tasks

- **Where to inject selective diff data?**
  - Between `context_builder`/`snippets` output and `payload.build_ai_payload()`, plus triage prompt projection path.

- **Where is semantic meaning currently lost most?**
  1) `payload` caps/truncation, 2) triage compact projection reducing snippets to paths, 3) triage schema forcing path/caution slots.

- **Which modules should stay deterministic?**
  - `git_collect`, `categorize`, `scoring`, `snippets` selection mechanics, ordering/truncation contracts, forensic render.

- **Which modules should hand off to models?**
  - semantic claim synthesis and category interpretation (triage/draft), after deterministic evidence selection but before over-compression.

- **How to redesign without breaking release flow?**
  - dual-track artifacts first (forensic + semantic), switch triage input to semantic track, then contract cleanup; keep fallback/source selection and release workflow unchanged until semantic outputs stabilize.
