from __future__ import annotations

import json
import os
from typing import Any

from tools.release.changelog.ai.config import (
    AIProviderKind,
    AIReasoningEffort,
    AIStructuredMode,
    DEFAULT_AI_DRAFT_MODEL,
    OPENAI_API_KEY_ENV_VAR,
)
from tools.release.changelog.ai.providers.capabilities import (
    ProviderCapabilities,
    build_structured_mode_fallback_chain,
    get_provider_capabilities,
    resolve_request_parameter_capabilities,
    resolve_generation_settings,
)
from tools.release.changelog.ai.providers.parsing import parse_json_object_from_text

LOCAL_NO_AUTH_API_KEY_PLACEHOLDER = "local-no-auth"
_MODE_RECOVERABLE_ERROR_MARKERS = (
    "response_format",
    "json_schema",
    "invalid schema",
    "strict",
    "unsupported",
    "not supported",
    "invalid request",
    "invalid_request_error",
    "unknown parameter",
    "unrecognized",
    "does not support",
)
_RESPONSES_TEXT_CONTENT_TYPES = frozenset(
    {
        "output_text",
        "text",
        "summary_text",
        "reasoning_text",
    }
)
_RESPONSES_JSON_CONTENT_FIELDS = (
    "json",
    "parsed",
    "output_json",
)


