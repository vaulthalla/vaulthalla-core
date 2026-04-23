from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from tools.release.changelog.ai.config import AIProviderKind

AI_TRIAGE_SCHEMA_VERSION = "vaulthalla.release.ai_triage.v1"
_OPTIONAL_TOP_LEVEL_STRING_ARRAY_FIELDS = ("dropped_noise", "caution_notes")
_OPTIONAL_CATEGORY_STRING_ARRAY_FIELDS = ("important_files", "retained_snippets", "caution_notes")

@dataclass(frozen=True)
class _TriageSchemaLimits:
    version_max_length: int
    summary_points_max_items: int
    summary_point_max_length: int
    categories_max_items: int
    category_name_max_length: int
    priority_rank_maximum: int
    key_points_max_items: int
    key_point_max_length: int
    important_files_max_items: int
    important_file_max_length: int
    retained_snippets_max_items: int
    retained_snippet_max_length: int
    category_caution_notes_max_items: int
    category_caution_note_max_length: int
    dropped_noise_max_items: int
    dropped_noise_max_length: int
    caution_notes_max_items: int
    caution_note_max_length: int


_DEFAULT_TRIAGE_SCHEMA_LIMITS = _TriageSchemaLimits(
    version_max_length=80,
    summary_points_max_items=8,
    summary_point_max_length=260,
    categories_max_items=10,
    category_name_max_length=60,
    priority_rank_maximum=20,
    key_points_max_items=6,
    key_point_max_length=260,
    important_files_max_items=6,
    important_file_max_length=240,
    retained_snippets_max_items=4,
    retained_snippet_max_length=420,
    category_caution_notes_max_items=4,
    category_caution_note_max_length=240,
    dropped_noise_max_items=12,
    dropped_noise_max_length=260,
    caution_notes_max_items=8,
    caution_note_max_length=260,
)

_HOSTED_COMPACT_TRIAGE_SCHEMA_LIMITS = _TriageSchemaLimits(
    version_max_length=40,
    summary_points_max_items=4,
    summary_point_max_length=180,
    categories_max_items=5,
    category_name_max_length=50,
    priority_rank_maximum=12,
    key_points_max_items=3,
    key_point_max_length=180,
    important_files_max_items=3,
    important_file_max_length=200,
    retained_snippets_max_items=1,
    retained_snippet_max_length=140,
    category_caution_notes_max_items=2,
    category_caution_note_max_length=180,
    dropped_noise_max_items=6,
    dropped_noise_max_length=180,
    caution_notes_max_items=4,
    caution_note_max_length=180,
)


def _build_triage_response_json_schema(limits: _TriageSchemaLimits) -> dict[str, Any]:
    return {
        "type": "object",
        "additionalProperties": False,
        "required": [
            "schema_version",
            "version",
            "summary_points",
            "categories",
            "dropped_noise",
            "caution_notes",
        ],
        "properties": {
            "schema_version": {"type": "string", "const": AI_TRIAGE_SCHEMA_VERSION},
            "version": {"type": "string", "minLength": 1, "maxLength": limits.version_max_length},
            "summary_points": {
                "type": "array",
                "minItems": 1,
                "maxItems": limits.summary_points_max_items,
                "items": {"type": "string", "minLength": 1, "maxLength": limits.summary_point_max_length},
            },
            "categories": {
                "type": "array",
                "minItems": 1,
                "maxItems": limits.categories_max_items,
                "items": {
                    "type": "object",
                    "additionalProperties": False,
                    "required": [
                        "name",
                        "signal_strength",
                        "priority_rank",
                        "key_points",
                        "important_files",
                        "retained_snippets",
                        "caution_notes",
                    ],
                    "properties": {
                        "name": {"type": "string", "minLength": 1, "maxLength": limits.category_name_max_length},
                        "signal_strength": {"type": "string", "enum": ["strong", "moderate", "weak"]},
                        "priority_rank": {"type": "integer", "minimum": 1, "maximum": limits.priority_rank_maximum},
                        "key_points": {
                            "type": "array",
                            "minItems": 1,
                            "maxItems": limits.key_points_max_items,
                            "items": {"type": "string", "minLength": 1, "maxLength": limits.key_point_max_length},
                        },
                        "important_files": {
                            "type": "array",
                            "maxItems": limits.important_files_max_items,
                            "items": {"type": "string", "minLength": 1, "maxLength": limits.important_file_max_length},
                        },
                        "retained_snippets": {
                            "type": "array",
                            "maxItems": limits.retained_snippets_max_items,
                            "items": {"type": "string", "minLength": 1, "maxLength": limits.retained_snippet_max_length},
                        },
                        "caution_notes": {
                            "type": "array",
                            "maxItems": limits.category_caution_notes_max_items,
                            "items": {
                                "type": "string",
                                "minLength": 1,
                                "maxLength": limits.category_caution_note_max_length,
                            },
                        },
                    },
                },
            },
            "dropped_noise": {
                "type": "array",
                "maxItems": limits.dropped_noise_max_items,
                "items": {"type": "string", "minLength": 1, "maxLength": limits.dropped_noise_max_length},
            },
            "caution_notes": {
                "type": "array",
                "maxItems": limits.caution_notes_max_items,
                "items": {"type": "string", "minLength": 1, "maxLength": limits.caution_note_max_length},
            },
        },
    }


