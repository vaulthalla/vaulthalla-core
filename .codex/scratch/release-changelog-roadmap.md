# Release Changelog Roadmap (Control Plane & Hardening Phase)

This roadmap reflects the transition from feature buildout to control-plane design, integration quality, and production hardening.

The core release and AI drafting pipeline is complete.  
Current focus is on making the system configurable, scalable, and operationally stable.

---

## Implemented Spine (As Of Today)

Completed phases:

- ✅ Phase 1: collector quality foundation  
- ✅ Phase 2: deterministic raw renderer  
- ✅ Phase 3: CLI changelog draft workflow  
- ✅ Phase 4: deterministic AI payload builder  
- ✅ Phase 5a: AI draft stage  
- ✅ Phase 5b: optional triage stage  
- ✅ Phase 5c: optional polish stage  
- ✅ Phase 6a: provider seam (hosted OpenAI + OpenAI-compatible)  
- ✅ Phase 6b: operator preflight workflow (`changelog ai-check`)  
- ✅ Phase 7a: local Debian packaging orchestration (`build-deb`)  
- ✅ Phase 7b: web deployable artifact inclusion in release outputs  
- ✅ Phase 8: GitHub release workflow with `Production` environment tracking  

Current command spine exists end-to-end from local tooling through CI release artifact generation.

---

## Current End-to-End Flow

1. Version guard/sync:
   - `check`, `sync`, `set-version`, `bump`

2. Deterministic release context:
   - git collect → categorize → score → snippets → context

3. Deterministic outputs:
   - raw markdown draft  
   - model-ready payload JSON  

4. AI drafting path:
   - payload → optional triage → draft → optional polish → local markdown  

5. Local packaging:
   - `build-deb` (Debian artifacts + Next standalone tarball)  

6. CI release path:
   - `.github/workflows/release.yml`  
   - canonical package action: `.github/actions/package/action.yml`  
   - artifact upload + optional GitHub Release attachment  
   - deployment tracked under GitHub `Production` environment  

---

## Today’s Key Contract Learnings

1. Debian version validation is upstream-only:
   - `upstream(debian/changelog) == VERSION`
   - revision suffix is not part of canonical comparison.

2. Release workflow web parity requires `build_web` semantics:
   - private icon sync from `~/vaulthalla-web-icons` must happen before web checks.

3. Debian Meson source directory must be `core/` in `debian/rules`.

4. Debian packaging requires explicit staging of non-Meson payloads expected by `debian/install`.

5. Next standalone archive copy must preserve symlinks in CI (pnpm-linked layouts).

6. `environment: Production` in release workflow is mandatory by design.

7. `.github/actions/package` is the canonical CI packaging entrypoint; avoid duplicate packaging logic in workflows.

---

## Phase 9: AI Pipeline Control Plane & Configuration

**Goal:** Replace ad-hoc CLI-driven AI configuration with a structured, reusable control plane.

### Part 1: Control Plane Spine

- Support per-stage model configuration:
  - triage
  - draft
  - polish
- Allow global fallback model if stage-specific config is absent.
- Remove assumption of single model across entire pipeline.
- Introduce config file (e.g. `.vaulthalla/ai.yml`)
- Support named profiles for reusable provider and stage configuration
- Add CLI support:
  - `--ai-profile <slug>`

```yaml
profiles:
  local-gemma:
    provider: openai-compatible
    base_url: http://127.0.0.1:8888/v1
    stages:
      triage:
        model: Gemma-4-31B
        reasoning_effort: low
      draft:
        model: Gemma-4-31B
        reasoning_effort: medium
      polish:
        model: Gemma-4-31B
        reasoning_effort: high

  openai-balanced:
    provider: openai
    stages:
      triage:
        model: gpt-5-nano
        reasoning_effort: low
      draft:
        model: gpt-5-mini
        reasoning_effort: medium
      polish:
        model: gpt-5.4
        reasoning_effort: high
```

- Make CLI an override layer:
  - profile provides defaults
  - flags override selectively
- Reduce verbosity for common usage.

### Part 2: Provider Semantics & Capability Layer

- Introduce `reasoning_effort` at stage level:
  - `low`, `medium`, `high`
- Map to provider-specific behavior:
  - OpenAI: native API support
  - OpenAI-compatible: best-effort or prompt-level hint
- Ensure safe no-op when unsupported.
- Add configurable structured output modes:
  - strict JSON schema
  - JSON object
  - prompt-driven JSON
- Default behavior adapts per provider capability.
- Providers declare capabilities:
  - supports_reasoning_effort
  - supports_strict_schema
