from __future__ import annotations

import unittest

from tools.release.changelog.ai.contracts.emergency_triage import (
    AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
    AIEmergencyTriageResult,
    ai_emergency_triage_result_to_dict,
    parse_ai_emergency_triage_response,
)


class AIEmergencyTriageContractsTests(unittest.TestCase):
    def test_parse_valid_response_with_expected_ids(self) -> None:
        raw = {
            "schema_version": AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
            "version": "0.34.1",
            "items": [
                {
                    "id": "tools:1",
                    "category": "tools",
                    "change_kind": "api-contract",
                    "change_summary": "Updated request/response payload shaping and parsing.",
                    "confidence": "high",
                    "evidence_refs": ["tools/release/changelog/ai/providers/openai.py"],
                },
                {
                    "id": "tools:2",
                    "category": "tools",
                    "change_kind": "output-artifact",
                    "change_summary": "Added comparison artifact writing for profile runs.",
                    "confidence": "medium",
                },
            ],
        }
        parsed = parse_ai_emergency_triage_response(
            raw,
            expected_item_ids=("tools:1", "tools:2"),
        )
        self.assertIsInstance(parsed, AIEmergencyTriageResult)
        self.assertEqual(parsed.version, "0.34.1")
        self.assertEqual(parsed.items[0].id, "tools:1")
        self.assertEqual(parsed.items[1].change_kind, "output-artifact")

    def test_parse_rejects_identity_drift(self) -> None:
        raw = {
            "schema_version": AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
            "version": "0.34.1",
            "items": [
                {
                    "id": "tools:1",
                    "category": "tools",
                    "change_kind": "api-contract",
                    "change_summary": "Updated request/response payload shaping and parsing.",
                    "confidence": "high",
                }
            ],
        }
        with self.assertRaisesRegex(ValueError, "preserve 1:1 item identity"):
            parse_ai_emergency_triage_response(raw, expected_item_ids=("tools:1", "tools:2"))

    def test_to_dict_omits_optional_fields_when_empty(self) -> None:
        parsed = parse_ai_emergency_triage_response(
            {
                "schema_version": AI_EMERGENCY_TRIAGE_SCHEMA_VERSION,
                "version": "0.34.1",
                "items": [
                    {
                        "id": "tools:1",
                        "category": "tools",
                        "change_kind": "api-contract",
                        "change_summary": "Updated request/response payload shaping and parsing.",
                        "confidence": "high",
                    }
                ],
            }
        )
        rendered = ai_emergency_triage_result_to_dict(parsed)
        self.assertNotIn("insufficient_context_reason", rendered["items"][0])
        self.assertNotIn("evidence_refs", rendered["items"][0])


if __name__ == "__main__":
    unittest.main()
