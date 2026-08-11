#!/usr/bin/env python3
"""Tests for deterministic release archive construction."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class ReleaseArchiveTests(unittest.TestCase):
    def test_archive_order_timestamp_and_hash_are_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "Product-1.0"
            (source / "VST3").mkdir(parents=True)
            (source / "README.md").write_text("release\n", encoding="utf-8")
            (source / "VST3" / "plugin.bin").write_bytes(b"plugin")
            outputs = [root / "first.zip", root / "second.zip"]
            for output in outputs:
                subprocess.run(
                    [sys.executable, str(ROOT / "scripts" / "create_release_archive.py"),
                     "--source", str(source), "--output", str(output), "--epoch", "1700000000"],
                    check=True,
                    capture_output=True,
                )
            self.assertEqual(outputs[0].read_bytes(), outputs[1].read_bytes())
            self.assertEqual(outputs[0].with_suffix(".zip.sha256").read_text(encoding="utf-8").split()[0],
                             outputs[1].with_suffix(".zip.sha256").read_text(encoding="utf-8").split()[0])
            with zipfile.ZipFile(outputs[0]) as archive:
                self.assertEqual(archive.namelist(), ["Product-1.0/README.md", "Product-1.0/VST3/plugin.bin"])
                self.assertEqual(len({entry.date_time for entry in archive.infolist()}), 1)


if __name__ == "__main__":
    unittest.main()
