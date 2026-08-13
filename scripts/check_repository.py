#!/usr/bin/env python3
"""Deterministic repository checks that require only the Python standard library."""

from __future__ import annotations

import io
import re
import subprocess
import sys
import tarfile
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
PROHIBITED_SUFFIXES = {".bin", ".hex", ".rom"}
PROHIBITED_ROOTS = (
    "BarrVerb/",
    "research/sources/",
    "tmp/",
    "build/",
    "out/",
)
BINARY_ASSET_SUFFIXES = {".gif", ".ico", ".jpeg", ".jpg", ".mp3", ".mp4", ".otf", ".pdf", ".png", ".ttf", ".wav", ".webp", ".woff", ".woff2", ".zip"}
DOCUMENTED_ASSET_RULES = (
    re.compile(r"^artifacts/audio/.+\.(json|wav)$", re.IGNORECASE),
    re.compile(r"^artifacts/ui/.+\.(jpe?g|mp4|png)$", re.IGNORECASE),
    re.compile(r"^factory-patches/[^/]+\.rvp\.json$", re.IGNORECASE),
    re.compile(r"^factory-patches/catalog\.json$", re.IGNORECASE),
    re.compile(r"^schemas/[^/]+\.json$", re.IGNORECASE),
    re.compile(r"^src/ui/WebAssets/(editor\.(css|js)|index\.html)$", re.IGNORECASE),
    re.compile(r"^tests/fixtures/.+\.(json|wav)$", re.IGNORECASE),
)
GENERATED_DATA_ROOTS = ("artifacts/audio/", "factory-patches/", "schemas/", "src/ui/WebAssets/", "tests/fixtures/")
GENERATED_DATA_SUFFIXES = {".css", ".html", ".js", ".json", ".wav"}
LOCAL_MARKERS = (
    "C:" + "/Users/" + "dnroh",
    "C:" + "\\Users\\" + "dnroh",
)


def git_paths(*arguments: str) -> list[str]:
    result = subprocess.run(
        ["git", *arguments],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [item.decode("utf-8").replace("\\", "/") for item in result.stdout.split(b"\0") if item]


def project_files() -> list[Path]:
    return [ROOT / item for item in git_paths("ls-files", "-z", "--cached", "--others", "--exclude-standard")]


def tracked_paths() -> list[str]:
    return git_paths("ls-files", "-z", "--cached")


def is_text_file(path: Path) -> bool:
    return path.name in {"CMakeLists.txt", ".gitattributes", ".gitignore", "DCO", "LICENSE"} or path.suffix.lower() in TEXT_SUFFIXES


def is_documented_asset(relative: str) -> bool:
    return any(rule.match(relative) for rule in DOCUMENTED_ASSET_RULES)


def check_provenance_paths(paths: list[str], failures: list[str], source: str) -> None:
    for relative in paths:
        if relative.startswith(PROHIBITED_ROOTS):
            failures.append(f"{relative}: prohibited local research/output path in {source}")
        path = Path(relative)
        if path.suffix.lower() in PROHIBITED_SUFFIXES:
            failures.append(f"{relative}: prohibited ROM/firmware-like extension in {source}")
        is_binary = path.suffix.lower() in BINARY_ASSET_SUFFIXES
        is_generated_data = relative.startswith(GENERATED_DATA_ROOTS) and path.suffix.lower() in GENERATED_DATA_SUFFIXES
        if (is_binary or is_generated_data) and not is_documented_asset(relative):
            failures.append(f"{relative}: binary/generated data has no ASSET_PROVENANCE.md rule")


def archive_paths() -> list[str]:
    result = subprocess.run(
        ["git", "archive", "--format=tar", "HEAD"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    with tarfile.open(fileobj=io.BytesIO(result.stdout), mode="r:") as archive:
        return [member.name.replace("\\", "/").rstrip("/") for member in archive.getmembers() if member.isfile()]


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
    tracked = tracked_paths()

    check_provenance_paths(tracked, failures, "tracked source")
    check_provenance_paths(archive_paths(), failures, "git archive")

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

    print(f"Repository checks passed for {len(files)} project files; {len(tracked)} tracked paths and the source archive satisfy provenance policy.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