- Pipeline adapts behavior dynamically based on provider.

### Part 3: Provider Transport Refinement

- OpenAIProvider:
  - migrate toward Responses API where applicable
  - support reasoning effort natively
  - align request construction with stage-level `reasoning_effort` and `structured_mode`
  - preserve clean compatibility with the current control-plane contract

- OpenAICompatibleProvider:
  - support fallback structured modes
  - avoid strict schema assumptions
  - improve resilience for local models
  - treat provider capabilities honestly and degrade intentionally when native support is absent

- Structured output fallback behavior:
  - define explicit fallback order for structured generation:
    - strict JSON schema
    - JSON object
    - prompt-driven JSON
  - ensure fallback behavior is deterministic, testable, and provider-aware
  - avoid silent behavior changes that make debugging ambiguous

- Prompt discipline and output tightening:
  - revise stage prompts to reduce verbosity and speculative filler
  - tighten instructions so outputs stay evidence-bound and concise
  - explicitly forbid unsupported extrapolation beyond commit/changelog evidence
  - reduce category bloat and duplicate phrasing across triage/draft/polish
  - preserve useful uncertainty language when evidence is weak
  - ensure local/open models are steered toward:
    - shorter summaries
    - cleaner bullets
    - less repeated framing text
    - no decorative or generic release-language padding

- JSON/output robustness:
  - improve extraction/repair handling for providers that return extra wrapper text or noisy JSON-adjacent output
  - validate parsed outputs cleanly before continuing pipeline stages
  - distinguish clearly between:
    - transport failure
    - invalid JSON
    - unsupported structured mode
    - schema mismatch
  - make failure messages operator-useful and debugging-friendly

- Reasoning / response tuning:
  - ensure reasoning settings are only passed where supported
  - prevent local-compatible transports from receiving unsupported provider fields by default
  - minimize unnecessary response verbosity for stages that should stay operational and terse
  - keep stage behavior aligned with intended workload:
    - triage = lean classification
    - draft = structured synthesis
    - polish = concise refinement, not ornamental rewriting

- Provider-specific request shaping:
  - allow transport/request builders to adapt per provider without changing stage logic
  - centralize request-shaping logic so prompt + transport behavior is easier to audit
  - keep provider-specific hacks contained rather than leaking into stage code

- Resilience and operator trust:
  - make degraded-mode behavior explicit in code/tests
  - ensure local-model paths fail soft where possible instead of crashing on unsupported features
  - avoid hidden “best effort” behavior that changes output quality unpredictably
  - keep defaults conservative and stable for repeatable release drafting

- Tests and verification:
  - add regression tests for provider-specific request construction
  - add tests for structured-mode fallback behavior
  - add tests for prompt/output tightening expectations where practical
  - add tests ensuring noisy local-model responses are handled or rejected clearly
  - verify transport changes do not break existing profile/control-plane behavior

### Part 4: Execution Tuning, Stage Contracts & Operator Controls

- Formalize stage execution contract:
  - `draft` is required
  - `triage` and `polish` run if defined in the selected profile
  - execution order is fixed:
    - `triage -> draft -> polish`
  - CLI remains an override layer rather than the primary orchestrator
  - optional CLI disables may be added later if needed (`--no-triage`, `--no-polish`)

- Expand profile schema for execution tuning:
  - support profile-level defaults plus per-stage overrides for:
    - `temperature`
    - `max_output_tokens`
  - preserve sane built-in defaults so config remains optional and low-maintenance
  - keep new tuning fields optional and tightly validated

- Temperature controls:
  - add stage-aware temperature support
  - intended defaults:
    - `triage: 0`
    - `draft: 0.2`
    - `polish: 0`
  - allow per-profile global defaults plus per-stage override
  - clamp to safe ranges so operators cannot accidentally push the pipeline into creative/sloppy behavior
  - treat temperature as a fine-tuning lever, not a primary behavior driver

- Max output token controls:
  - add support for fixed token ceilings per stage
  - allow profile-level defaults plus per-stage override
  - keep hard ceilings explicit for operator control, debugging, and CI stability
  - use this as the primary hard constraint for shaping stage output length

- Dynamic token budgeting:
  - support dynamic max token budgeting based on input size
  - prefer a bounded ratio-based mode over naive guessed token counts
  - proposed mode shape:
    - `fixed`
    - `dynamic_ratio`
  - dynamic mode should compute output budget from input size, then clamp to configured min/max bounds
  - keep behavior deterministic and testable

