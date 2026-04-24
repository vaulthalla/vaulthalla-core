# Release Classifier → Triage Upgrade Roadmap

## Thesis

The current release-tooling pipeline is strongest at deterministic evidence collection and weakest at preserving semantic meaning for downstream AI stages.

Right now the deterministic classifier stack is over-compressing the release into a forensics-style report dominated by:

- evidence tiers
- commit/file/snippet counts
- top files
- churn deltas
- snippet provenance
- path-heavy metadata

That data is useful for debugging and trust, but it is a poor substrate for triage. Triage is being forced to reconstruct release meaning from bookkeeping rather than from actual change content. This is why downstream output keeps drifting toward:

- `Important files`
- `Retained snippet`
- `Caution`
- path-heavy summaries
- churn commentary

The upgrade goal is to preserve the current deterministic forensic pipeline while adding a new model-facing semantic payload that gives triage richer, more complete, more meaningful evidence.

---

## North Star

Move from:

- heuristics summarizing
- models compressing score-heavy metadata

To:

- heuristics selecting evidence
- models interpreting selected semantic evidence

Put bluntly:

> deterministic stages should choose what matters  
> model stages should explain what changed

---

## Current Structural Problems

### 1. The model-facing input is dominated by metadata, not change meaning
The raw draft input is rich in:
- evidence strength
- themes
- key commits
- top files
- snippets
- counts and deltas

But poor in:
- real diff semantics
- actual code/config/command changes
- grounded explanation of what changed and why

### 2. Semantic meaning is lost before triage
Biggest loss points:
- payload caps/truncation
- snippet reduction
- triage compact projection stripping snippet text down to paths
- triage schema pressure toward `important_files`, `retained_snippets`, `caution_notes`

### 3. The deterministic layer is doing pseudo-synthesis
The classifier stack currently does more than select evidence:
- infers themes from paths
- labels signal strength
- ranks significance from churn
- emits generic snippet reasons

This creates “meaning-like” metadata that the model then amplifies, even when the actual semantic evidence is weak.

### 4. Release notes cannot recover from bad changelog substrate
`release_notes` is not the main problem. It is polishing already path-heavy, caution-heavy changelog output. Until upstream evidence quality improves, downstream presentation stages will continue polishing weak material.

---

## Upgrade Strategy

## Phase 1 — Introduce a Dual-Track Output Model

### Goal
Keep the current deterministic forensic artifact, but add a new semantic artifact for triage.

### Keep
- existing `ReleaseContext`
- existing forensic/debug payload
- existing raw draft render for inspection/debugging

### Add
A new model-facing semantic payload, for example:

- `.changelog_scratch/changelog.semantic_payload.json`

### Why
We need to separate:
- **forensic trust/debugging data**
- **semantic model input**

The current system is trying to use one artifact for both jobs.

### Deliverables
- existing payload remains intact for debugging
- new semantic payload exists in parallel
- triage can later be switched to consume semantic payload instead of forensic payload

---

## Phase 2 — Build Deterministic Semantic Packets Per Category

### Goal
Feed triage bounded, meaningful evidence rather than score summaries.

### Per-category packet should include
- category name
- optional priority hint
- 2–4 strongest commit subjects
- 3–6 selected semantic diff hunks
- 1–3 supporting files max
- optional operator note only when strongly justified

### Packet shape should prefer
- commit subjects that explain the change
- diff excerpts showing:
  - function/class signature changes
  - config key additions/changes
  - CLI surface changes
  - schema field changes
  - prompt/contract changes
  - validation/control-flow changes
  - artifact/output path changes
  - error handling / diagnostics changes

### Packet shape should avoid
- file score spam
- snippet provenance prose
- churn-first framing
- path-only evidence
- formatting/import noise
- repetitive test fixture churn unless it exposes a real contract change

### Key principle
The deterministic layer should not explain the release. It should select a compact evidence packet the model can actually reason over.

---

## Phase 3 — Upgrade Snippet Selection Into Semantic Hunk Selection

### Goal
Improve the quality of selected evidence before triage sees it.

### Current issue
Snippets are selected heuristically, but their reasoning is generic and later projections reduce them too aggressively.

### Upgrade
Keep hunk selection deterministic, but enrich the selected snippet object with:

- `kind`
- `excerpt`
- `why_selected`

### Example `kind` values
- `command-surface`
- `config-surface`
- `api-contract`
- `error-handling`
- `filesystem-lifecycle`
- `packaging-script`
- `prompt-contract`
- `schema-change`
- `output-artifact`

### Example `why_selected`
- "changes request construction and diagnostics"
- "adds comparison artifact generation"
- "updates lifecycle teardown behavior"
- "introduces prompt/contract constraints for release stages"

### Rule
No AI here yet. This is still deterministic evidence selection, not semantic summarization.

