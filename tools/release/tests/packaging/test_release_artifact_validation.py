from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from tools.release.packaging.debian import validate_release_artifacts


def _write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


class ReleaseArtifactValidationTests(unittest.TestCase):
    def test_validation_passes_when_expected_artifacts_exist(self) -> None:
        with TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir) / "release"
            output_dir.mkdir(parents=True, exist_ok=True)
            _write(output_dir / "vaulthalla_1.2.3-1_amd64.deb", "deb")
            _write(output_dir / "vaulthalla-web_1.2.3-1_next-standalone.tar.gz", "web")
            _write(output_dir / "changelog.release.md", "# release")
            _write(output_dir / "changelog.raw.md", "# raw")
            _write(output_dir / "changelog.payload.json", '{"schema_version":"x"}')

            result = validate_release_artifacts(output_dir=output_dir, require_changelog=True)

            self.assertEqual(result.output_dir, output_dir.resolve())
            self.assertEqual(len(result.debian_artifacts), 1)
            self.assertEqual(len(result.web_artifacts), 1)
            self.assertEqual(len(result.changelog_artifacts), 3)

    def test_validation_reports_missing_outputs_clearly(self) -> None:
        with TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir) / "release"
            output_dir.mkdir(parents=True, exist_ok=True)
            _write(output_dir / "changelog.raw.md", "# raw")

            with self.assertRaisesRegex(ValueError, "Missing expected outputs"):
                _ = validate_release_artifacts(output_dir=output_dir, require_changelog=True)

    def test_validation_can_skip_changelog_checks(self) -> None:
        with TemporaryDirectory() as temp_dir:
            output_dir = Path(temp_dir) / "release"
            output_dir.mkdir(parents=True, exist_ok=True)
            _write(output_dir / "vaulthalla_1.2.3-1_amd64.deb", "deb")
            _write(output_dir / "vaulthalla-web_1.2.3-1_next-standalone.tar.gz", "web")

            result = validate_release_artifacts(output_dir=output_dir, require_changelog=False)
            self.assertEqual(len(result.debian_artifacts), 1)
            self.assertEqual(len(result.web_artifacts), 1)


if __name__ == "__main__":
    unittest.main()
