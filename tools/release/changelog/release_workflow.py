from __future__ import annotations

import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Literal, Mapping

from tools.release.changelog.ai.config import (
    AIPipelineCLIOverrides,
    AIPipelineConfig,
    AIProviderConfig,
    AIStageName,
    resolve_ai_pipeline_config,
)
from tools.release.changelog.ai.contracts.polish import AIPolishResult
from tools.release.changelog.ai.contracts.triage import build_triage_ir_payload
from tools.release.changelog.ai.providers import build_structured_json_provider, run_provider_preflight
from tools.release.changelog.ai.providers.base import StructuredJSONProvider
from tools.release.changelog.ai.render.markdown import render_draft_markdown, render_polish_markdown
from tools.release.changelog.ai.stages.draft import generate_draft_from_payload
from tools.release.changelog.ai.stages.polish import run_polish_stage
from tools.release.changelog.ai.stages.triage import run_triage_stage
from tools.release.version.adapters.debian import CHANGELOG_HEADER_PATTERN, parse_debian_version
from tools.release.version.adapters.version_file import read_version_file

ReleaseAIMode = Literal["auto", "openai-only", "local-only", "disabled"]
ReleaseChangelogPath = Literal["openai", "local", "manual"]

DEFAULT_RELEASE_AI_MODE: ReleaseAIMode = "auto"
DEFAULT_RELEASE_OPENAI_PROFILE = "openai-balanced"


@dataclass(frozen=True)
class ReleaseAISettings:
    mode: ReleaseAIMode
    openai_profile: str
    openai_api_key_present: bool
    local_enabled: bool
    local_profile: str | None
    local_base_url_override: str | None
    local_api_key: str | None


@dataclass(frozen=True)
class ManualChangelogValidation:
    path: Path
    version: str
    detected_target: str
    content: str


@dataclass(frozen=True)
class ReleaseChangelogSelection:
    path: ReleaseChangelogPath
    content: str
    local_base_url_overrode_profile: bool = False


def parse_release_ai_settings(environ: Mapping[str, str] | None = None) -> ReleaseAISettings:
    env = dict(os.environ if environ is None else environ)

    mode_raw = env.get("RELEASE_AI_MODE", DEFAULT_RELEASE_AI_MODE)
    mode = _parse_release_ai_mode(mode_raw)
    openai_profile = _normalize_or_default(env.get("RELEASE_AI_PROFILE_OPENAI"), DEFAULT_RELEASE_OPENAI_PROFILE)
    openai_api_key_present = bool(_normalize_optional_string(env.get("OPENAI_API_KEY")))
    local_enabled = _parse_bool(env.get("RELEASE_LOCAL_LLM_ENABLED"), var_name="RELEASE_LOCAL_LLM_ENABLED", default=False)
    local_profile = _normalize_optional_string(env.get("RELEASE_LOCAL_LLM_PROFILE"))
    local_base_url_override = _normalize_optional_string(env.get("RELEASE_LOCAL_LLM_BASE_URL"))
    local_api_key = _normalize_optional_string(env.get("RELEASE_LOCAL_LLM_API_KEY"))

    return ReleaseAISettings(
        mode=mode,
        openai_profile=openai_profile,
        openai_api_key_present=openai_api_key_present,
        local_enabled=local_enabled,
        local_profile=local_profile,
        local_base_url_override=local_base_url_override,
        local_api_key=local_api_key,
    )


