#!/usr/bin/env python3
"""Validate the identity, contents, binaries, and checksum of a Windows package."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path


REQUIRED_SUFFIXES = (
    "Standalone/Reverb Playground.exe",
    "VST3/Reverb Playground.vst3/Contents/x86_64-win/Reverb Playground.vst3",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "ASSET_PROVENANCE.md",
    "README.md",
    "install-vst3.ps1",
    "build-info.json",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def checkout_commit(root: Path) -> str:
    return subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, check=True,
        capture_output=True, text=True,
    ).stdout.strip()


def validate(archive_path: Path, expected_commit: str) -> dict[str, object]:
    checksum_path = archive_path.with_suffix(archive_path.suffix + ".sha256")
    if not checksum_path.is_file():
        raise ValueError(f"missing checksum file: {checksum_path}")
    recorded = checksum_path.read_text(encoding="utf-8").split()[0].upper()
    actual = sha256(archive_path)
    if recorded != actual:
        raise ValueError(f"checksum mismatch: recorded {recorded}, actual {actual}")

    with zipfile.ZipFile(archive_path) as archive:
        names = archive.namelist()
        for suffix in REQUIRED_SUFFIXES:
            if not any(name.endswith("/" + suffix) for name in names):
                raise ValueError(f"missing package member: {suffix}")
        build_info_name = next(name for name in names if name.endswith("/build-info.json"))
        build_info = json.loads(archive.read(build_info_name))
        if build_info.get("commit") != expected_commit[:12]:
            raise ValueError(
                f"build-info commit {build_info.get('commit')!r} does not match {expected_commit[:12]!r}")
        if build_info.get("formats") != ["Standalone", "VST3"]:
            raise ValueError("build-info formats must be Standalone and VST3")
        binary_names = [
            next(name for name in names if name.endswith("/Standalone/Reverb Playground.exe")),
            next(name for name in names if name.endswith(
                "/VST3/Reverb Playground.vst3/Contents/x86_64-win/Reverb Playground.vst3")),
        ]
        identity = expected_commit[:12].encode("ascii")
        for binary_name in binary_names:
            if identity not in archive.read(binary_name):
                raise ValueError(f"binary does not embed source identity {expected_commit[:12]}: {binary_name}")

    return {
        "archive": archive_path.name,
        "sha256": actual,
        "commit": expected_commit,
        "members": len(names),
        "formats": ["Standalone", "VST3"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--commit", help="Expected full source commit; defaults to HEAD")
    parser.add_argument("--json", action="store_true")
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    expected = arguments.commit or checkout_commit(root)
    if not re.fullmatch(r"[0-9a-fA-F]{40}", expected):
        parser.error("--commit must be a full 40-character Git commit")
    try:
        result = validate(arguments.archive.resolve(), expected.lower())
    except (OSError, ValueError, zipfile.BadZipFile, json.JSONDecodeError) as error:
        print(f"Package validation failed: {error}", file=sys.stderr)
        return 1
    if arguments.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Package validation passed: {result['archive']} {result['sha256']} commit {expected[:12]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