AI_TRIAGE_RESPONSE_JSON_SCHEMA: dict[str, Any] = _build_triage_response_json_schema(
    _DEFAULT_TRIAGE_SCHEMA_LIMITS
)
AI_TRIAGE_HOSTED_COMPACT_RESPONSE_JSON_SCHEMA: dict[str, Any] = _build_triage_response_json_schema(
    _HOSTED_COMPACT_TRIAGE_SCHEMA_LIMITS
)


def resolve_triage_response_json_schema(
    *,
    provider_kind: AIProviderKind,
    model: str | None,
) -> dict[str, Any]:
    if provider_kind == "openai" and isinstance(model, str) and model.strip().lower().startswith("gpt-5"):
        return AI_TRIAGE_HOSTED_COMPACT_RESPONSE_JSON_SCHEMA
    return AI_TRIAGE_RESPONSE_JSON_SCHEMA


@dataclass(frozen=True)
class AITriageCategory:
    name: str
    signal_strength: str
    priority_rank: int
    key_points: tuple[str, ...]
    important_files: tuple[str, ...]
    retained_snippets: tuple[str, ...]
    caution_notes: tuple[str, ...] = ()


@dataclass(frozen=True)
class AITriageResult:
    schema_version: str
    version: str
    summary_points: tuple[str, ...]
    categories: tuple[AITriageCategory, ...]
    dropped_noise: tuple[str, ...] = ()
    caution_notes: tuple[str, ...] = ()


def parse_ai_triage_response(data: Any) -> AITriageResult:
    if not isinstance(data, dict):
        raise ValueError("AI triage response must be a JSON object.")
    normalized_data = _normalize_optional_string_arrays(data)

    schema_version = _read_non_empty_string(normalized_data, "schema_version")
    if schema_version != AI_TRIAGE_SCHEMA_VERSION:
        raise ValueError(
            f"`schema_version` must be {AI_TRIAGE_SCHEMA_VERSION!r}, got {schema_version!r}."
        )

    version = _read_non_empty_string(normalized_data, "version")
    summary_points = _read_non_empty_string_list(normalized_data, "summary_points", required=True)
    categories_raw = normalized_data.get("categories")
    if not isinstance(categories_raw, list) or not categories_raw:
        raise ValueError("AI triage response must include non-empty `categories` list.")

    seen_names: set[str] = set()
    seen_ranks: set[int] = set()
    categories: list[AITriageCategory] = []
    for index, category_raw in enumerate(categories_raw):
        if not isinstance(category_raw, dict):
            raise ValueError(f"AI triage category at index {index} must be an object.")

        path = f"categories[{index}]"
        name = _read_non_empty_string(category_raw, "name", path=path)
        if name in seen_names:
            raise ValueError(f"Duplicate triage category name `{name}` is not allowed.")
        seen_names.add(name)

        signal_strength = _read_non_empty_string(category_raw, "signal_strength", path=path)
        if signal_strength not in {"strong", "moderate", "weak"}:
            raise ValueError(
                f"`{path}.signal_strength` must be one of strong|moderate|weak."
            )

        priority_rank_raw = category_raw.get("priority_rank")
        if not isinstance(priority_rank_raw, int) or priority_rank_raw < 1:
            raise ValueError(f"`{path}.priority_rank` must be an integer >= 1.")
        if priority_rank_raw in seen_ranks:
            raise ValueError(f"Duplicate triage priority rank `{priority_rank_raw}` is not allowed.")
        seen_ranks.add(priority_rank_raw)

        key_points = _read_non_empty_string_list(category_raw, "key_points", path=path, required=True)
        important_files = _read_non_empty_string_list(category_raw, "important_files", path=path)
        retained_snippets = _read_non_empty_string_list(category_raw, "retained_snippets", path=path)
        caution_notes = _read_non_empty_string_list(category_raw, "caution_notes", path=path)

        categories.append(
            AITriageCategory(
                name=name,
                signal_strength=signal_strength,
                priority_rank=priority_rank_raw,
                key_points=tuple(key_points),
                important_files=tuple(important_files),
                retained_snippets=tuple(retained_snippets),
                caution_notes=tuple(caution_notes),
            )
        )

    categories.sort(key=lambda item: item.priority_rank)
    return AITriageResult(
        schema_version=schema_version,
        version=version,
        summary_points=tuple(summary_points),
        categories=tuple(categories),
        dropped_noise=tuple(_read_non_empty_string_list(normalized_data, "dropped_noise")),
        caution_notes=tuple(_read_non_empty_string_list(normalized_data, "caution_notes")),
    )