def resolve_release_changelog(
    *,
    repo_root: Path,
    payload: dict,
    settings: ReleaseAISettings,
    manual_changelog_path: Path | str = Path("debian/changelog"),
    logger: Callable[[str], None] | None = None,
) -> ReleaseChangelogSelection:
    emit = logger or (lambda _line: None)
    emit(f"Release changelog mode: {settings.mode}")
    emit(f"OpenAI profile: {settings.openai_profile}")
    emit(f"OpenAI API key present: {'yes' if settings.openai_api_key_present else 'no'}")
    emit(f"Local LLM fallback enabled: {'yes' if settings.local_enabled else 'no'}")
    if settings.local_profile:
        emit(f"Local LLM profile: {settings.local_profile}")
    if settings.local_base_url_override:
        emit(f"Local LLM base URL override from env: {settings.local_base_url_override}")

    for candidate in _release_candidate_order(settings.mode):
        if candidate == "openai":
            can_use_openai, reason = _can_attempt_openai(settings)
            if not can_use_openai:
                emit(f"Skipping OpenAI path: {reason}")
                continue
            try:
                content = _generate_openai_release_changelog(
                    repo_root=repo_root,
                    payload=payload,
                    settings=settings,
                    logger=emit,
                )
            except Exception as exc:
                emit(f"OpenAI path failed, falling back: {exc}")
                continue
            emit("Selected changelog path: OpenAI")
            return ReleaseChangelogSelection(path="openai", content=content)

        if candidate == "local":
            can_use_local, reason = _can_attempt_local(settings)
            if not can_use_local:
                emit(f"Skipping local LLM path: {reason}")
                continue
            try:
                content, used_override = _generate_local_release_changelog(
                    repo_root=repo_root,
                    payload=payload,
                    settings=settings,
                    logger=emit,
                )
            except Exception as exc:
                emit(f"Local LLM path failed, falling back: {exc}")
                continue
            emit("Selected changelog path: local LLM")
            return ReleaseChangelogSelection(
                path="local",
                content=content,
                local_base_url_overrode_profile=used_override,
            )

        validation = validate_manual_changelog_current(
            repo_root=repo_root,
            changelog_path=manual_changelog_path,
        )
        emit("Selected changelog path: manual/no-AI")
        emit(
            "Manual changelog stale check passed: "
            f"{validation.path} targets version {validation.detected_target} (VERSION={validation.version})."
        )
        return ReleaseChangelogSelection(path="manual", content=validation.content)

    # The candidate order always ends with manual.
    raise ValueError("No release changelog strategy could be resolved.")


def resolve_local_release_pipeline_config(
    *,
    repo_root: Path,
    profile_slug: str,
    base_url_override: str | None,
) -> tuple[AIPipelineConfig, str | None, bool]:
    base_pipeline = resolve_ai_pipeline_config(
        repo_root=repo_root,
        profile_slug=profile_slug,
        cli_overrides=AIPipelineCLIOverrides(provider="openai-compatible"),
    )
    resolved_pipeline = resolve_ai_pipeline_config(
        repo_root=repo_root,
        profile_slug=profile_slug,
        cli_overrides=AIPipelineCLIOverrides(
            provider="openai-compatible",
            base_url=base_url_override,
        ),
    )
    normalized_override = _normalize_optional_string(base_url_override)
    override_applied = normalized_override is not None and resolved_pipeline.base_url == normalized_override
    return resolved_pipeline, base_pipeline.base_url, override_applied


def validate_manual_changelog_current(
    *,
    repo_root: Path,
    changelog_path: Path | str,
) -> ManualChangelogValidation:
    version_path = repo_root / "VERSION"
    version = read_version_file(version_path)
    target_path = Path(changelog_path)
    if not target_path.is_absolute():
        target_path = (repo_root / target_path).resolve()
    if not target_path.is_file():
        raise ValueError(
            "Manual changelog validation failed: "
            f"required file is missing at {target_path}."
        )

    content = target_path.read_text(encoding="utf-8")
    first_non_empty_line = ""
    for line in content.splitlines():
        stripped = line.strip()
        if stripped:
            first_non_empty_line = stripped
            break
    if not first_non_empty_line:
        raise ValueError(f"Manual changelog validation failed: changelog is empty: {target_path}")

    header_match = CHANGELOG_HEADER_PATTERN.fullmatch(first_non_empty_line)
    if header_match:
        detected = parse_debian_version(header_match.group("full_version")).upstream
        if detected != version:
            raise ValueError(
                "Manual changelog validation failed: stale changelog. "
                f"{target_path} targets {detected} but VERSION is {version}."
            )
        return ManualChangelogValidation(
            path=target_path,
            version=str(version),
            detected_target=str(detected),
            content=content,
        )

    token = re.escape(str(version))
    if not re.search(rf"\b{token}\b", content):
        raise ValueError(
            "Manual changelog validation failed: stale changelog. "
            f"{target_path} does not mention VERSION {version}."
        )
    return ManualChangelogValidation(
        path=target_path,
        version=str(version),
        detected_target=str(version),
        content=content,
    )