class OpenAIProvider:
    """Thin OpenAI transport adapter for one structured JSON generation pass."""

    def __init__(
        self,
        *,
        model: str = DEFAULT_AI_DRAFT_MODEL,
        provider_kind: AIProviderKind = "openai",
        api_key: str | None = None,
        api_key_env_var: str = OPENAI_API_KEY_ENV_VAR,
        base_url: str | None = None,
        timeout_seconds: float | None = None,
        require_api_key: bool = True,
        sdk_client: Any | None = None,
    ) -> None:
        self.model = model
        self.provider_kind = provider_kind
        self.base_url = base_url
        self.last_structured_mode_used: AIStructuredMode | None = None

        if sdk_client is not None:
            self._client = sdk_client
            return

        resolved_api_key = api_key or os.getenv(api_key_env_var)
        if not resolved_api_key and require_api_key:
            raise ValueError(
                f"{api_key_env_var} is not set. Export {api_key_env_var} to run `changelog ai-draft`."
            )
        if not resolved_api_key:
            # OpenAI-compatible local gateways may not require auth; keep a placeholder
            # so SDK client construction still succeeds when no key is provided.
            resolved_api_key = LOCAL_NO_AUTH_API_KEY_PLACEHOLDER

        self._client = _build_sdk_client(
            api_key=resolved_api_key,
            base_url=base_url,
            timeout_seconds=timeout_seconds,
        )

    def list_models(self) -> list[str]:
        try:
            response = self._client.models.list()
        except Exception as exc:
            raise ValueError(f"Model discovery request failed: {exc}") from exc

        data = response.get("data") if isinstance(response, dict) else getattr(response, "data", None)
        if not isinstance(data, list):
            raise ValueError("Model discovery response did not include a valid `data` list.")

        model_ids: list[str] = []
        for item in data:
            model_id = item.get("id") if isinstance(item, dict) else getattr(item, "id", None)
            if isinstance(model_id, str) and model_id.strip():
                model_ids.append(model_id.strip())
        return sorted(set(model_ids))

    def generate_structured_json(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        json_schema: dict[str, Any],
        reasoning_effort: AIReasoningEffort | None = None,
        structured_mode: AIStructuredMode | None = None,
        temperature: float | None = None,
        max_output_tokens: int | None = None,
    ) -> dict[str, Any]:
        self.last_structured_mode_used = None
        settings = resolve_generation_settings(
            provider_kind=self.provider_kind,
            requested_structured_mode=structured_mode,
            requested_reasoning_effort=reasoning_effort,
        )
        mode_chain = build_structured_mode_fallback_chain(settings.structured_mode)
        errors: list[str] = []

        for mode_index, mode in enumerate(mode_chain):
            try:
                content = self._generate_text_output(
                    system_prompt=system_prompt,
                    user_prompt=user_prompt,
                    json_schema=json_schema,
                    structured_mode=mode,
                    reasoning_effort=settings.reasoning_effort,
                    temperature=temperature,
                    max_output_tokens=max_output_tokens,
                )
                self.last_structured_mode_used = mode
                return parse_json_object_from_text(content)
            except Exception as exc:
                message = str(exc)
                mode_label = f"{self.provider_kind}:{mode}"
                errors.append(f"{mode_label}: {message}")
                if message.startswith("AI provider refusal:"):
                    raise ValueError(message) from exc
                has_next_mode = mode_index < len(mode_chain) - 1
                if has_next_mode and _is_mode_recoverable_error(message):
                    continue
                break

        attempted = ", ".join(mode_chain)
        details = " | ".join(errors[-2:])
        raise ValueError(
            f"AI generation failed after structured-mode attempts [{attempted}]. {details}"
        )

    def capabilities(self) -> ProviderCapabilities:
        return get_provider_capabilities(self.provider_kind)

    def _generate_text_output(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        json_schema: dict[str, Any],
        structured_mode: AIStructuredMode,
        reasoning_effort: AIReasoningEffort | None,
        temperature: float | None,
        max_output_tokens: int | None,
    ) -> str:
        if self.provider_kind == "openai":
            return self._generate_hosted_openai_output(
                system_prompt=system_prompt,
                user_prompt=user_prompt,
                json_schema=json_schema,
                structured_mode=structured_mode,
                reasoning_effort=reasoning_effort,
                temperature=temperature,
                max_output_tokens=max_output_tokens,
            )
        return self._generate_openai_compatible_output(
            system_prompt=system_prompt,
            user_prompt=user_prompt,
            json_schema=json_schema,
            structured_mode=structured_mode,
            reasoning_effort=reasoning_effort,
            temperature=temperature,
            max_output_tokens=max_output_tokens,
        )

    def _generate_hosted_openai_output(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        json_schema: dict[str, Any],
        structured_mode: AIStructuredMode,
        reasoning_effort: AIReasoningEffort | None,
        temperature: float | None,
        max_output_tokens: int | None,
    ) -> str:
        responses = getattr(self._client, "responses", None)
        create = getattr(responses, "create", None)
        if not callable(create):
            raise ValueError(
                "AI transport error: Hosted OpenAI provider requires Responses API support on the active OpenAI SDK/client."
            )
        parameter_capabilities = resolve_request_parameter_capabilities(
            provider_kind="openai",
            model=self.model,
        )
        try:
            response = create(
                **self._build_responses_request(
                    model=self.model,
                    system_prompt=system_prompt,
                    user_prompt=user_prompt,
                    json_schema=json_schema,
                    structured_mode=structured_mode,
                    reasoning_effort=reasoning_effort,
                    temperature=temperature,
                    max_output_tokens=max_output_tokens,
                    include_temperature=parameter_capabilities.supports_temperature,
                )
            )
        except Exception as exc:
            raise ValueError(f"AI transport error: Responses API request failed: {exc}") from exc
        return _extract_responses_output_text(response)

    def _generate_openai_compatible_output(
        self,
        *,
        system_prompt: str,
        user_prompt: str,
        json_schema: dict[str, Any],
        structured_mode: AIStructuredMode,
        reasoning_effort: AIReasoningEffort | None,
        temperature: float | None,
        max_output_tokens: int | None,
    ) -> str:
        try:
            response = self._client.chat.completions.create(
                **self._build_chat_request(
                    model=self.model,
                    system_prompt=system_prompt,
                    user_prompt=user_prompt,
                    json_schema=json_schema,
                    structured_mode=structured_mode,
                    reasoning_effort=reasoning_effort,
                    temperature=temperature,
                    max_output_tokens=max_output_tokens,
                )
            )
        except Exception as exc:
            raise ValueError(f"AI transport error: Chat Completions request failed: {exc}") from exc
        return _extract_message_content(response)

    @staticmethod
    def _build_chat_request(
        *,
        model: str,
        system_prompt: str,
        user_prompt: str,
        json_schema: dict[str, Any],
        structured_mode: AIStructuredMode,
        reasoning_effort: AIReasoningEffort | None,
        temperature: float | None,
        max_output_tokens: int | None,
    ) -> dict[str, Any]:
        messages = [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ]
        request: dict[str, Any] = {
            "model": model,
            "messages": messages,
        }

        if structured_mode == "strict_json_schema":
            request["response_format"] = {
                "type": "json_schema",
                "json_schema": {
                    "name": "vaulthalla_release_changelog_draft",
                    "schema": json_schema,
                    "strict": True,
                },
            }
        elif structured_mode == "json_object":
            request["response_format"] = {"type": "json_object"}
        else:
            messages[0]["content"] = (
                f"{system_prompt}\n\nReturn only a valid JSON object with no prose or markdown."
            )

        if reasoning_effort is not None:
            request["reasoning"] = {"effort": reasoning_effort}
        if temperature is not None:
            request["temperature"] = temperature
        if max_output_tokens is not None:
            request["max_completion_tokens"] = max_output_tokens

        return request

    @staticmethod
    def _build_responses_request(
        *,
        model: str,
        system_prompt: str,
        user_prompt: str,
        json_schema: dict[str, Any],
        structured_mode: AIStructuredMode,
        reasoning_effort: AIReasoningEffort | None,
        temperature: float | None,
        max_output_tokens: int | None,
        include_temperature: bool,
    ) -> dict[str, Any]:
        effective_system_prompt = system_prompt
        if structured_mode == "prompt_json":
            effective_system_prompt = (
                f"{system_prompt}\n\nReturn only a valid JSON object with no prose or markdown."
            )

        request: dict[str, Any] = {
            "model": model,
            "input": [
                {
                    "role": "system",
                    "content": [{"type": "input_text", "text": effective_system_prompt}],
                },
                {
                    "role": "user",
                    "content": [{"type": "input_text", "text": user_prompt}],
                },
            ],
        }

        if structured_mode == "strict_json_schema":
            request["text"] = {
                "format": {
                    "type": "json_schema",
                    "name": "vaulthalla_release_changelog_draft",
                    "schema": json_schema,
                    "strict": True,
                }
            }
        elif structured_mode == "json_object":
            request["text"] = {"format": {"type": "json_object"}}

        if reasoning_effort is not None:
            request["reasoning"] = {"effort": reasoning_effort}
        if include_temperature and temperature is not None:
            request["temperature"] = temperature
        if max_output_tokens is not None:
            request["max_output_tokens"] = max_output_tokens

        return request


