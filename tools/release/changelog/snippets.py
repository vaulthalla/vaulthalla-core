from __future__ import annotations

from dataclasses import dataclass
import re
from pathlib import Path

from tools.release.changelog.git_collect import get_file_patch
from tools.release.changelog.models import CategoryContext, DiffSnippet, FileChange
from tools.release.changelog.scoring import is_semantic_noise_path, score_patch_hunk

try:
    from unidiff import PatchSet
except ModuleNotFoundError:  # pragma: no cover - exercised by environments without optional dependency.
    PatchSet = None

UNIDIFF_AVAILABLE = PatchSet is not None

HUNK_HEADER_RE = re.compile(r"^@@ .* @@.*$", re.MULTILINE)
_MAX_ADJACENT_HUNK_GAP = 8
_MAX_GROUPED_HUNKS = 3


@dataclass(frozen=True)
class _ParsedHunk:
    text: str
    source_start: int | None
    source_length: int | None
    target_start: int | None
    target_length: int | None


@dataclass(frozen=True)
class _EvidenceUnit:
    text: str
    hunk_count: int
    changed_line_count: int
    meaningful_line_count: int


def split_patch_into_hunks(patch: str) -> list[str]:
    return [hunk.text for hunk in _parse_patch_hunks(patch)]


def build_snippet_reason(
    file_change: FileChange,
    hunk: str,
    category: str,
    *,
    hunk_count: int = 1,
    changed_lines: int | None = None,
) -> str:
    del hunk  # Reason currently depends on file-level signals.

    reasons = [f"Selected from high-scoring {category} file"]
    if hunk_count > 1:
        reasons.append(f"grouped {hunk_count} adjacent hunks")
    if changed_lines is not None and changed_lines >= 8:
        reasons.append(f"{changed_lines} changed lines")
    if file_change.commit_count > 1:
        reasons.append(f"touched in {file_change.commit_count} commits")
    if file_change.flags:
        reasons.append(f"flags: {', '.join(file_change.flags)}")
    return "; ".join(reasons)


def extract_relevant_snippets(
    repo_root: Path | str,
    previous_tag: str | None,
    category_contexts: dict[str, CategoryContext],
    max_files_per_category: int = 5,
    max_hunks_per_file: int = 2,
) -> dict[str, list[DiffSnippet]]:
    repo_root = Path(repo_root).resolve()
    results: dict[str, list[DiffSnippet]] = {}

    for category_name, context in category_contexts.items():
        snippets: list[DiffSnippet] = []
        max_files, base_hunks_per_file, max_total_snippets = _category_snippet_budget(
            context,
            max_files_per_category=max_files_per_category,
            max_hunks_per_file=max_hunks_per_file,
        )
        candidate_files = [
            file_change
            for file_change in context.files
            if not is_semantic_noise_path(file_change.path)
        ][:max_files]

        for file_change in candidate_files:
            patch = get_file_patch(repo_root, file_change.path, previous_tag)
            if not patch.strip():
                continue

            evidence_units = _extract_evidence_units(patch)
            if not evidence_units:
                continue

            ranked_units = sorted(
                evidence_units,
                key=lambda unit: _score_evidence_unit(unit, category_name, file_change.path),
                reverse=True,
            )

            file_hunk_cap = _file_hunk_cap(
                file_change,
                base_hunks_per_file=base_hunks_per_file,
                remaining=max_total_snippets - len(snippets),
            )
            if file_hunk_cap <= 0:
                break

            for unit in ranked_units[:file_hunk_cap]:
                snippet_score = _score_evidence_unit(unit, category_name, file_change.path)
                reason = build_snippet_reason(
                    file_change,
                    unit.text,
                    category_name,
                    hunk_count=unit.hunk_count,
                    changed_lines=unit.changed_line_count,
                )
                snippets.append(
                    DiffSnippet(
                        path=file_change.path,
                        category=category_name,
                        subscopes=file_change.subscopes,
                        score=snippet_score,
                        reason=reason,
                        patch=unit.text,
                        flags=file_change.flags,
                    )
                )
                if len(snippets) >= max_total_snippets:
                    break
            if len(snippets) >= max_total_snippets:
                break

        snippets.sort(key=lambda item: item.score, reverse=True)
        results[category_name] = snippets

    return results


