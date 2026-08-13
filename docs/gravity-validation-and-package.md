# Gravity Diffusion validation and Windows package

M9.6 validates the first shipped Gravity Diffusion implementation as the exact
editable schema-v2 factory graph, not as a private preset or a second DSP path.
The validation target is the 58-node, 94-cable graph with five visible Macros,
eight Curve Mappers, two LFOs, and 21 delay-bearing audio lines.

## Deterministic audio and automation gates

The native suite loads `factory-patches/gravity-diffusion.rvp.json` from disk
and checks the following behavior:

- impulse renders at 44.1, 48, and 96 kHz are finite, bounded, stereo, and non-silent;
- bounded-noise renders at the same rates remain finite and bounded;
- Gravity, Size, Feedback, Damping, and Modulation sweep continuously through
  their complete normalized ranges without recompiling the graph;
- a representative combined five-control trajectory remains finite and has no
  full-scale sample discontinuity;
- all 32 combined macro endpoint states remain finite under silence, impulse,
  and bounded noise; and
- graph publication retains the bounded 10 ms topology crossfade and performs
  no graph discovery or allocation on the audio thread.

The factory-file tests complement the builder-level Gravity tests: a generated
graph cannot pass in place of the artifact an external user actually loads.

## State and named-host validation

The complete Gravity document is stored with deliberately non-default macro
values (`-0.72`, `0.41`, `0.63`, `-0.28`, `0.84`) through the JUCE
`AudioProcessor` host-state contract. A fresh processor restores all 58 nodes,
94 connections, 58 layout entries, and all five exact values before audio
preparation, then retains the same payload after 96 kHz preparation.

| Host | Version/path | Result |
|---|---|---|
| Reverb Playground Standalone | packaged Release application | Complete Gravity graph selected, audio online, and full-window editor pass |
| Tracktion pluginval | 1.0.4, strictness 10 | Pass; scan, cold/warm open, editor lifecycle, state/restoration, automation, threading, buses, fuzzing, and audio at 44.1/48/96 kHz with 64/128/256/512/1024-frame blocks |
| JUCE AudioProcessor host-state harness | JUCE 8.0.13 | Pass; exact complete Gravity payload and macro-value restore before and after preparation |

Pluginval's downloaded Windows archive was checked against SHA-256
`C08E61CE3B96DB41636F8EC7E76F4C7E2C13EBDAC7FA1B5A1F52B4F32EC715AB`.
The current run is recorded in
`artifacts/validation/m9-6-gravity/pluginval-gravity-release.txt`.

## Windows scaling evidence

The same physical 1920-by-1200 display was switched through the real Windows
100%, 125%, and 150% per-monitor scale settings. Each Release standalone was
newly launched and maximized after the change. `PrintWindow` captured the target
window at native physical resolution so the evidence excludes desktop overlays
and avoids DPI-virtualized partial-screen capture. The original 125% setting
was restored after validation.

Evidence under `artifacts/ui/m9-6-gravity-validation/` shows:

- `02-standalone-100-percent.png` — full outer window and usable editor at 100%;
- `01-standalone-125-percent.png` — full outer window and usable editor at 125%;
- `03-standalone-150-percent.png` — selected Gravity Diffusion factory,
  complete 58-node graph, all three panes, toolbar controls, and full client at 150%.

The outer JUCE frame and WebView client meet on every edge in all three
captures. The 150% capture is the most constrained case and still exposes the
factory picker, A/B switch, measurement controls, module library, graph canvas,
inspector, scrollbars, and graph-editing toolbar.

## Package identity and verification

`scripts/package_windows.ps1` performs a Release configure/build for standalone
and VST3, creates the deterministic archive, and then runs
`scripts/validate_windows_package.py`. The validator:

1. recomputes and checks the adjacent SHA-256 file;
2. requires both current binaries and every install/license/provenance member;
3. requires `build-info.json` to name Standalone and VST3 and the exact
   12-character source commit; and
4. verifies that the same commit identity is embedded in both binary payloads.

Normal packaging still rejects a dirty worktree. `-AllowDirty` is only a local
pre-commit exercise and is not release evidence. The final M9.6 archive must be
created from the committed tree and validated against that exact commit.
