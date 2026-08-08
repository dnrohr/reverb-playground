#!/usr/bin/env python3
"""Deterministic repository checks that require only the Python standard library."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".json",
    ".md",
    ".ps1",
    ".py",
    ".txt",
    ".yml",
    ".yaml",
}
PROHIBITED_NAMES = (
    re.compile(r"^rom\.h$", re.IGNORECASE),
    re.compile(r"^midif.*\.bin$", re.IGNORECASE),
    re.compile(r"^u51\.hex$", re.IGNORECASE),
)
LOCAL_MARKERS = (
    "C:" + "/Users/" + "dnroh",
    "C:" + "\\Users\\" + "dnroh",
)


def project_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [ROOT / item.decode("utf-8") for item in result.stdout.split(b"\0") if item]


def is_text_file(path: Path) -> bool:
    return path.name in {"CMakeLists.txt", ".gitattributes", ".gitignore"} or path.suffix.lower() in TEXT_SUFFIXES


def check_text(path: Path, failures: list[str]) -> None:
    raw = path.read_bytes()
    relative = path.relative_to(ROOT).as_posix()

    if b"\r\n" in raw:
        failures.append(f"{relative}: CRLF line endings; repository text must use LF")

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        failures.append(f"{relative}: not valid UTF-8 ({error})")
        return

    if text and not text.endswith("\n"):
        failures.append(f"{relative}: missing final newline")

    for number, line in enumerate(text.splitlines(), start=1):
        if line.rstrip(" \t") != line:
            failures.append(f"{relative}:{number}: trailing whitespace")
        for marker in LOCAL_MARKERS:
            if marker in line:
                failures.append(f"{relative}:{number}: developer-local absolute path")


def check_markdown_links(path: Path, failures: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    relative = path.relative_to(ROOT).as_posix()
    for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
        if "://" in target or target.startswith(("#", "mailto:")):
            continue
        local = target.split("#", 1)[0]
        if local and not (path.parent / local).resolve().exists():
            failures.append(f"{relative}: missing local link target {target!r}")


def main() -> int:
    failures: list[str] = []
    files = project_files()

    for path in files:
        if any(pattern.match(path.name) for pattern in PROHIBITED_NAMES):
            failures.append(f"{path.relative_to(ROOT).as_posix()}: prohibited ROM-derived filename")
        if is_text_file(path):
            check_text(path, failures)
        if path.suffix.lower() == ".md":
            check_markdown_links(path, failures)

    if failures:
        print("Repository checks failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(f"Repository checks passed for {len(files)} project files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
