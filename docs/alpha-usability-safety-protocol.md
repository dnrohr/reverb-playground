# Alpha usability and safety validation protocol

Status: ready for non-implementer participant sessions. Internal dry runs do
not count toward the participant requirement.

## Purpose and completion rule

This study tests whether a person other than the implementer can install the
Windows alpha, understand the Barr reference, construct and repair graphs,
modulate a delay, preserve work, and inspect behavior without unsafe output.

Run at least three individual sessions:

- one participant familiar with audio plugins or modular patching;
- one participant unfamiliar with node-based audio tools; and
- one participant who primarily uses keyboard navigation or can deliberately
  complete the keyboard-only segment.

A participant may satisfy more than one description, but three distinct
anonymous session IDs are required. M7.5 is complete only after all sessions
are recorded in [Alpha validation findings](alpha-validation-findings.md), all
P0/P1 findings are closed, and no known crash, data-loss, dangerous-output, or
audio-thread violation remains open.

## Privacy and research conduct

- Assign only `P01`, `P02`, and so on. Do not record names, email addresses,
  employers, account names, faces, voices, IP addresses, exact locations, DAW
  project names, or unrelated screen content.
- Record broad experience bands only: `new`, `some`, or `experienced` for
  audio plugins and node patching.
- Do not make audio/video recordings. The facilitator records task outcome,
  elapsed time rounded to the nearest minute, observable difficulty, quoted UI
  terms (not participant speech), and finding IDs.
- Participation is voluntary. Stop immediately on request or if hearing,
  equipment, or data could be at risk.

## Candidate and environment record

Before each session, record:

- anonymous participant ID and the two experience bands;
- standalone or VST3, host name/version if applicable, Windows 10/11, and
  display scale (100%, 125%, or 150%);
- package version, 12-character commit, and SHA-256 verification result;
- audio device class (built-in, USB interface, or host-managed), sample rate,
  and buffer size without device serial/name;
- reduced-motion setting and whether the keyboard-only segment is primary or
  deliberate testing.

Use a clean extracted package and no prior Reverb Playground settings or patch
files. Keep monitor/headphone gain conservative. Start with Master Audition
Gain at or below `0.25`; confirm **Emergency Mute** is reachable before sound.

## Facilitation rules

Give the participant the task goal and the product documentation link, not the
click sequence. Allow five minutes before one neutral prompt such as “What are
you looking for?” Mark any prompt. Do not explain signal flow, repair the graph,
or take control unless a stop condition occurs. After each task, ask what the
participant believes happened before revealing the expected result.

Outcome codes:

- `independent` — completed with no facilitator prompt;
- `prompted` — completed after one neutral prompt;
- `assisted` — required a specific instruction or facilitator action;
- `failed` — not completed within the time box or participant abandoned it;
- `blocked` — product/environment defect made completion impossible.

## Session tasks

### 1. Install and establish safety — 8 minutes

Goal: install/open the candidate, verify its identity, select stereo audio, and
identify how to silence output immediately.

Expected evidence: checksum was checked; header commit matches
`build-info.json`; audio is online or host-managed; participant points to
**Emergency Mute** before triggering audio. Stop if identity differs, the UI is
blank/clipped, or audio device setup cannot be closed safely.

### 2. Complete the Barr tutorial — 12 minutes

Give the participant [Getting started: hear and inspect the Barr
reference](getting-started-barr-tutorial.md). Ask them to describe the
stereo-to-mono-to-stereo path, trigger a safe impulse, enable energy, capture a
500 ms / -80 dBFS response with live input muted, zoom to the early response,
edit one Allpass coefficient, undo it, and save/reload the patch.

Expected evidence: separate L/R boundary cables and explicit Sum are
understood; output remains comfortable; energy and response views are not
mistaken for processors; Undo and save/reload preserve the visible graph.

### 3. Build a legal delayed feedback loop — 10 minutes