def _build_sdk_client(
    *,
    api_key: str,
    base_url: str | None = None,
    timeout_seconds: float | None = None,
) -> Any:
    try:
        from openai import OpenAI
    except Exception as exc:
        raise ValueError(
            "OpenAI SDK is not available in this environment. Install `openai` in the active venv."
        ) from exc
    kwargs: dict[str, Any] = {"api_key": api_key}
    if base_url:
        kwargs["base_url"] = base_url
    if timeout_seconds is not None:
        kwargs["timeout"] = timeout_seconds
    return OpenAI(**kwargs)


def _extract_message_content(response: Any) -> str:
    choices = getattr(response, "choices", None)
    if not choices:
        raise ValueError("AI parse error: OpenAI response contained no choices.")

    message = getattr(choices[0], "message", None)
    if message is None:
        raise ValueError("AI parse error: OpenAI response choice contained no message.")

    refusal = getattr(message, "refusal", None)
    if refusal:
        raise ValueError(f"AI provider refusal: {refusal}")

    content = getattr(message, "content", None)
    if isinstance(content, str) and content.strip():
        return content

    # Some SDK variants may return content blocks instead of a raw string.
    if isinstance(content, list):
        blocks: list[str] = []
        for block in content:
            if isinstance(block, dict):
                text = block.get("text")
            else:
                text = getattr(block, "text", None)
            if isinstance(text, str) and text.strip():
                blocks.append(text)
        if blocks:
            return "".join(blocks)

    raise ValueError("AI parse error: OpenAI response did not include structured JSON content.")