def ai_triage_result_to_dict(result: AITriageResult) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "schema_version": result.schema_version,
        "version": result.version,
        "summary_points": list(result.summary_points),
        "categories": [
            {
                "name": category.name,
                "signal_strength": category.signal_strength,
                "priority_rank": category.priority_rank,
                "key_points": list(category.key_points),
                "important_files": list(category.important_files),
                "retained_snippets": list(category.retained_snippets),
            }
            for category in result.categories
        ],
    }
    for index, category in enumerate(result.categories):
        if category.caution_notes:
            payload["categories"][index]["caution_notes"] = list(category.caution_notes)
    if result.dropped_noise:
        payload["dropped_noise"] = list(result.dropped_noise)
    if result.caution_notes:
        payload["caution_notes"] = list(result.caution_notes)
    return payload


def build_triage_ir_payload(result: AITriageResult) -> dict[str, Any]:
    """Return compact, deterministic draft-input representation from triage result."""
    return {
        "schema_version": result.schema_version,
        "version": result.version,
        "summary_points": list(result.summary_points),
        "categories": [
            {
                "name": category.name,
                "signal_strength": category.signal_strength,
                "priority_rank": category.priority_rank,
                "key_points": list(category.key_points),
                "important_files": list(category.important_files),
                "retained_snippets": list(category.retained_snippets),
                "caution_notes": list(category.caution_notes),
            }
            for category in result.categories
        ],
        "dropped_noise": list(result.dropped_noise),
        "caution_notes": list(result.caution_notes),
    }


def _read_non_empty_string(obj: dict[str, Any], key: str, path: str | None = None) -> str:
    value = obj.get(key)
    if not isinstance(value, str) or not value.strip():
        prefix = f"{path}." if path else ""
        raise ValueError(f"`{prefix}{key}` must be a non-empty string.")
    return value.strip()


def _read_non_empty_string_list(
    obj: dict[str, Any],
    key: str,
    *,
    path: str | None = None,
    required: bool = False,
) -> list[str]:
    raw = obj.get(key)
    prefix = f"{path}." if path else ""

    if raw is None:
        if required:
            raise ValueError(f"`{prefix}{key}` must be a non-empty list of strings.")
        return []
    if not isinstance(raw, list):
        raise ValueError(f"`{prefix}{key}` must be a list of strings.")
    if required and not raw:
        raise ValueError(f"`{prefix}{key}` must be a non-empty list of strings.")

    normalized: list[str] = []
    for index, value in enumerate(raw):
        if not isinstance(value, str):
            raise ValueError(f"`{prefix}{key}[{index}]` must be a non-empty string.")
        trimmed = value.strip()
        if not trimmed:
            if required:
                raise ValueError(f"`{prefix}{key}[{index}]` must be a non-empty string.")
            continue
        normalized.append(trimmed)
    if required and not normalized:
        raise ValueError(f"`{prefix}{key}` must be a non-empty list of strings.")
    return normalized


def _normalize_optional_string_arrays(data: dict[str, Any]) -> dict[str, Any]:
    normalized: dict[str, Any] = dict(data)
    for field in _OPTIONAL_TOP_LEVEL_STRING_ARRAY_FIELDS:
        if field in normalized:
            normalized[field] = _normalize_optional_string_array_value(normalized[field])

    categories_raw = normalized.get("categories")
    if isinstance(categories_raw, list):
        normalized_categories: list[Any] = []
        for category_raw in categories_raw:
            if not isinstance(category_raw, dict):
                normalized_categories.append(category_raw)
                continue
            category = dict(category_raw)
            for field in _OPTIONAL_CATEGORY_STRING_ARRAY_FIELDS:
                if field in category:
                    category[field] = _normalize_optional_string_array_value(category[field])
            normalized_categories.append(category)
        normalized["categories"] = normalized_categories
    return normalized


def _normalize_optional_string_array_value(raw: Any) -> Any:
    if raw is None:
        return []
    if not isinstance(raw, list):
        return raw
    normalized: list[str] = []
    for value in raw:
        if not isinstance(value, str):
            # Optional arrays can contain local-model placeholders; drop non-string entries.
            continue
        trimmed = value.strip()
        if trimmed:
            normalized.append(trimmed)
    return normalized
