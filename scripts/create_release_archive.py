#!/usr/bin/env python3
"""Create a sorted, timestamp-normalized ZIP and adjacent SHA-256 file."""

from __future__ import annotations

import argparse
import hashlib
import os
import time
import zipfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--epoch", required=True, type=int)
    arguments = parser.parse_args()

    source = arguments.source.resolve()
    output = arguments.output.resolve()
    if not source.is_dir():
        parser.error(f"source directory does not exist: {source}")
    output.parent.mkdir(parents=True, exist_ok=True)

    timestamp = time.gmtime(max(arguments.epoch, 315532800))[:6]
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted((path for path in source.rglob("*") if path.is_file()), key=lambda item: item.as_posix()):
            relative = path.relative_to(source.parent).as_posix()
            info = zipfile.ZipInfo(relative, timestamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o755 if path.suffix.lower() in {".exe", ".ps1"} else 0o644) << 16
            archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)

    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    checksum = output.with_suffix(output.suffix + ".sha256")
    checksum.write_text(f"{digest}  {output.name}{os.linesep}", encoding="utf-8", newline="\n")
    print(f"Created {output} ({output.stat().st_size} bytes)")
    print(f"SHA-256 {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
