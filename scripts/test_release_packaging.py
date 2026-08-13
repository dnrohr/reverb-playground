#!/usr/bin/env python3
"""Tests for deterministic release archive construction."""

from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
import zipfile
import json
import hashlib
from pathlib import Path

from validate_windows_package import validate


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

    def test_windows_package_validation_checks_identity_binaries_and_checksum(self) -> None:
        commit = "0123456789abcdef0123456789abcdef01234567"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            package = root / "ReverbPlayground-0.1.0-windows-x64"
            members = {
                "Standalone/Reverb Playground.exe": b"binary-0123456789ab",
                "VST3/Reverb Playground.vst3/Contents/x86_64-win/Reverb Playground.vst3": b"plugin-0123456789ab",
                "LICENSE": b"license",
                "THIRD_PARTY_NOTICES.md": b"notices",
                "ASSET_PROVENANCE.md": b"assets",
                "README.md": b"readme",
                "install-vst3.ps1": b"install",
                "build-info.json": json.dumps({
                    "commit": commit[:12], "formats": ["Standalone", "VST3"]
                }).encode(),
            }
            archive_path = root / f"{package.name}.zip"
            with zipfile.ZipFile(archive_path, "w") as archive:
                for name, data in members.items():
                    archive.writestr(f"{package.name}/{name}", data)
            digest = hashlib.sha256(archive_path.read_bytes()).hexdigest()
            archive_path.with_suffix(".zip.sha256").write_text(
                f"{digest}  {archive_path.name}\n", encoding="utf-8")
            result = validate(archive_path, commit)
            self.assertEqual(result["commit"], commit)
            self.assertEqual(result["formats"], ["Standalone", "VST3"])


if __name__ == "__main__":
    unittest.main()