def _release_candidate_order(mode: ReleaseAIMode) -> tuple[ReleaseChangelogPath, ...]:
    if mode == "auto":
        return ("openai", "local", "manual")
    if mode == "openai-only":
        return ("openai", "manual")
    if mode == "local-only":
        return ("local", "manual")
    return ("manual",)


def _can_attempt_openai(settings: ReleaseAISettings) -> tuple[bool, str]:
    if not settings.openai_api_key_present:
        return False, "OPENAI_API_KEY is not set."
    if not settings.openai_profile:
        return False, "RELEASE_AI_PROFILE_OPENAI is not set."
    return True, "ok"


def _can_attempt_local(settings: ReleaseAISettings) -> tuple[bool, str]:
    if not settings.local_enabled:
        return False, "RELEASE_LOCAL_LLM_ENABLED is not true."
    if not settings.local_profile:
        return False, "RELEASE_LOCAL_LLM_PROFILE is not set."
    return True, "ok"


def _generate_openai_release_changelog(
    *,
    repo_root: Path,
    payload: dict,
    settings: ReleaseAISettings,
    logger: Callable[[str], None],
) -> str:
    pipeline = resolve_ai_pipeline_config(
        repo_root=repo_root,
        profile_slug=settings.openai_profile,
        cli_overrides=AIPipelineCLIOverrides(provider="openai"),
    )
    logger(f"Attempting OpenAI profile `{settings.openai_profile}`.")
    return _run_release_ai_pipeline(
        payload=payload,
        pipeline=pipeline,
        logger=logger,
    )


def _generate_local_release_changelog(
    *,
    repo_root: Path,
    payload: dict,
    settings: ReleaseAISettings,
    logger: Callable[[str], None],
) -> tuple[str, bool]:
    assert settings.local_profile is not None
    pipeline, profile_base_url, override_applied = resolve_local_release_pipeline_config(
        repo_root=repo_root,
        profile_slug=settings.local_profile,
        base_url_override=settings.local_base_url_override,
    )
    if settings.local_base_url_override:
        logger(
            "Local LLM base URL override active via RELEASE_LOCAL_LLM_BASE_URL: "
            f"{pipeline.base_url} (profile base_url={profile_base_url or '<none>'})."
        )
    else:
        logger(f"Local LLM base URL from profile/default: {pipeline.base_url or '<provider-default>'}.")

    if settings.local_api_key:
        logger("Using RELEASE_LOCAL_LLM_API_KEY for local OpenAI-compatible gateway authentication.")
    logger(f"Attempting local profile `{settings.local_profile}`.")
    content = _run_release_ai_pipeline(
        payload=payload,
        pipeline=pipeline,
        local_api_key=settings.local_api_key,
        logger=logger,
    )
    return content, override_applied