def _extract_responses_output_text(response: Any) -> str:
    output_text = _extract_text_candidate(_read_response_field(response, "output_text"))
    if output_text is not None:
        return output_text

    output_items = _as_response_items(_read_response_field(response, "output"))
    text_chunks: list[str] = []
    text_seen: set[str] = set()
    json_chunks: list[str] = []
    json_seen: set[str] = set()
    refusal_chunks: list[str] = []
    item_types: set[str] = set()
    content_types: set[str] = set()

    for item in output_items:
        item_type = _response_type_label(_read_response_field(item, "type"))
        if item_type is not None:
            item_types.add(item_type)

        item_refusal = _extract_text_candidate(_read_response_field(item, "refusal"))
        if item_type == "refusal" or item_refusal is not None:
            refusal_chunks.append(item_refusal or "request refused")

        _append_unique(
            text_chunks,
            text_seen,
            _extract_text_candidate(_read_response_field(item, "text")),
        )
        _append_unique(
            text_chunks,
            text_seen,
            _extract_text_candidate(_read_response_field(item, "output_text")),
        )
        for field in _RESPONSES_JSON_CONTENT_FIELDS:
            _append_unique(
                json_chunks,
                json_seen,
                _extract_json_candidate(_read_response_field(item, field)),
            )

        for content_item in _as_response_items(_read_response_field(item, "content")):
            content_type = _response_type_label(_read_response_field(content_item, "type"))
            if content_type is not None:
                content_types.add(content_type)

            content_refusal = _extract_text_candidate(_read_response_field(content_item, "refusal"))
            if content_type == "refusal" or content_refusal is not None:
                refusal_chunks.append(content_refusal or "request refused")

            _append_unique(
                text_chunks,
                text_seen,
                _extract_text_candidate(_read_response_field(content_item, "text")),
            )
            _append_unique(
                text_chunks,
                text_seen,
                _extract_text_candidate(_read_response_field(content_item, "output_text")),
            )
            _append_unique(
                text_chunks,
                text_seen,
                _extract_text_candidate(_read_response_field(content_item, "summary")),
            )
            _append_unique(
                text_chunks,
                text_seen,
                _extract_text_candidate(_read_response_field(content_item, "reasoning")),
            )
            if content_type in _RESPONSES_TEXT_CONTENT_TYPES:
                _append_unique(
                    text_chunks,
                    text_seen,
                    _extract_text_candidate(content_item),
                )

            for field in _RESPONSES_JSON_CONTENT_FIELDS:
                _append_unique(
                    json_chunks,
                    json_seen,
                    _extract_json_candidate(_read_response_field(content_item, field)),
                )

    if text_chunks:
        return "".join(text_chunks)
    if json_chunks:
        return json_chunks[0]

    refusal_only = bool(refusal_chunks)
    if refusal_only:
        refusal_text = " | ".join(chunk for chunk in refusal_chunks if chunk.strip())
        raise ValueError(f"AI provider refusal: {refusal_text or 'request refused'}")

    raise ValueError(
        _format_responses_parse_error(
            item_types=item_types,
            content_types=content_types,
            saw_refusal=bool(refusal_chunks),
            saw_output=bool(output_items),
        )
    )


def _read_response_field(obj: Any, key: str) -> Any:
    if isinstance(obj, dict):
        return obj.get(key)
    return getattr(obj, key, None)


def _as_response_items(value: Any) -> list[Any]:
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        return list(value)
    return [value]


def _response_type_label(value: Any) -> str | None:
    if value is None:
        return None
    if isinstance(value, str):
        label = value.strip()
        return label or None
    return str(value)


def _extract_text_candidate(value: Any, *, depth: int = 0) -> str | None:
    if value is None or depth > 5:
        return None
    if isinstance(value, str):
        text = value.strip()
        return text if text else None
    if isinstance(value, (list, tuple)):
        chunks = [
            chunk
            for chunk in (_extract_text_candidate(item, depth=depth + 1) for item in value)
            if chunk is not None
        ]
        if chunks:
            return "".join(chunks)
        return None
    if isinstance(value, dict):
        for key in (
            "value",
            "text",
            "output_text",
            "summary_text",
            "reasoning_text",
            "summary",
            "reasoning",
            "content",
            "message",
        ):
            nested = value.get(key)
            extracted = _extract_text_candidate(nested, depth=depth + 1)
            if extracted is not None:
                return extracted
        return None

    for attr in (
        "value",
        "text",
        "output_text",
        "summary_text",
        "reasoning_text",
        "summary",
        "reasoning",
        "content",
        "message",
    ):
        nested = getattr(value, attr, None)
        if nested is None:
            continue
        extracted = _extract_text_candidate(nested, depth=depth + 1)
        if extracted is not None:
            return extracted
    return None


def _extract_json_candidate(value: Any) -> str | None:
    if value is None:
        return None
    if isinstance(value, (dict, list)):
        return json.dumps(value, ensure_ascii=False)
    model_dump = getattr(value, "model_dump", None)
    if callable(model_dump):
        try:
            dumped = model_dump(exclude_none=True)
        except TypeError:
            dumped = model_dump()
        if isinstance(dumped, (dict, list)):
            return json.dumps(dumped, ensure_ascii=False)
    return None


def _append_unique(buffer: list[str], seen: set[str], value: str | None) -> None:
    if value is None:
        return
    if value in seen:
        return
    seen.add(value)
    buffer.append(value)


def _format_responses_parse_error(
    *,
    item_types: set[str],
    content_types: set[str],
    saw_refusal: bool,
    saw_output: bool,
) -> str:
    item_types_label = ", ".join(sorted(item_types)) if item_types else "<none>"
    content_types_label = ", ".join(sorted(content_types)) if content_types else "<none>"
    return (
        "AI parse error: Responses API output had no extractable text or JSON content "
        f"(saw_output={saw_output}, item_types={item_types_label}, "
        f"content_types={content_types_label}, saw_refusal={saw_refusal})."
    )


def _is_mode_recoverable_error(message: str) -> bool:
    lower = message.lower()
    return any(marker in lower for marker in _MODE_RECOVERABLE_ERROR_MARKERS) or "ai parse error" in lower
