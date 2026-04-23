from __future__ import annotations

import unittest

from tools.release.changelog.ai.contracts.emergency_triage import (
    AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
    AIEmergencyTriageItem,
    AIEmergencyTriageResult,
)
from tools.release.changelog.ai.stages.emergency_triage import (
    AI_TRIAGE_SYNTHESIZED_INPUT_SCHEMA_VERSION,
    build_semantic_evidence_unit_id,
    build_triage_input_from_emergency_result,
    run_emergency_triage_stage,
)


class _FakeProvider:
    def __init__(self, response: dict) -> None:
        self._response = response
        self.calls: list[dict] = []

    def generate_structured_json(self, **kwargs):
        self.calls.append(kwargs)
        return self._response


def _sample_semantic_payload() -> dict:
    return {
        "schema_version": "vaulthalla.release.semantic_payload.v1",
        "version": "0.34.1",
        "previous_tag": "v0.34.0",
        "head_sha": "abc123",
        "commit_count": 2,
        "categories": [
            {
                "name": "tools",
                "signal_strength": "strong",
                "summary_hint": "Release tooling and OpenAI integration changes.",
                "key_commits": ["Improve AI compare output provenance"],
                "candidate_commits": [
                    {
                        "sha": "abcd1234",
                        "subject": "Improve AI compare output provenance",
                        "body": "Include more config and output context.",
                        "categories": ["tools"],
                        "weight": "medium",
                        "weight_score": 160,
                        "changed_files": 4,
                        "insertions": 80,
                        "deletions": 10,
                        "sample_paths": ["tools/release/cli.py"],
                    }
                ],
                "supporting_files": ["tools/release/cli.py"],
                "semantic_hunks": [
                    {
                        "path": "tools/release/cli.py",
                        "kind": "output-artifact",
                        "why_selected": "captures comparison artifact generation flow",
                        "excerpt": "@@ ...",
                        "region_type": "function",
                        "context_label": "cmd_changelog_ai_compare",
                    },
                    {
                        "path": "tools/release/changelog/ai/prompts/triage.py",
                        "kind": "prompt-contract",
                        "why_selected": "captures claim-centric contract changes",
                        "excerpt": "@@ ...",
                        "region_type": "function",
                        "context_label": "build_triage_user_prompt",
                    },
                ],
            }
        ],
        "all_commits": [],
        "notes": [],
    }


class AIEmergencyTriageStageTests(unittest.TestCase):
    def test_run_stage_enforces_one_item_per_input_unit_id(self) -> None:
        payload = _sample_semantic_payload()
        unit_one = build_semantic_evidence_unit_id("tools", 0)
        unit_two = build_semantic_evidence_unit_id("tools", 1)
        fake = _FakeProvider(
            {
                "schema_version": AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
                "version": "0.34.1",
                "items": [
                    {
                        "id": unit_one,
                        "category": "tools",
                        "change_kind": "output-artifact",
                        "change_summary": "Added profile comparison artifact generation and wiring.",
                        "confidence": "high",
                    },
                    {
                        "id": unit_two,
                        "category": "tools",
                        "change_kind": "prompt-contract",
                        "change_summary": "Refined triage instructions for claim-centric output.",
                        "confidence": "medium",
                    },
                ],
            }
        )
        result = run_emergency_triage_stage(payload, provider=fake)
        self.assertEqual(result.schema_version, AI_EMERGENCY_TRIAGE_SCHEMA_VERSION)
        self.assertEqual(len(result.items), 2)
        call = fake.calls[0]
        self.assertEqual(call["stage"], "emergency_triage")
        self.assertIn("one item per input", call["user_prompt"])

    def test_build_synthesized_triage_input_payload(self) -> None:
        payload = _sample_semantic_payload()
        result = AIEmergencyTriageResult(
            schema_version=AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
            version="0.34.1",
            items=(
                AIEmergencyTriageItem(
                    id=build_semantic_evidence_unit_id("tools", 0),
                    category="tools",
                    change_kind="output-artifact",
                    change_summary="Added profile comparison artifact generation and wiring.",
                    confidence="high",
                    evidence_refs=("tools/release/cli.py#cmd_changelog_ai_compare",),
                ),
                AIEmergencyTriageItem(
                    id=build_semantic_evidence_unit_id("tools", 1),
                    category="tools",
                    change_kind="prompt-contract",
                    change_summary="Refined triage instructions for claim-centric output.",
                    confidence="medium",
                ),
            ),
        )
        triage_payload = build_triage_input_from_emergency_result(payload, result)
        self.assertEqual(triage_payload["schema_version"], AI_TRIAGE_SYNTHESIZED_INPUT_SCHEMA_VERSION)
        self.assertEqual(triage_payload["version"], "0.34.1")
        self.assertEqual(len(triage_payload["categories"]), 1)
        units = triage_payload["categories"][0]["synthesized_units"]
        self.assertEqual(len(units), 2)
        self.assertEqual(units[0]["source_kind"], "output-artifact")
        self.assertEqual(units[0]["source_path"], "tools/release/cli.py")


if __name__ == "__main__":
    unittest.main()