def _run_release_ai_pipeline(
    *,
    payload: dict,
    pipeline: AIPipelineConfig,
    local_api_key: str | None = None,
    logger: Callable[[str], None],
) -> str:
    run_triage = pipeline.is_stage_enabled("triage")
    run_polish = pipeline.is_stage_enabled("polish")

    providers = _build_stage_providers(
        pipeline=pipeline,
        include_triage=run_triage,
        include_polish=run_polish,
        local_api_key=local_api_key if pipeline.provider == "openai-compatible" else None,
        logger=logger,
    )

    draft_input: dict = payload
    source_kind = "payload"
    triage_cfg = pipeline.stages["triage"]
    draft_cfg = pipeline.stages["draft"]
    polish_cfg = pipeline.stages["polish"]

    if run_triage:
        try:
            triage_result = run_triage_stage(
                payload,
                provider=providers["triage"],
                provider_kind=pipeline.provider,
                reasoning_effort=triage_cfg.reasoning_effort,
                structured_mode=triage_cfg.structured_mode,
                temperature=triage_cfg.temperature,
                max_output_tokens_policy=triage_cfg.max_output_tokens,
            )
        except Exception as exc:
            raise ValueError(f"Triage stage failed: {exc}") from exc
        draft_input = build_triage_ir_payload(triage_result)
        source_kind = "triage"

    try:
        draft = generate_draft_from_payload(
            draft_input,
            provider=providers["draft"],
            source_kind=source_kind,
            provider_kind=pipeline.provider,
            reasoning_effort=draft_cfg.reasoning_effort,
            structured_mode=draft_cfg.structured_mode,
            temperature=draft_cfg.temperature,
            max_output_tokens_policy=draft_cfg.max_output_tokens,
        )
    except Exception as exc:
        raise ValueError(f"Draft stage failed: {exc}") from exc

    polish_result: AIPolishResult | None = None
    if run_polish:
        try:
            polish_result = run_polish_stage(
                draft,
                provider=providers["polish"],
                provider_kind=pipeline.provider,
                reasoning_effort=polish_cfg.reasoning_effort,
                structured_mode=polish_cfg.structured_mode,
                temperature=polish_cfg.temperature,
                max_output_tokens_policy=polish_cfg.max_output_tokens,
            )
        except Exception as exc:
            raise ValueError(f"Polish stage failed: {exc}") from exc

    if polish_result is not None:
        return render_polish_markdown(polish_result)
    return render_draft_markdown(draft)


def _build_stage_providers(
    *,
    pipeline: AIPipelineConfig,
    include_triage: bool,
    include_polish: bool,
    local_api_key: str | None,
    logger: Callable[[str], None],
) -> dict[AIStageName, StructuredJSONProvider]:
    stage_order: list[AIStageName] = []
    if include_triage:
        stage_order.append("triage")
    stage_order.append("draft")
    if include_polish:
        stage_order.append("polish")

    providers_by_fingerprint: dict[tuple[str, str, str | None, str | None], StructuredJSONProvider] = {}
    stage_providers: dict[AIStageName, StructuredJSONProvider] = {}

    for stage in stage_order:
        stage_cfg = pipeline.stages[stage]
        provider_cfg = AIProviderConfig(
            kind=pipeline.provider,
            model=stage_cfg.model,
            base_url=pipeline.base_url,
            api_key=local_api_key,
        )
        fingerprint = (
            provider_cfg.kind,
            provider_cfg.model,
            provider_cfg.base_url,
            provider_cfg.api_key,
        )
        provider = providers_by_fingerprint.get(fingerprint)
        if provider is None:
            provider = build_structured_json_provider(provider_cfg)
            _ = run_provider_preflight(provider_cfg, provider=provider, require_model=True)
            providers_by_fingerprint[fingerprint] = provider
            endpoint = provider_cfg.base_url or "<hosted>"
            logger(
                "AI preflight OK: "
                f"provider={provider_cfg.kind}, model={provider_cfg.model}, endpoint={endpoint}"
            )
        stage_providers[stage] = provider

    return stage_providers


def _parse_release_ai_mode(raw: str | None) -> ReleaseAIMode:
    normalized = _normalize_or_default(raw, DEFAULT_RELEASE_AI_MODE)
    if normalized not in {"auto", "openai-only", "local-only", "disabled"}:
        raise ValueError(
            "Invalid RELEASE_AI_MODE. Expected one of: auto, openai-only, local-only, disabled."
        )
    return normalized  # type: ignore[return-value]


def _parse_bool(raw: str | None, *, var_name: str, default: bool) -> bool:
    normalized = _normalize_optional_string(raw)
    if normalized is None:
        return default
    truthy = {"1", "true", "yes", "on"}
    falsy = {"0", "false", "no", "off"}
    lower = normalized.lower()
    if lower in truthy:
        return True
    if lower in falsy:
        return False
    raise ValueError(f"Invalid {var_name}: expected true/false.")


def _normalize_optional_string(raw: str | None) -> str | None:
    if raw is None:
        return None
    normalized = raw.strip()
    return normalized or None


def _normalize_or_default(raw: str | None, default: str) -> str:
    normalized = _normalize_optional_string(raw)
    return normalized if normalized is not None else default

