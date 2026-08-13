#!/usr/bin/env python3
"""Require the configured native build identity to match the checkout."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / "build/windows-msvc/CMakeCache.txt"


def configured_commit(cache_text: str) -> str | None:
    match = re.search(r"^REVERB_BUILD_COMMIT:STRING=([^\r\n]+)$", cache_text, re.MULTILINE)
    return match.group(1) if match else None


def main() -> int:
    expected = subprocess.run(
        ["git", "rev-parse", "--short=12", "HEAD"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if not CACHE.is_file():
        print(f"Build identity check failed: missing {CACHE.relative_to(ROOT)}", file=sys.stderr)
        return 1
    actual = configured_commit(CACHE.read_text(encoding="utf-8", errors="replace"))
    if actual != expected:
        print(
            f"Build identity check failed: configured {actual!r}, checkout {expected!r}",
            file=sys.stderr,
        )
        return 1
    print(f"Build identity passed: configured source commit {actual} matches HEAD.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
