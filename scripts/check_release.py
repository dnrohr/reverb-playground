#!/usr/bin/env python3
"""Validate the static alpha-release publication contract."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Callable


ROOT = Path(__file__).resolve().parents[1]
TAG = "v0.1.0-alpha.1"
ARCHIVE = "ReverbPlayground-0.1.0-windows-x64.zip"
NOTES = ROOT / "docs/releases/v0.1.0-alpha.1.md"
DEMO = ROOT / "artifacts/ui/m7-6-alpha-release/reverb-playground-alpha-demo.mp4"


def check_contract(
    read: Callable[[Path], str] = lambda path: path.read_text(encoding="utf-8"),
) -> list[str]:
    failures: list[str] = []
    cmake = read(ROOT / "CMakeLists.txt")
    workflow = read(ROOT / ".github/workflows/release.yml")
    notes = read(NOTES)
    readme = read(ROOT / "README.md")

    version = re.search(r"project\(\s*ReverbPlayground\s+VERSION\s+([0-9.]+)", cmake, re.S)
    if not version or version.group(1) != "0.1.0":
        failures.append("CMakeLists.txt: release contract expects product version 0.1.0")
    for token in (
        "push:", "tags:", "verify.ps1 -Configuration Release", "package_windows.ps1",
        "actions/upload-artifact@v4", "softprops/action-gh-release@v2", ARCHIVE,
        "prerelease: true", "body_path: docs/releases/v0.1.0-alpha.1.md",
    ):
        if token not in workflow:
            failures.append(f".github/workflows/release.yml: missing {token!r}")
    for heading in (
        "Supported system and formats", "Major capabilities", "Known alpha limitations",
        "Demonstration", "Feedback and source",
    ):
        if f"## {heading}" not in notes:
            failures.append(f"release notes: missing {heading!r} section")
    for disclosure in (
        "Windows 10 or 11", "Standalone", "VST3", "SHA-256",
        "three non-implementer sessions have not yet run", "GitHub Issues",
    ):
        if disclosure not in notes:
            failures.append(f"release notes: missing disclosure {disclosure!r}")
    for link in (
        "docs/windows-package-installation.md", "docs/getting-started-barr-tutorial.md",
        "docs/roadmap.md", "https://github.com/dnrohr/reverb-playground/issues",
        "artifacts/ui/m7-6-alpha-release/reverb-playground-alpha-demo.mp4",
    ):
        if link not in readme:
            failures.append(f"README.md: missing direct landing-page link {link!r}")
    if not DEMO.is_file() or DEMO.stat().st_size < 100_000:
        failures.append("release demonstration video is missing or implausibly small")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tag", help="tag being published; must match this release")
    arguments = parser.parse_args()
    failures = check_contract()
    if arguments.tag and arguments.tag != TAG:
        failures.append(f"release tag must be {TAG}, received {arguments.tag}")
    if failures:
        print("Release contract checks failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"Release contract passed for {TAG} and {ARCHIVE}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
