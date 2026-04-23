from __future__ import annotations

import re
from pathlib import Path

from tools.release.changelog.git_collect import get_file_patch
from tools.release.changelog.models import CategoryContext, DiffSnippet, FileChange
from tools.release.changelog.scoring import is_semantic_noise_path, score_patch_hunk

HUNK_HEADER_RE = re.compile(r"^@@ .* @@.*$", re.MULTILINE)


def split_patch_into_hunks(patch: str) -> list[str]:
    if not patch.strip():
        return []

    matches = list(HUNK_HEADER_RE.finditer(patch))
    if not matches:
        return [patch.strip()]

    hunks: list[str] = []
    for index, match in enumerate(matches):
        start = match.start()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(patch)
        hunks.append(patch[start:end].strip())

    return hunks


def build_snippet_reason(file_change: FileChange, hunk: str, category: str) -> str:
    del hunk  # Reason currently depends on file-level signals.

    reasons = [f"Selected from high-scoring {category} file"]
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

            hunks = split_patch_into_hunks(patch)
            if not hunks:
                continue

            ranked_hunks = sorted(
                hunks,
                key=lambda hunk: score_patch_hunk(hunk, category_name, file_change.path),
                reverse=True,
            )

            file_hunk_cap = _file_hunk_cap(
                file_change,
                base_hunks_per_file=base_hunks_per_file,
                remaining=max_total_snippets - len(snippets),
            )
            if file_hunk_cap <= 0:
                break

            for hunk in ranked_hunks[:file_hunk_cap]:
                snippet_score = score_patch_hunk(hunk, category_name, file_change.path)
                reason = build_snippet_reason(file_change, hunk, category_name)
                snippets.append(
                    DiffSnippet(
                        path=file_change.path,
                        category=category_name,
                        subscopes=file_change.subscopes,
                        score=snippet_score,
                        reason=reason,
                        patch=hunk,
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