- Default dynamic-ratio behavior:
  - consider making bounded `dynamic_ratio` the default stage token policy
  - use sane stage-specific defaults so output scales with release size without rambling or truncating
  - likely direction:
    - triage = low ratio, tight cap
    - draft = higher ratio, broad cap
    - polish = medium ratio, moderate cap
  - ensure very small releases still get enough minimum output space
  - ensure very large releases cannot explode output size

- Config precedence and resolution:
  - continue deterministic resolution order:
    - system defaults
    - profile defaults
    - stage overrides
    - CLI overrides
  - ensure temperature and token budget fields resolve the same way as model/reasoning/structured mode
  - keep config semantics uniform and unsurprising

- Validation and safety rails:
  - reject invalid temperature ranges
  - reject invalid token-budget shapes
  - validate dynamic ratio config:
    - required fields present
    - ratio in sane range
    - min/max consistent
  - reject malformed stage definitions cleanly
  - keep errors operator-readable

- Observability and debugging:
  - surface resolved execution settings per stage when useful:
    - model
    - reasoning effort
    - structured mode
    - temperature
    - max output token policy
  - make it easy to inspect why a given run behaved the way it did
  - keep runtime reporting concise and useful rather than noisy

- Backward compatibility:
  - existing profiles without new tuning fields should continue to work
  - existing CLI flows should continue to function with built-in defaults
  - deterministic non-AI changelog paths remain untouched

- Testing and hardening:
  - add regression tests for:
    - stage presence/required-stage contract
    - execution order
    - temperature resolution and validation
    - fixed token budget resolution
    - dynamic-ratio token budget resolution
    - min/max clamping behavior
    - backward compatibility when tuning fields are absent
  - verify large and small release inputs do not produce pathological token budgets

```yaml
profiles:
  local-gemma:
    provider: openai-compatible
    base_url: http://127.0.0.1:8888/v1

    default_temperature:
      triage: 0
      draft: 0.2
      polish: 0

    default_max_output_tokens:
      triage: 300
      draft:
        mode: dynamic_ratio
        ratio: 0.35
        min: 800
        max: 4000
      polish: 800

    stages:
      triage:
        model: Gemma-4-31B
        reasoning_effort: low

      draft:
        model: Gemma-4-31B
        reasoning_effort: medium
        structured_mode: strict_json_schema

      polish:
        model: Gemma-4-31B
        reasoning_effort: high

  openai-balanced:
    provider: openai

    default_temperature:
      triage: 0
      draft: 0.15
      polish: 0

    default_max_output_tokens:
      triage:
        mode: dynamic_ratio
        ratio: 0.12
        min: 200
        max: 800
      draft:
        mode: dynamic_ratio
        ratio: 0.3
        min: 900
        max: 3500
      polish:
        mode: dynamic_ratio
        ratio: 0.2
        min: 500
        max: 1500

    stages:
      triage:
        model: gpt-5-nano
        reasoning_effort: low

      draft:
        model: gpt-5-mini
        reasoning_effort: medium
        structured_mode: strict_json_schema

      polish:
        model: gpt-5.4
        reasoning_effort: high
```
---

## Phase 10: Deployment Integration & CI Hardening

**Goal:** Integrate AI output into release pipeline safely and reliably.

### 10a: AI Draft Integration into Release Workflow

- Decide invocation point in CI:
  - pre-release vs post-build
- Ensure deterministic fallback path when AI is disabled or fails.

### 10b: CI Reliability & Diagnostics

- Improve failure visibility for:
  - packaging
  - artifact validation
  - AI stage failures
- Validate runner assumptions for web + Debian builds.

### 10c: Packaging Validation Pass

- Verify Debian staging correctness:
  - ensure installed contents match expectations
- Confirm web artifact integrity.

---

## Phase 11: Publication & Distribution

**Goal:** Extend release pipeline into full distribution lifecycle.

### 11a: Nexus/APT Publication Automation

- Automate upload to Nexus APT repository
- Define promotion rules and environment boundaries
- Secure credentials and environment handling

### 11b: Debian Post-Install UX (Deferred)

- Implement optional nginx install prompt in `postinst`
- Keep non-blocking and configurable
- Avoid interfering with automated installs

---

## Fast Status Summary

The release and AI drafting backbone is complete.

The system is now entering a control-plane phase:
- multi-model orchestration
- structured configuration
- provider-aware execution

Once stabilized, focus shifts to:
- CI integration
- publication automation
- operational hardening

The goal is not more features — it is **predictability, flexibility, and control at scale**.
