from __future__ import annotations

import unittest
from unittest.mock import patch

from tools.release.changelog.models import CategoryContext, FileChange
from tools.release.changelog.snippets import extract_relevant_snippets


def _file(path: str, *, score: float = 10.0, insertions: int = 20, deletions: int = 4) -> FileChange:
    return FileChange(
        path=path,
        category="tools",
        subscopes=("release",),
        insertions=insertions,
        deletions=deletions,
        commit_count=1,
        score=score,
        flags=("release-tooling",),
    )


def _make_patch(hunks: int) -> str:
    blocks: list[str] = []
    for index in range(1, hunks + 1):
        blocks.append(
            f"@@ -{index},1 +{index},2 @@\n"
            f"-old_line_{index}\n"
            f"+new_line_{index}\n"
            f"+if changed_{index}: handle_update()\n"
        )
    return "\n".join(blocks)


class SnippetHarvestTests(unittest.TestCase):
    def test_extract_relevant_snippets_scales_with_category_heaviness(self) -> None:
        heavy = CategoryContext(
            name="tools",
            commit_count=12,
            insertions=900,
            deletions=220,
            commits=[],
            files=[
                _file("tools/release/cli.py", score=20.0, insertions=260, deletions=60),
                _file("tools/release/changelog/payload.py", score=18.0, insertions=220, deletions=50),
                _file("tools/release/changelog/ai/prompts/triage.py", score=17.5, insertions=210, deletions=48),
                _file("tools/release/changelog/release_workflow.py", score=16.0, insertions=180, deletions=42),
            ],
            snippets=[],
            detected_themes=["release-automation"],
        )
        light = CategoryContext(
            name="tools",
            commit_count=1,
            insertions=20,
            deletions=5,
            commits=[],
            files=[
                _file("tools/release/cli.py", score=12.0, insertions=20, deletions=5),
                _file("tools/release/changelog/payload.py", score=10.0, insertions=16, deletions=4),
            ],
            snippets=[],
            detected_themes=["release-automation"],
        )

        with patch("tools.release.changelog.snippets.get_file_patch", return_value=_make_patch(8)):
            snippets = extract_relevant_snippets(
                repo_root=".",
                previous_tag="v0.1.0",
                category_contexts={"heavy": heavy, "light": light},
                max_files_per_category=2,
                max_hunks_per_file=1,
            )

        self.assertGreater(len(snippets["heavy"]), len(snippets["light"]))

    def test_extract_relevant_snippets_excludes_derived_artifact_paths(self) -> None:
        context = CategoryContext(
            name="tools",
            commit_count=2,
            insertions=42,
            deletions=8,
            commits=[],
            files=[
                _file(".changelog_scratch/changelog.release.md", score=100.0, insertions=40, deletions=6),
                _file("tools/release/cli.py", score=12.0, insertions=12, deletions=2),
            ],
            snippets=[],
            detected_themes=["release-automation"],
        )

        def _patch_for(path_repo, path_file, path_prev):
            _ = (path_repo, path_prev)
            if path_file == ".changelog_scratch/changelog.release.md":
                raise AssertionError("derived artifact path should be excluded from semantic snippet harvest")
            return _make_patch(2)

        with patch("tools.release.changelog.snippets.get_file_patch", side_effect=_patch_for):
            snippets = extract_relevant_snippets(
                repo_root=".",
                previous_tag="v0.1.0",
                category_contexts={"tools": context},
                max_files_per_category=5,
                max_hunks_per_file=2,
            )

        harvested = snippets["tools"]
        self.assertTrue(harvested)
        self.assertTrue(all(item.path == "tools/release/cli.py" for item in harvested))


if __name__ == "__main__":
    unittest.main()
