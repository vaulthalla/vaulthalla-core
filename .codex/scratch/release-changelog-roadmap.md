---

# Release Changelog Roadmap (Pipeline Completion)

Scope: complete the full release pipeline from git → AI → packaged artifact → CI/CD deployment signal.

---

## Status (Current Reality)

The system has progressed significantly beyond the original baseline.

Completed:

- ✅ Version pipeline is production-usable (`check/sync/set-version/bump`)
- ✅ Collector pipeline exists and is stable (`git_collect`, `categorize`, `scoring`, `snippets`, `context_builder`)
- ✅ Deterministic raw renderer implemented (`render_raw`)
- ✅ CLI draft workflow implemented (`changelog draft`)
- ✅ Deterministic AI payload builder implemented (Phase 4)
- ✅ AI draft stage implemented (Phase 5a)
- ✅ AI triage optimizer stage implemented (Phase 5b)

Current pipeline:

git → categorized → scored → payload → triage → IR → draft → markdown

Remaining work focuses on:
- polish stage
- provider flexibility
- packaging automation
- CI/CD integration

---

## Phase 5c: Polish Stage (Editorial Pass)

Goal:

- refine AI draft output for readability, flow, and conciseness
- eliminate redundancy and awkward phrasing
- preserve strict factual grounding

Key constraints:

- input must NOT be raw payload
- input must be:
  - structured draft result OR
  - rendered markdown + minimal structure
- must NOT reintroduce hallucination risk
- must NOT expand scope beyond draft content

Concrete tasks:

1. Add polish contract:
   - `contracts/polish.py`
   - structured output similar to draft, or minimal markdown rewrite

2. Add polish prompt:
   - `prompts/polish.py`
   - enforce:
     - no new facts
     - no feature invention
     - compression + clarity only

3. Add polish stage:
   - `stages/polish.py`
   - input: draft result
   - output: refined result

4. CLI integration:
   - optional flag: `--polish`
   - pipeline:
     - payload → (triage?) → draft → polish

Expected output:

- clean, professional changelog suitable for publishing

Verification:

- fixture-based tests comparing:
  - original draft vs polished output
  - preservation of facts
  - reduced redundancy

---

## Phase 6: Provider Abstraction + Local LLM Support

Goal:

- support multiple inference backends:
  - OpenAI hosted
  - OpenAI-compatible local endpoints (vLLM, mlx, etc.)

Concrete tasks:

1. Expand provider layer:
   - `providers/base.py` (interface)
   - `providers/openai.py`
   - `providers/openai_compatible.py`

2. Add config surface (code-level only):
   - base_url
   - model
   - api_key (env)

3. Allow CLI override:
   - `--model`
   - optional future: `--provider`, `--base-url`

4. Ensure:
   - all stages are provider-agnostic
   - contracts remain identical regardless of backend

Expected output:

- identical pipeline behavior across hosted and local models

Verification:

- mock tests
- manual run against local OpenAI-compatible endpoint

---

## Phase 7: Debian Packaging Automation

Goal:

- automate `.deb` build pipeline from inside `tools/release`

This phase transitions from:
> changelog generation → actual release artifact creation

Concrete tasks:

1. Add packaging module:
   - `tools/release/debian/` (or similar)

2. Implement build orchestration:
   - run `dpkg-buildpackage` or equivalent
   - ensure:
     - version is synced
     - changelog is updated
     - build artifacts are captured

3. Add release output directory:
   - e.g. `/release/` or `/dist/`
   - store:
     - `.deb`
     - build logs
     - metadata

4. Optional:
   - hook changelog generation into Debian changelog automatically

Expected output:

- one command produces a full `.deb` artifact

Example CLI:

    python -m tools.release build-deb

Verification:

- successful local `.deb` build
- install test via dpkg/apt

---

## Phase 8: GitHub CI/CD + Production Channel

Goal:

- fully integrate release pipeline into CI/CD
- surface deployments as “Production” in GitHub UI

Concrete tasks:

1. GitHub Actions workflow:
   - trigger on:
     - tag push OR manual dispatch
   - steps:
     - version validation
     - changelog generation
     - optional AI draft
     - Debian build
     - artifact upload

2. Add release publishing:
   - attach `.deb` to GitHub Release
   - optionally push to Nexus APT repo

3. Add environment deployment signal:
   - mark release as:
     - `production`
   - use GitHub environments:
     - `production`
     - optional `staging`

4. Optional:
   - release notes auto-filled from AI changelog

Expected output:

- full pipeline:
  - commit/tag → CI → artifact → release → production signal

Verification:

- test workflow on tagged release
- confirm:
  - artifact present
  - release notes generated
  - environment marked as deployed

---

## Final Pipeline (Target State)

git
 → collector
 → payload
 → triage (5b)
 → draft (5a)
 → polish (5c)
 → markdown
 → debian packaging (7)
 → CI/CD release (8)
 → production deployment signal

---

## Updated Progress Tracker

- [ ] Phase 0 baseline (optional / partially bypassed)
- [x] Phase 1 collector quality
- [x] Phase 2 raw renderer
- [x] Phase 3 CLI draft workflow
- [x] Phase 4 deterministic AI payload
- [x] Phase 5a AI draft
- [x] Phase 5b AI triage
- [ ] Phase 5c AI polish
- [ ] Phase 6 provider abstraction + local LLM
- [ ] Phase 7 Debian packaging automation
- [ ] Phase 8 CI/CD + production channel
