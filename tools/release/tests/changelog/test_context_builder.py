from __future__ import annotations

import unittest
from unittest.mock import patch

from tools.release.changelog.context_builder import build_release_context
from tools.release.changelog.models import CommitInfo


class ContextBuilderTests(unittest.TestCase):
    def test_same_version_tag_with_ahead_head_adds_warning(self) -> None:
        commits = [
            CommitInfo(
                sha="6cf9a156eae61fd9ba2c2204ee9c73c2832bd14b",
                subject="update packaging documentation",
                body="",
                files=["tools/release/changelog/render_raw.py"],
                insertions=5,
                deletions=1,
                categories=["tools"],
            )
        ]

        with (
            patch("tools.release.changelog.context_builder.get_latest_tag", return_value="v0.30.0"),
            patch(
                "tools.release.changelog.context_builder.get_head_sha",
                return_value="6cf9a156eae61fd9ba2c2204ee9c73c2832bd14b",
            ),
            patch("tools.release.changelog.context_builder.get_previous_release_tag_before") as lower_before_base,
            patch("tools.release.changelog.context_builder.get_commits_since_tag", return_value=commits),
            patch(
                "tools.release.changelog.context_builder.get_release_file_stats",
                return_value={"tools/release/changelog/render_raw.py": (5, 1)},
            ),
            patch("tools.release.changelog.context_builder.extract_relevant_snippets", return_value={}),
        ):
            context = build_release_context(version="0.30.0", repo_root=".")

        self.assertEqual(context.commit_count, 1)
        self.assertEqual(context.previous_tag, "v0.30.0")
        lower_before_base.assert_not_called()
        self.assertTrue(context.cross_cutting_notes)
        self.assertIn("Release tag already exists for this version", context.cross_cutting_notes[0])

    def test_patch_release_defaults_to_previous_release_before_line_base(self) -> None:
        with (
            patch("tools.release.changelog.context_builder.get_latest_tag", return_value="v0.34.1"),
            patch("tools.release.changelog.context_builder.get_previous_release_tag_before", return_value="v0.33.0"),
            patch("tools.release.changelog.context_builder.get_head_sha", return_value="abc123"),
            patch("tools.release.changelog.context_builder.get_commits_since_tag", return_value=[]),
            patch("tools.release.changelog.context_builder.get_release_file_stats", return_value={}),
            patch("tools.release.changelog.context_builder.extract_relevant_snippets", return_value={}),
        ):
            context = build_release_context(version="0.34.4", repo_root=".")
        self.assertEqual(context.previous_tag, "v0.33.0")

    def test_non_patch_release_defaults_to_latest_tag(self) -> None:
        with (
            patch("tools.release.changelog.context_builder.get_latest_tag", return_value="v0.33.9"),
            patch("tools.release.changelog.context_builder.get_previous_release_tag_before") as lower_before_base,
            patch("tools.release.changelog.context_builder.get_head_sha", return_value="abc123"),
            patch("tools.release.changelog.context_builder.get_commits_since_tag", return_value=[]),
            patch("tools.release.changelog.context_builder.get_release_file_stats", return_value={}),
            patch("tools.release.changelog.context_builder.extract_relevant_snippets", return_value={}),
        ):
            context = build_release_context(version="0.34.0", repo_root=".")
        self.assertEqual(context.previous_tag, "v0.33.9")
        lower_before_base.assert_not_called()

    def test_explicit_since_tag_override_bypasses_default_resolution(self) -> None:
        with (
            patch("tools.release.changelog.context_builder.get_latest_tag") as latest,
            patch("tools.release.changelog.context_builder.get_previous_release_tag_before") as lower_before_base,
            patch("tools.release.changelog.context_builder.get_head_sha", return_value="abc123"),
            patch("tools.release.changelog.context_builder.get_commits_since_tag", return_value=[]),
            patch("tools.release.changelog.context_builder.get_release_file_stats", return_value={}),
            patch("tools.release.changelog.context_builder.extract_relevant_snippets", return_value={}),
        ):
            context = build_release_context(version="0.34.2", repo_root=".", previous_tag="v0.34.1")
        self.assertEqual(context.previous_tag, "v0.34.1")
        latest.assert_not_called()
        lower_before_base.assert_not_called()

    def test_commit_order_is_preserved_for_recency_emphasis(self) -> None:
        commits = [
            CommitInfo(
                sha="newest",
                subject="new patch fix",
                body="",
                files=["tools/release/cli.py"],
                insertions=10,
                deletions=2,
                categories=["tools"],
            ),
            CommitInfo(
                sha="older",
                subject="older minor-line setup",
                body="",
                files=["tools/release/changelog/context_builder.py"],
                insertions=40,
                deletions=5,
                categories=["tools"],
            ),
        ]
        with (
            patch("tools.release.changelog.context_builder.get_latest_tag", return_value="v0.34.1"),
            patch("tools.release.changelog.context_builder.get_previous_release_tag_before", return_value="v0.33.0"),
            patch("tools.release.changelog.context_builder.get_head_sha", return_value="abc123"),
            patch("tools.release.changelog.context_builder.get_commits_since_tag", return_value=commits),
            patch(
                "tools.release.changelog.context_builder.get_release_file_stats",
                return_value={
                    "tools/release/cli.py": (10, 2),
                    "tools/release/changelog/context_builder.py": (40, 5),
                },
            ),
            patch("tools.release.changelog.context_builder.extract_relevant_snippets", return_value={}),
        ):
            context = build_release_context(version="0.34.2", repo_root=".")
        self.assertEqual([commit.sha for commit in context.commits], ["newest", "older"])


if __name__ == "__main__":
    unittest.main()
