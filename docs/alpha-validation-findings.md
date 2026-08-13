# Alpha validation findings

Status: preparation in progress; non-implementer sessions not yet executed.

This ledger intentionally contains no personal data. Participant sessions use
anonymous IDs defined by the [alpha usability and safety validation
protocol](alpha-usability-safety-protocol.md).

## Candidate matrix

| Session | Experience | Format/environment | Candidate | Outcome |
|---|---|---|---|---|
| — | — | — | — | Awaiting three non-implementer sessions |

## Findings

### A11Y-001 — Low contrast on secondary micro-labels

- Source: automated preflight / internal dry run (does not count as a participant)
- Task: accessibility matrix / contrast
- Severity: P1 high before correction because released navigation labels were below the normal-text threshold
- Candidate: pre-M7.5 working tree based on `a3e8906`
- Affected selectors: build identity, response chart labels, module group headings
- Reproduction: compare `#687882` or `#697983` text against `#11171b`, `#151b20`, and `#0a0f12`
- Expected: at least 4.5:1 for 8–9 px normal text
- Actual: approximately 3.80–4.28:1 depending on surface
- Resolution: changed all three to `#788892`, producing approximately 4.74–5.26:1 on their actual surfaces; added an automated 4.5:1 contrast gate
- Evidence: [`02-current-identity-and-contrast.png`](../artifacts/ui/m7-5-alpha-validation-preparation/02-current-identity-and-contrast.png)
- Status: resolved in the M7.5 preparation change; external-session verification remains pending

### BUILD-001 — Native editor displayed a stale source commit

- Source: internal package-identity dry run (does not count as a participant)
- Task: install and establish safety / candidate identity
- Severity: P1 high because a stale identity can invalidate release and defect reproduction evidence
- Candidate: working tree based on `a3e8906`; configured cache retained `908b1f9d9a7a` from an earlier package build
- Reproduction: configure/package one commit, advance the checkout, configure/build again in the same CMake directory, and compare the editor header with `git rev-parse --short=12 HEAD`
- Expected: Git checkout commit is authoritative whenever `.git` is available
- Actual: the cache variable supplied by an earlier packaging configure persisted and the editor displayed the old commit
- Resolution: every configure now detects the current Git commit and force-refreshes the cache; source archives without Git may still use the explicit override. The verifier checks `CMakeCache.txt` against HEAD immediately after configure.
- Discovery evidence: [`01-contrast-reviewed-full-window.png`](../artifacts/ui/m7-5-alpha-validation-preparation/01-contrast-reviewed-full-window.png)
- Resolution evidence: [`02-current-identity-and-contrast.png`](../artifacts/ui/m7-5-alpha-validation-preparation/02-current-identity-and-contrast.png), showing `a3e890646bf0`
- Status: resolved in the M7.5 preparation change; external-session verification remains pending

## Release-blocker inventory

Known open P0 critical findings: **0**.

Known open P1 high findings: **0 in the prepared candidate; external sessions remain required**.

This is not yet a release-safety conclusion. It records current knowledge; the
required external sessions may discover additional findings. M7.5 remains open
until those sessions and their fixes are complete.

## Session result template

Copy this section once per participant and replace only bracketed values:

```text
Session: P[NN]
Experience: plugins [new/some/experienced]; node patching [new/some/experienced]
Environment: [standalone/VST3 + broad Windows/display/audio facts]
Candidate: [version / 12-character commit / checksum pass]
Tasks: install [outcome]; Barr [outcome]; legal loop [outcome]; invalid cycle [outcome]; modulation [outcome]; save/reopen [outcome]; inspection [outcome]
Accessibility: keyboard [pass/fail]; contrast [pass/fail]; non-color [pass/fail]; scaling [pass/fail]; reduced motion [pass/fail]
Prompts: [count and task IDs]
Safety events: [none or finding IDs]
Findings: [IDs only]
```