---

## Phase 4 — Route Triage to the Semantic Payload

### Goal
Make triage consume actual semantic evidence instead of compacted forensic metadata.

### Current issue
Triage compact projection is one of the worst semantic loss points in the whole system because it strips snippet content down to path identifiers.

### Upgrade
Switch triage input from:
- current forensic payload projection

To:
- new semantic payload projection

### Expected result
Triage will no longer have to reverse-engineer the release story from:
- path lists
- count summaries
- score metadata
- generic snippet slots

It can instead focus on:
- grouping
- ranking
- synthesizing grounded claims

### Important note
Do this before rewriting triage prompts or contracts aggressively. Fix the substrate first.

---

## Phase 5 — Simplify the Triage Contract

### Goal
Make triage output claim-centric, not metadata-centric.

### Demote or remove as first-class fields
- `important_files`
- `retained_snippets`
- default `caution_notes`
- churn commentary
- path-heavy recap fields

### Prefer fields like
- `theme`
- `summary_points`
- `grounded_claims`
- `evidence_refs`
- optional `operator_note` only when strongly justified

### Rule for caution
Caution should be exceptional, not structural.
Most releases do not need a warning label per category.

### Desired effect
Triage IR should become a clean drafting substrate, not a report template.

---

## Phase 6 — Clean Downstream Stages

### Goal
Stop downstream stages from inheriting classifier internals.

### Draft should consume
- themes
- grounded claims
- compact evidence references

### Draft should stop inheriting
- path-heavy slots
- retained snippet language
- score/churn framing
- default caution culture

### Polish should become
- actual wording refinement
- redundancy removal
- readability cleanup

### Release notes should become
- presentation-focused
- more human-readable
- optionally brand-aware in a restrained way
- still derived from grounded changelog content

### Debian projection should gain guardrails
Filter or reject bullets containing things like:
- `Important files:`
- `Retained snippet:`
- path-confetti bullets
- low-value structural metadata

---

## Optional Future Phase — Lightweight Category Interpretation Stage

### Goal
If needed later, add a small model stage between deterministic selection and global triage.

### What it would do
Per category, a cheap model could read the semantic packet and output:
- concise category summary
- 3–5 grounded changes
- optional operator note if justified

### What it should not become
Do **not** start with a full commit-by-commit classifier pipeline unless simpler category packets fail.

### Reason
Commit-by-commit interpretation is likely to:
- inflate orchestration complexity
- increase duplicate information
- slow CI unnecessarily
- move the compression problem one layer earlier

### Better principle
Category packets first. Commit-level AI only if later evidence shows it is necessary.

---

## Expected Outcomes

If this roadmap is executed well, we should see:

### Upstream
- less semantic loss before triage
- richer, more complete, more meaningful model input
- reduced dependence on score-heavy metadata

### Triage
- less path obsession
- less caution spam
- better grounded thematic grouping
- potentially lower reasoning requirements again

### Draft / Polish / Release Notes
- cleaner release story
- fewer audit-report artifacts
- better human readability
- less need for downstream cleanup gymnastics

### Debian changelog
- more meaningful bullets
- fewer path-heavy and structurally awkward entries

---

## Recommended Implementation Order

### Pass 1
Add semantic payload in parallel with current forensic payload.

### Pass 2
Add deterministic semantic hunk selection and category packets.

### Pass 3
Switch triage to semantic payload while keeping current triage schema mostly intact.

### Pass 4
Evaluate whether triage reasoning can be reduced once substrate improves.

### Pass 5
Rewrite triage schema/prompt to remove path-heavy and caution-heavy structural pressure.

### Pass 6
Clean draft, polish, release_notes, and Debian bullet extraction to consume the improved IR.

---

## Design Rules to Preserve

### Keep deterministic
- git collection
- category assignment
- evidence selection
- ordering
- truncation behavior
- forensic/debug artifact generation

### Let models do
- semantic interpretation
- thematic grouping
- grounded summary writing
- presentation refinement

### Do not let deterministic heuristics pretend to understand release meaning
Heuristics should choose evidence, not narrate the release.

---

## Practical Success Criteria

We should consider this upgrade successful when:

- triage no longer emits `Important files` and `Retained snippet` style clutter by default
- caution language becomes rare and justified
- changelog sections describe what changed, not which files ranked highest
- release notes feel like polished engineering communication, not an audit recap
- Debian changelog bullets stop inheriting classifier internals
- triage can plausibly move back toward lower reasoning effort because it is finally receiving meaningful evidence

---

## Concrete First Task After This Roadmap

Implement a dual-track payload system:

1. keep current forensic payload unchanged
2. add semantic payload with selective diff-backed category packets
3. expose both as artifacts
4. route triage to semantic payload in a guarded switch

That is the lowest-risk, highest-value first upgrade.
