#!/usr/bin/env python3
"""Validate the privacy, journey, and release gates of the alpha study record."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROTOCOL = ROOT / "docs/alpha-usability-safety-protocol.md"
FINDINGS = ROOT / "docs/alpha-validation-findings.md"

REQUIRED_PROTOCOL_PHRASES = (
    "Install and establish safety",
    "Complete the Barr tutorial",
    "Build a legal delayed feedback loop",
    "Cause and repair an invalid algebraic cycle",
    "Add visible modulation",
    "Save, close, reopen, and inspect",
    "Keyboard",
    "Contrast",
    "Non-color",
    "Scaling",
    "Reduced motion",
    "P0 critical",
    "P1 high",
)


def check_preparation(protocol: str, findings: str) -> list[str]:
    failures: list[str] = []
    for phrase in REQUIRED_PROTOCOL_PHRASES:
        if phrase not in protocol:
            failures.append(f"protocol: missing required journey/accessibility gate {phrase!r}")
    for forbidden in ("participant name", "email address:", "IP address:", "exact location:"):
        if forbidden.lower() in findings.lower():
            failures.append(f"findings: personal-data field is prohibited: {forbidden!r}")
    if "anonymous IDs" not in findings or "P[NN]" not in findings:
        failures.append("findings: anonymous participant-ID policy/template is missing")
    if "Known open P0 critical findings:" not in findings or "Known open P1 high findings:" not in findings:
        failures.append("findings: release-blocker inventory is missing")
    return failures


def check_release(findings: str) -> list[str]:
    failures: list[str] = []
    sessions = set(re.findall(r"^Session: (P\d{2})$", findings, re.MULTILINE))
    if len(sessions) < 3:
        failures.append(f"release: requires at least three distinct recorded sessions; found {len(sessions)}")
    if not re.search(r"Known open P0 critical findings:\s*\*\*0\*\*", findings):
        failures.append("release: open P0 inventory must be exactly zero")
    if not re.search(r"Known open P1 high findings:\s*\*\*0\*\*", findings):
        failures.append("release: open P1 inventory must be exactly zero")
    if "not yet executed" in findings.lower() or "awaiting three" in findings.lower():
        failures.append("release: findings ledger still reports participant execution pending")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release", action="store_true", help="also require completed external sessions and zero blockers")
    arguments = parser.parse_args()
    protocol = PROTOCOL.read_text(encoding="utf-8")
    findings = FINDINGS.read_text(encoding="utf-8")
    failures = check_preparation(protocol, findings)
    if arguments.release:
        failures.extend(check_release(findings))
    if failures:
        print("Alpha validation checks failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    mode = "release" if arguments.release else "preparation"
    print(f"Alpha validation {mode} contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
