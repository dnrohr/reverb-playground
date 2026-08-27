#!/usr/bin/env python3
"""Check that shipped behavior and canonical commands remain documented."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_SOURCE = ROOT / "web/src/modules.ts"
MODULE_REFERENCE = ROOT / "docs/module-and-visualization-reference.md"
DEVELOPMENT_GUIDE = ROOT / "docs/development.md"
TUTORIAL = ROOT / "docs/getting-started-barr-tutorial.md"
GRAVITY_CONTRACT = ROOT / "docs/gravity-behavior-and-measurements.md"
AUDIO_FILE_CONTRACT = ROOT / "docs/audio-file-source-and-transport-contract.md"

REQUIRED_GRAVITY_PHRASES = (
    "`-1.0...+1.0`",
    "Default and reset",
    "timeToPeakMs",
    "earlyLateEnergyRatioDb",
    "peakLevelDbfs",
    "integratedEnergyDb",
    "rt60Seconds",
    "Inverse rise",
    "Clustered/Bloom center",
    "Forward decay",
    "must never emit wet energy before its input arrives",
    "original project work",
)

REQUIRED_AUDIO_FILE_PHRASES = (
    "Live Input",
    "Audio File",
    "Test Impulse",
    "one-channel input is copied to both graph input channels",
    "more than two channels is rejected",
    "fixed-capacity stereo ring",
    "callback never allocates",
    "End of file",
    "Underrun",
    "VST3 host state",
)

REQUIRED_VISUALIZATIONS = (
    "schematic-canvas",
    "feedback-loops",
    "live-energy",
    "impulse-decay",
    "density-inspector",
    "teaching-overlays",
    "control-previews",
    "diagnostics",
)

CANONICAL_COMMANDS = (
    "git clone https://github.com/dnrohr/reverb-playground.git",
    "cd reverb-playground",
    r".\scripts\verify.ps1 -Configuration Debug",
    "pnpm --dir web install --frozen-lockfile",
    "pnpm --dir web typecheck",
    "pnpm --dir web test",
    "pnpm --dir web build",
    "python -m unittest discover -s scripts -p 'test_*.py'",
    "python scripts/check_repository.py",
    "python scripts/check_documentation.py",
    "cmake --preset windows-msvc",
    "cmake --build --preset windows-debug --parallel 2",
    "ctest --preset windows-debug",
)


def shipped_module_types(source: str) -> tuple[str, ...]:
    """Return module types in their source declaration order."""
    return tuple(re.findall(r"\{ type: '([^']+)', label:", source))


def check_contract(
    module_source: str,
    reference: str,
    development: str,
    tutorial: str,
    gravity: str,
    audio_file: str,
) -> list[str]:
    failures: list[str] = []
    modules = shipped_module_types(module_source)
    if not modules:
        failures.append("web/src/modules.ts: no shipped module declarations found")
    for module_type in modules:
        marker = f"<!-- module: {module_type} -->"
        if reference.count(marker) != 1:
            failures.append(
                f"docs/module-and-visualization-reference.md: expected one {marker!r} marker"
            )
    for visualization in REQUIRED_VISUALIZATIONS:
        marker = f"<!-- visualization: {visualization} -->"
        if reference.count(marker) != 1:
            failures.append(
                f"docs/module-and-visualization-reference.md: expected one {marker!r} marker"
            )
    for command in CANONICAL_COMMANDS:
        if command not in development:
            failures.append(f"docs/development.md: missing canonical command {command!r}")
    for phrase in (
        "Barr Reference",
        "Trigger Impulse",
        "Capture impulse",
        "Diagnostics",
        "Save Patch",
        "Load Patch",
    ):
        if phrase not in tutorial:
            failures.append(f"docs/getting-started-barr-tutorial.md: missing tutorial action {phrase!r}")
    for phrase in REQUIRED_GRAVITY_PHRASES:
        if phrase not in gravity:
            failures.append(
                f"docs/gravity-behavior-and-measurements.md: missing contract phrase {phrase!r}"
            )
    for phrase in REQUIRED_AUDIO_FILE_PHRASES:
        if phrase not in audio_file:
            failures.append(
                f"docs/audio-file-source-and-transport-contract.md: missing contract phrase {phrase!r}"
            )
    return failures


def main() -> int:
    failures = check_contract(
        MODULE_SOURCE.read_text(encoding="utf-8"),
        MODULE_REFERENCE.read_text(encoding="utf-8"),
        DEVELOPMENT_GUIDE.read_text(encoding="utf-8"),
        TUTORIAL.read_text(encoding="utf-8"),
        GRAVITY_CONTRACT.read_text(encoding="utf-8"),
        AUDIO_FILE_CONTRACT.read_text(encoding="utf-8"),
    )
    if failures:
        print("Documentation contract checks failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        f"Documentation contract passed for {len(shipped_module_types(MODULE_SOURCE.read_text(encoding='utf-8')))} "
        f"modules, {len(REQUIRED_VISUALIZATIONS)} visualizations, and {len(CANONICAL_COMMANDS)} commands."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
