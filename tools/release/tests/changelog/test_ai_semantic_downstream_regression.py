from __future__ import annotations

import json
from pathlib import Path
import unittest

from tools.release.changelog.ai.prompts.draft import build_draft_user_prompt
from tools.release.changelog.ai.prompts.polish import build_polish_user_prompt
from tools.release.changelog.ai.prompts.release_notes import build_release_notes_user_prompt
from tools.release.changelog.ai.prompts.triage import build_triage_user_prompt


FIXTURES_DIR = Path(__file__).parent / "fixtures"


def _load_json_fixture(name: str) -> dict:
    return json.loads((FIXTURES_DIR / name).read_text(encoding="utf-8"))


class AISemanticDownstreamRegressionTests(unittest.TestCase):
    def test_triage_projection_uses_semantic_payload_fields(self) -> None:
        payload = _load_json_fixture("semantic_payload_realistic.json")
        user_prompt = build_triage_user_prompt(payload)
        lower = user_prompt.lower()

        self.assertIn("semantic payload (compact projection)", lower)
        self.assertIn("summary_hint", lower)
        self.assertIn("semantic_hunks", lower)
        self.assertIn("kind", lower)
        self.assertIn("why_selected", lower)
        self.assertIn("grounded_claims", lower)
        self.assertNotIn("important_files", lower)
        self.assertNotIn("retained_snippets", lower)

    def test_draft_prompt_pushes_claim_centric_triage_handoff(self) -> None:
        triage_ir = {
            "schema_version": "vaulthalla.release.ai_triage.v2",
            "version": "2.5.0",
            "summary_points": ["Release tooling and core reliability updates dominate this cycle."],
            "categories": [
                {
                    "name": "tools",
                    "signal_strength": "strong",
                    "priority_rank": 1,
                    "theme": "Release tooling contract and workflow updates",
                    "grounded_claims": ["Triage schema now requires theme and grounded claims."],
                    "evidence_refs": ["tools/release/changelog/ai/contracts/triage.py#schema-change"],
                }
            ],
        }
        user_prompt = build_draft_user_prompt(triage_ir, source_kind="triage").lower()
        self.assertIn("theme", user_prompt)
        self.assertIn("grounded_claims", user_prompt)
        self.assertIn("do not echo file paths/lists", user_prompt)
        self.assertIn("slot-by-slot recap language", user_prompt)

    def test_polish_and_release_notes_prompts_remove_classifier_residue(self) -> None:
        polish_prompt = build_polish_user_prompt(
            {
                "schema_version": "vaulthalla.release.ai_draft.v1",
                "title": "Release 2.5.0",
                "summary": "Priority rank 1 category and evidence refs dominate.",
                "sections": [
                    {
                        "category": "tools",
                        "overview": "Evidence refs indicate tools/release/changelog/ai/contracts/triage.py changed.",
                        "bullets": ["important files: tools/release/changelog/ai/contracts/triage.py"],
                    }
                ],
            }
        ).lower()
        notes_prompt = build_release_notes_user_prompt(
            "# Release\n- evidence_refs: tools/release/changelog/ai/contracts/triage.py#schema-change\n"
        ).lower()

        self.assertIn("classifier/audit residue", polish_prompt)
        self.assertIn("path-heavy recap", polish_prompt)
        self.assertIn("remove classifier residue", notes_prompt)
        self.assertNotIn("brand voice reference", notes_prompt)


if __name__ == "__main__":
    unittest.main()
