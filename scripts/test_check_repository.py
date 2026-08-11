#!/usr/bin/env python3
"""Unit tests for release provenance rules."""

from __future__ import annotations

import unittest

from scripts import check_repository


class ProvenancePolicyTests(unittest.TestCase):
    def test_every_declared_asset_family_is_recognized(self) -> None:
        examples = (
            "artifacts/audio/m6/example.wav",
            "artifacts/audio/m6/example.json",
            "artifacts/ui/m6/example.png",
            "artifacts/ui/m6/example.jpg",
            "artifacts/ui/m6/example.mp4",
            "factory-patches/example.rvp.json",
            "schemas/example.json",
            "src/ui/WebAssets/index.html",
            "src/ui/WebAssets/editor.css",
            "src/ui/WebAssets/editor.js",
            "tests/fixtures/golden/example.wav",
            "tests/fixtures/golden/example.json",
        )
        self.assertTrue(all(check_repository.is_documented_asset(path) for path in examples))

    def test_research_rom_and_undocumented_media_are_rejected(self) -> None:
        failures: list[str] = []
        check_repository.check_provenance_paths(
            [
                "BarrVerb/plugin/source.cpp",
                "research/sources/midiverb/u51.hex",
                "docs/unattributed-photo.png",
            ],
            failures,
            "test archive",
        )
        self.assertEqual(len(failures), 4)
        self.assertTrue(any("prohibited local research/output path" in failure for failure in failures))
        self.assertTrue(any("prohibited ROM/firmware-like extension" in failure for failure in failures))
        self.assertTrue(any("no ASSET_PROVENANCE.md rule" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
