#!/usr/bin/env python3
"""Verify repository ignore defaults for generated Therion outputs."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]


class TherionGitignoreTest(unittest.TestCase):
    def test_ignores_generated_outputs_without_hiding_sources_or_fixtures(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            shutil.copyfile(REPOSITORY_ROOT / ".gitignore", root / ".gitignore")
            subprocess.run(["git", "init", "--quiet", str(root)], check=True)

            paths = [
                "output/cave.pdf",
                "outputs/cave.lox",
                "cave.3d",
                "cave.plt",
                "survey/main.th",
                "survey/map.th2",
                "survey/project.thconfig",
                "tests/fixtures/example.lox",
            ]
            for relative_path in paths:
                path = root / relative_path
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()

            result = subprocess.run(
                ["git", "-C", str(root), "check-ignore", "--no-index", "--stdin"],
                input="\n".join(paths) + "\n",
                text=True,
                capture_output=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                set(result.stdout.splitlines()),
                {"output/cave.pdf", "outputs/cave.lox", "cave.3d", "cave.plt"},
            )


if __name__ == "__main__":
    unittest.main()
