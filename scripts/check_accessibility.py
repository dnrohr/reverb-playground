#!/usr/bin/env python3
"""Static release checks for the editor's testable accessibility contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def relative_luminance(hex_color: str) -> float:
    channels = [int(hex_color[index:index + 2], 16) / 255 for index in (1, 3, 5)]
    linear = [
        value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4
        for value in channels
    ]
    return 0.2126 * linear[0] + 0.7152 * linear[1] + 0.0722 * linear[2]


def contrast_ratio(foreground: str, background: str) -> float:
    lighter, darker = sorted(
        (relative_luminance(foreground), relative_luminance(background)), reverse=True
    )
    return (lighter + 0.05) / (darker + 0.05)


def check_contract(styles: str, app: str, editor_shell: str, plugin_editor: str) -> list[str]:
    failures: list[str] = []
    contrast_pairs = {
        "primary text / editor background": ("#edf1f4", "#0d1115"),
        "muted text / raised surface": ("#91a0aa", "#151b20"),
        "cyan status / editor background": ("#45d0cc", "#0d1115"),
        "amber status / editor background": ("#f2b44e", "#0d1115"),
        "violet control / editor background": ("#bd9cff", "#0d1115"),
        "danger status / editor background": ("#ec575c", "#0d1115"),
        "secondary labels / header": ("#788892", "#11171b"),
        "secondary labels / surface": ("#788892", "#151b20"),
        "secondary labels / chart": ("#788892", "#0a0f12"),
        "module item text / module item": ("#d5dde1", "#1a2228"),
    }
    for label, (foreground, background) in contrast_pairs.items():
        ratio = contrast_ratio(foreground, background)
        if ratio < 4.5:
            failures.append(f"contrast: {label} is {ratio:.2f}:1; expected at least 4.5:1")
    for required in (
        ".patch-identity span { color: #788892;",
        ".chart-grid text, .axis-label { fill: #788892;",
        ".module-group h2 { margin: 0 0 7px 4px; color: #788892;",
    ):
        if required not in styles:
            failures.append(f"styles.css: missing contrast-reviewed declaration {required!r}")
    reduced_motion = re.search(r"@media \(prefers-reduced-motion: reduce\)\s*\{(?P<body>.*?)\n\}", styles, re.DOTALL)
    if not reduced_motion or "animation-duration: .001ms !important" not in reduced_motion.group("body") or "transition-duration: .001ms !important" not in reduced_motion.group("body"):
        failures.append("styles.css: reduced-motion media rule must suppress animation and transition duration")
    for token in (
        'aria-label="Module library"',
        'aria-label="Patch canvas"',
        'aria-label="Inspector"',
        'AUDIO / SOLID',
        'CONTROL / DASHED',
        "\n              nodesFocusable\n",
        "\n              edgesFocusable\n",
        'aria-pressed={energyEnabled}',
        "prefers-reduced-motion: reduce",
    ):
        if token not in app:
            failures.append(f"App.tsx: missing keyboard/non-color/reduced-motion contract token {token!r}")
    if "browser_->setBounds(bounds);" not in editor_shell:
        failures.append("EditorShell.cpp: WebView must receive the complete available logical bounds")
    for token in ("setResizable(true, true);", "setResizeLimits(640, 400, 8192, 8192);"):
        if token not in plugin_editor:
            failures.append(f"PluginEditor.cpp: missing scaling contract {token!r}")
    for token in (
        "preferredEditorSize(",
        "juce::StandalonePluginHolder::getInstance() != nullptr",
        "setSize(initial.width, initial.height);",
    ):
        if token not in plugin_editor:
            failures.append(f"PluginEditor.cpp: missing bounded initial sizing contract {token!r}")
    for token in ("max-width: 100vw", "grid-template-columns: minmax(0, 1fr)"):
        if token not in styles:
            failures.append(f"styles.css: missing viewport clamp contract {token!r}")
    return failures


def main() -> int:
    failures = check_contract(
        (ROOT / "web/src/styles.css").read_text(encoding="utf-8"),
        (ROOT / "web/src/App.tsx").read_text(encoding="utf-8"),
        (ROOT / "src/ui/Source/EditorShell.cpp").read_text(encoding="utf-8"),
        (ROOT / "src/app/PluginEditor.cpp").read_text(encoding="utf-8"),
    )
    if failures:
        print("Accessibility contract checks failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("Accessibility contract passed: contrast, keyboard labels, non-color cues, scaling, and reduced motion.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