def _category_snippet_budget(
    context: CategoryContext,
    *,
    max_files_per_category: int,
    max_hunks_per_file: int,
) -> tuple[int, int, int]:
    change_total = context.insertions + context.deletions
    heaviness = 0
    heaviness += min(context.commit_count // 3, 5)
    heaviness += min(change_total // 220, 5)
    heaviness += min(len(context.files) // 8, 4)

    file_cap = min(36, max_files_per_category + (heaviness * 3))
    hunk_cap = min(10, max_hunks_per_file + min(heaviness, 5))
    total_snippet_cap = min(260, 18 + (context.commit_count * 5) + (heaviness * 12))
    return file_cap, hunk_cap, total_snippet_cap


def _file_hunk_cap(
    file_change: FileChange,
    *,
    base_hunks_per_file: int,
    remaining: int,
) -> int:
    if remaining <= 0:
        return 0
    change_total = file_change.insertions + file_change.deletions
    commit_touch_bonus = min(file_change.commit_count, 4) - 1 if file_change.commit_count > 1 else 0
    change_bonus = min(change_total // 140, 4)
    cap = base_hunks_per_file + max(commit_touch_bonus, 0) + change_bonus
    return max(1, min(cap, remaining))


def _extract_evidence_units(patch: str) -> list[_EvidenceUnit]:
    parsed_hunks = _parse_patch_hunks(patch)
    if not parsed_hunks:
        return []

    grouped_units = _group_adjacent_hunks(parsed_hunks)
    if not grouped_units:
        return []

    filtered = [unit for unit in grouped_units if _is_semantically_useful_unit(unit)]
    return filtered if filtered else grouped_units


def _parse_patch_hunks(patch: str) -> list[_ParsedHunk]:
    if not patch.strip():
        return []

    parsed = _parse_with_unidiff(patch)
    if parsed:
        return parsed
    return _parse_with_regex(patch)


def _parse_with_unidiff(patch: str) -> list[_ParsedHunk]:
    if PatchSet is None:
        return []
    try:
        patch_set = PatchSet(patch.splitlines(keepends=True))
    except Exception:
        return []

    parsed: list[_ParsedHunk] = []
    for patched_file in patch_set:
        if getattr(patched_file, "is_binary_file", False):
            continue
        for hunk in patched_file:
            rendered = str(hunk).strip()
            if not rendered:
                continue
            parsed.append(
                _ParsedHunk(
                    text=rendered,
                    source_start=getattr(hunk, "source_start", None),
                    source_length=getattr(hunk, "source_length", None),
                    target_start=getattr(hunk, "target_start", None),
                    target_length=getattr(hunk, "target_length", None),
                )
            )
    return parsed


def _parse_with_regex(patch: str) -> list[_ParsedHunk]:
    matches = list(HUNK_HEADER_RE.finditer(patch))
    if not matches:
        cleaned = patch.strip()
        if not cleaned:
            return []
        return [
            _ParsedHunk(
                text=cleaned,
                source_start=None,
                source_length=None,
                target_start=None,
                target_length=None,
            )
        ]

    hunks: list[_ParsedHunk] = []
    for index, match in enumerate(matches):
        start = match.start()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(patch)
        rendered = patch[start:end].strip()
        if not rendered:
            continue
        hunks.append(
            _ParsedHunk(
                text=rendered,
                source_start=None,
                source_length=None,
                target_start=None,
                target_length=None,
            )
        )
    return hunks


def _group_adjacent_hunks(hunks: list[_ParsedHunk]) -> list[_EvidenceUnit]:
    if not hunks:
        return []

    groups: list[list[_ParsedHunk]] = []
    index = 0
    while index < len(hunks):
        current_group = [hunks[index]]
        index += 1
        while index < len(hunks) and len(current_group) < _MAX_GROUPED_HUNKS:
            if not _hunks_are_adjacent(current_group[-1], hunks[index], _MAX_ADJACENT_HUNK_GAP):
                break
            current_group.append(hunks[index])
            index += 1
        groups.append(current_group)

    units: list[_EvidenceUnit] = []
    for group in groups:
        rendered = "\n\n".join(hunk.text for hunk in group).strip()
        if not rendered:
            continue
        changed_line_count, meaningful_line_count = _change_line_counts(rendered)
        units.append(
            _EvidenceUnit(
                text=rendered,
                hunk_count=len(group),
                changed_line_count=changed_line_count,
                meaningful_line_count=meaningful_line_count,
            )
        )
    return units


def _hunks_are_adjacent(left: _ParsedHunk, right: _ParsedHunk, max_gap: int) -> bool:
    source_gap = _line_gap(left.source_start, left.source_length, right.source_start)
    target_gap = _line_gap(left.target_start, left.target_length, right.target_start)
    if source_gap is None and target_gap is None:
        return False
    return (source_gap is not None and source_gap <= max_gap) or (target_gap is not None and target_gap <= max_gap)


def _line_gap(start: int | None, length: int | None, next_start: int | None) -> int | None:
    if start is None or next_start is None:
        return None
    if length is None:
        length = 1
    end_line = start + max(length - 1, 0)
    gap = next_start - end_line
    if gap < 0:
        return 0
    return gap


def _change_line_counts(rendered_hunk: str) -> tuple[int, int]:
    changed = 0
    meaningful = 0
    for line in rendered_hunk.splitlines():
        if line.startswith("+++ ") or line.startswith("--- ") or line.startswith("@@"):
            continue
        if not line.startswith(("+", "-")):
            continue
        changed += 1
        if not _is_noise_change_line(line):
            meaningful += 1
    return changed, meaningful


def _is_semantically_useful_unit(unit: _EvidenceUnit) -> bool:
    if unit.meaningful_line_count >= 1:
        return True
    return unit.changed_line_count >= 10


def _score_evidence_unit(unit: _EvidenceUnit, category: str, path: str) -> float:
    score = score_patch_hunk(unit.text, category, path)
    if unit.hunk_count > 1:
        score += min(unit.hunk_count - 1, 2) * 1.4
    if unit.meaningful_line_count >= 8:
        score += 1.2
    if unit.meaningful_line_count == 0:
        score -= 6.0
    return round(score, 2)


def _is_noise_change_line(line: str) -> bool:
    payload = line[1:].strip() if line.startswith(("+", "-")) else line.strip()
    if not payload:
        return True

    lower = payload.lower()
    if lower.startswith(("import ", "from ", "#include ", "using ", "use ")):
        return True
    if payload.startswith(("//", "/*", "*", "#")):
        return True
    if re.fullmatch(r"[{}\[\](),.;:]+", payload):
        return True
    return False
