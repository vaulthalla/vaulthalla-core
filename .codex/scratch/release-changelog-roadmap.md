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

### 9a: Multi-Stage Model Configuration

- Support per-stage model configuration:
  - triage
  - draft
  - polish
- Allow global fallback model if stage-specific config is absent.
- Remove assumption of single model across entire pipeline.

### 9b: Reasoning Effort Integration

- Introduce `reasoning_effort` at stage level:
  - `low`, `medium`, `high`
- Map to provider-specific behavior:
  - OpenAI: native API support
  - OpenAI-compatible: best-effort or prompt-level hint
- Ensure safe no-op when unsupported.

### 9c: Structured Output Strategy Control

- Add configurable structured output modes:
  - strict JSON schema
  - JSON object
  - prompt-driven JSON
- Default behavior adapts per provider capability.

### 9d: AI Profile System (Config File)

- Introduce config file (e.g. `.vaulthalla/ai.yml`)
- Support named profiles for reusable provider and stage configuration
- CLI support:
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

### 9e: CLI Simplification & Override Layer

- CLI becomes override-only:
  - profile provides defaults
  - flags override selectively
- Reduce verbosity for common usage:
  - `--ai-profile local-gemma` becomes primary entrypoint

### 9f: Provider Capability Mapping

- Providers declare capabilities:
  - supports_reasoning_effort
  - supports_strict_schema
- Pipeline adapts behavior dynamically based on provider.

### 9g: Provider Transport Refinement

- OpenAIProvider:
  - migrate toward Responses API where applicable
  - support reasoning effort natively

- OpenAICompatibleProvider:
  - support fallback structured modes
  - avoid strict schema assumptions
  - improve resilience for local models

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