Goal: add **Delay**, **Gain / Invert**, and, where an input is occupied, an
explicit **Sum (+)** so some wet output returns to an earlier point through at
least `10 ms` delay and gain no greater than `0.5` magnitude.

Expected evidence: the loop contains an explicit Delay, publishes successfully,
highlights as a complete directed path, and appears in the loop inspector with
nominal delay and gain. Trigger only the fixed safe impulse. If output becomes
uncomfortable or the safety latch fires, use Emergency Mute, retain the graph
for observation, then follow the documented recovery sequence.

### 4. Cause and repair an invalid algebraic cycle — 7 minutes

Goal: construct a local **Sum (+)** / **Gain / Invert** cycle with no Delay,
observe why it is rejected, and repair it by breaking the cycle or inserting a
Delay.

Expected evidence: the visible draft remains editable; the error names the
need for explicit delay; the prior valid runtime remains audible; the corrected
graph publishes. Record any case where invalid topology replaces valid audio,
freezes editing, or loses work as P0/P1.

### 5. Add visible modulation — 8 minutes

Goal: patch **LFO** through **Scale / Offset** to a Delay or Allpass
`delay-mod` socket and identify the predicted/effective range.

Expected evidence: control uses dashed cables/diamond ports and is not confused
with audio; the participant keeps the target within its millisecond clamp;
continuous changes remain finite and responsive; Undo restores the prior
mapping.

### 6. Save, close, reopen, and inspect — 8 minutes

Goal: save the modified patch, close/reopen the editor or standalone, load the
file, then inspect feedback highlighting, Energy, response capture, and
Diagnostics.

Expected evidence: semantic graph, parameters, cable identities, layout, and
viewport survive; Saved/Unsaved is credible; inspection is correctly described
as measured, estimated, or topology-derived.

### 7. Accessibility matrix — 10 minutes

Perform the applicable rows and record pass/fail plus a finding for every fail:

| Check | Method | Pass condition |
|---|---|---|
| Keyboard | Tab/Shift+Tab, Enter, arrows where native, Ctrl+C/V/Z, Delete | Focus remains visible; graph elements and required actions are reachable; no keyboard trap |
| Contrast | Use released screenshot plus automated report | Normal text is at least 4.5:1; state/focus is discernible |
| Non-color | Temporarily use grayscale display filter if available | Audio/control, L/R, selection, loop alternatives, meters, and status remain distinguishable by text/shape/style |
| Scaling | Exercise 100%, 125%, and 150%, restarting after a change | Controls remain reachable; editor fills available bounds; text/ports do not clip materially |
| Reduced motion | Enable Windows animation reduction before launch | Energy reports Reduced, polling/animation is off, and graph/audio/editing remain usable |

## Stop and escalation rules

Stop the task immediately and preserve only non-personal reproduction facts if:

- audio exceeds the participant's comfortable level or cannot be muted;
- NaN, infinity, sustained runaway, unbounded CPU/memory, a crash, hang, or
  deadlock is observed;
- save/load or host state loses/corrupts work;
- the application reads/writes outside documented package/settings locations;
- a keyboard trap prevents reaching mute/close; or
- the participant reports discomfort.

Classify dangerous output, crash/hang, data loss, or audio-thread contract
violation as P0 or P1. Do not continue release work around it.

## Finding severity and closure

- `P0 critical`: hearing/equipment risk, exploitable corruption, repeatable
  data loss, or unrecoverable crash; release blocked immediately.
- `P1 high`: crash/hang, unsafe mute/recovery, audio-thread violation,
  inaccessible critical control, or common-workflow data loss; release blocked.
- `P2 medium`: task failure or serious confusion with a safe workaround.
- `P3 low`: polish, terminology, or discoverability issue that does not prevent
  completion.

Every finding records ID, task, severity, anonymous sessions affected,
candidate commit, reproduction, expected/actual result, status, resolution
commit, and verification. `closed`, `accepted-known-limitation`, and
`not-reproducible` require a written rationale. P0/P1 may only be `closed` for
release.
