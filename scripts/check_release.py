#!/usr/bin/env python3
"""Validate the static alpha-release publication contract."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Callable


ROOT = Path(__file__).resolve().parents[1]
TAG = "v0.1.0-alpha.2"
ARCHIVE = "ReverbPlayground-0.1.0-windows-x64.zip"
NOTES = ROOT / "docs/releases/v0.1.0-alpha.2.md"
DEMO = ROOT / "artifacts/ui/m33-usability-alpha-refresh/reverb-playground-alpha-2-demo.mp4"
MATRIX = ROOT / "artifacts/validation/m33-usability-alpha-refresh/workflow-matrix.json"


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
        "prerelease: true", "body_path: docs/releases/v0.1.0-alpha.2.md",
    ):
        if token not in workflow:
            failures.append(f".github/workflows/release.yml: missing {token!r}")
    for heading in (
        "Supported system and formats", "Changes since alpha.1", "Compatibility and migration",
        "Crash reports and privacy", "Known alpha limitations", "Demonstration", "Feedback and source",
    ):
        if f"## {heading}" not in notes:
            failures.append(f"release notes: missing {heading!r} section")
    for disclosure in (
        "Windows 10 or 11", "Standalone", "VST3", "SHA-256",
        "three non-implementer sessions have not yet run", "Nothing is uploaded automatically",
        "GitHub Issues",
    ):
        if disclosure not in notes:
            failures.append(f"release notes: missing disclosure {disclosure!r}")
    for link in (
        "docs/windows-package-installation.md", "docs/getting-started-barr-tutorial.md",
        "docs/roadmap.md", "https://github.com/dnrohr/reverb-playground/issues",
        "artifacts/ui/m33-usability-alpha-refresh/reverb-playground-alpha-2-demo.mp4",
    ):
        if link not in readme:
            failures.append(f"README.md: missing direct landing-page link {link!r}")
    verify_workflow = read(ROOT / ".github/workflows/verify.yml")
    for token in (
        "windows-development-package:", "needs: windows", "package_windows.ps1",
        "actions/upload-artifact@v4", "Standalone/Reverb Playground.exe",
        "retention-days: 14",
    ):
        if token not in verify_workflow:
            failures.append(f".github/workflows/verify.yml: missing development package token {token!r}")
    if not DEMO.is_file() or DEMO.stat().st_size < 100_000:
        failures.append("alpha.2 demonstration video is missing or implausibly small")
    try:
        matrix = json.loads(read(MATRIX))
        workflow_ids = {workflow.get("id") for workflow in matrix.get("workflows", [])}
        required_ids = {
            "first-audition", "first-construction", "nested-matrix-editing",
            "parameter-synchronization-and-tuning", "temporary-diagnosis-and-ab",
            "audio-file-playback-and-export", "dense-layout-cleanup",
            "crash-recovery", "emergency-mute", "guidance-and-reporting",
        }
        if matrix.get("candidate") != TAG or workflow_ids != required_ids:
            failures.append("alpha.2 integrated workflow matrix is incomplete or identifies the wrong candidate")
        for workflow in matrix.get("workflows", []):
            for field in ("expectedStateAudio", "saveReopen", "undoRedo", "accessibility", "realTimeSafety", "failureRecovery", "evidence"):
                if not workflow.get(field):
                    failures.append(f"alpha.2 workflow {workflow.get('id', '<unknown>')}: missing {field}")
    except (json.JSONDecodeError, OSError, TypeError) as error:
        failures.append(f"alpha.2 integrated workflow matrix is unreadable: {error}")
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
