# Roadmap progress

Last updated: 2026-08-08

| Task | Status | Evidence |
|---|---|---|
| M0.1 Select the primary implementation stack | Complete | ADR 0001; Markdown links verified; commit `e913f5a` on `origin/main` |
| M0.2 Create the project skeleton | Complete | Windows Debug and Release standalone/VST3 builds; Debug and Release native tests; standalone smoke launch and screenshot |
| M0.3 Establish continuous integration and quality checks | Complete | Shared local/CI verifier; warnings-as-errors; repository checks; deliberate failing-test proof |
| M0.4 Define the graph schema v1 | Complete | JSON Schema; typed C++ graph; deterministic serialization; valid/invalid fixtures; 4/4 tests; commit `29491a1` on `origin/main` |
| M0.5 Define real-time and safety contracts | Complete | Versioned contract; executable cycle validation; latched numerical guard; native tests |
| M1.1 Implement core DSP primitives | Complete | Mono gain/invert, sum, delay, allpass, and low-pass; multi-rate timing, energy, polarity, and reset tests |
| M1.2 Implement the fixed Barr reference graph | Complete | Stable serialized graph; public-primitive DSP; mono input sum; distinct finite stereo impulse output |
| M1.3 Add offline rendering and golden tests | Complete | Headless PCM16 WAV CLI; JSON analysis; silence/impulse/noise goldens; reset/reload diagnostics |
| M1.4 Add reference measurements | Complete | Automated onset/length/peak/stereo/Schroeder/RT60 metrics; synthetic estimator tests; versioned artifact |
| M1.5 Create the audible reference harness | Complete | Live wet processor; device/default status; impulse/gain/mute/safety controls; startup tests; screenshot/video |
| M2.1 Prototype the schematic editor shell | Complete | Embedded three-pane React Flow shell; documented navigation; scaling check; screenshot/video evidence |
| M2.2 Bind visible nodes to the fixed runtime | Complete | Native runtime snapshot; single-source identities/values; live inspector; mismatch detection |
| M2.3 Implement continuous parameter editing | Complete | Lock-free live parameters; smoothing/crossfade tests; exact undo/redo; screenshot/video evidence |
| M2.4 Save and reload the reference patch | Complete | Schema-v1 files; atomic validation; exact layout/viewport restore; invalid-load diagnostics |

## M0.2 verification

Environment:

- CMake 4.4.2
- Visual Studio 2022 Build Tools
- MSVC 19.44.35228.0
- Windows SDK 10.0.26100.0

Commands:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
cmake --build --preset windows-release --parallel 2
ctest --test-dir build/windows-msvc -C Release --output-on-failure
```

Results:

- Debug standalone: built and launched.
- Debug VST3: built with generated `moduleinfo.json`.
- Release standalone: built.
- Release VST3: built with generated `moduleinfo.json`.
- Debug tests: 2/2 passed.
- Release tests: 2/2 passed.
- UI evidence: [`artifacts/ui/m0-2-project-skeleton/standalone-smoke-shell.png`](../artifacts/ui/m0-2-project-skeleton/standalone-smoke-shell.png).

## M0.3 verification

Required local/CI command:

```powershell
./scripts/verify.ps1 -Configuration Debug
```

Results:

- Repository checks passed for all project files.
- MSVC project-owned targets compiled with `/W4 /WX /permissive-`.
- Debug standalone, VST3, and native test targets built.
- Native tests passed 2/2 after restoration.
- A temporary `REQUIRE_FALSE` assertion was added to `DspGainTests.cpp`; CTest failed 1/2 with exit code 8, proving the test gate rejects failures. The assertion was removed before the final passing verification.
- CI uses the same `scripts/verify.ps1` entry point on pushes to `main` and pull requests.
- UI unchanged; no new screenshot or video required.

## M0.4 verification

- JSON Schema Draft 2020-12 document added at `schemas/patch-v1.schema.json`.
- Semantic nodes/connections and editor layout are separate top-level structures.
- Stereo fixture I/O uses explicit `out-l`/`out-r` and `in-l`/`in-r` mono audio ports.
- Ports carry `audio` or `control` signal types and `input` or `output` direction.
- The invalid fixture connects control to audio; semantic validation rejects it with a specific diagnostic.
- The valid fixture preserves stable node/connection IDs, layout positions, and a `13.725` millisecond delay through deterministic JSON write/read cycles.
- Shared Debug verification passed 4/4 native tests with warnings-as-errors.
- UI unchanged; no screenshot or video required.

## M0.5 verification

- The versioned contract prohibits delay-free cycles and unbounded or blocking audio-thread work, and defines initial resource ceilings.
- Graph validation removes explicit `delay` nodes and rejects any remaining directed cycle.
- The numerical output guard detects NaN, infinity, and finite runaway levels, zeros the complete block, and remains muted until explicit reset.
- Parameter smoothing and immutable topology-snapshot publication have separate specified paths.
- Native tests observe zero-delay rejection, legal delayed feedback, non-finite containment, runaway containment, the mute latch, and reset.
- UI unchanged; no screenshot or video required.

## M1.1 verification

- Gain/invert and explicit sum match signed reference vectors.
- Delay impulse timing is tested at 44.1 and 96 kHz using the documented nearest-sample millisecond conversion.
- Coefficient-0.5 and general allpasses preserve impulse energy within the documented `1e-5` tolerance.
- One-pole low-pass step response is monotonic and bounded.
- Stateful primitives produce deterministic silence after reset.
- Allocation and validation occur in `prepare`; `process` and `reset` are bounded and `noexcept`.
- UI unchanged; no screenshot or video required.

## M1.2 verification

- The graph has stable IDs and round-trips through patch JSON deterministically.
- Stereo inputs reach an explicit mono sum, followed by input filtering and shared diffuser/tank stages.
- Separate left/right terminal allpasses create distinct mono output branches.
- A two-second impulse render produces finite, nonzero, non-identical wet channels.
- Reset followed by silent input produces deterministic stereo silence.
- Floating-point arithmetic, representative delay choices, absent outer feedback, filtering, and sample-rate departures are documented explicitly.
- UI unchanged; no screenshot or video required.

## M1.3 verification

- `reverb_render_cli` renders the canonical patch to stereo PCM16 WAV without creating a UI.
- Silence, impulse, and fixed-seed bounded-noise renders compare every PCM16 channel/sample against compact committed goldens with a one-LSB portable tolerance.
- Golden mismatches identify the first frame and channel plus expected, actual, and difference values.
- Same-engine reset/rerender and serialized patch reload reproduce identical floating-point channels on the primary toolchain.
- Analysis JSON records engine version, input, sample rate, frame count, per-channel peak/onset, and stable PCM16 FNV-1a hashes.
- Shared Debug verification passes 17/17 tests, including a CLI output smoke test.
- Fixtures are locally synthesized, documented, approximately 141 KiB total, and contain no third-party audio or ROM-derived data.
- UI unchanged; no screenshot or video required.

## M1.4 verification

- Automated metrics cover thresholded onset and impulse length, per-channel peak, stereo-difference RMS, decimated Schroeder energy decay, and T30-style RT60 extrapolation.
- A synthetic 0.75-second exponential response is recovered within one percent.
- RT60 returns null for silence, insufficient decay range, and excessive tail noise.
- The two-second 48 kHz Barr reference measurement is stored as a versioned JSON artifact and checked against a fresh render.
- Documentation labels direct observations, threshold-dependent values, derived curves, and estimated RT60 separately.
- UI unchanged; no screenshot or video required.

## M1.5 verification

- Standalone and VST3 processing replace stereo output with the fixed wet reference path.
- The standalone uses JUCE's restored/default output and opens its Audio/MIDI Settings dialog from the in-editor Audio Device button.
- Impulse injection, master linear gain, emergency mute, numerical safety latch/reset, and no-device silence have native tests.
- Repreparing from 44.1 to 96 kHz clears state and updates the active sample rate safely.
- The standalone built, launched, reported the default 48 kHz Windows device, opened/closed device settings, and remained responsive.
- Current screenshot: [`artifacts/ui/m1-5-audible-reference-harness/audible-reference-harness.png`](../artifacts/ui/m1-5-audible-reference-harness/audible-reference-harness.png).
- Interaction video: [`artifacts/ui/m1-5-audible-reference-harness/audible-reference-controls.mp4`](../artifacts/ui/m1-5-audible-reference-harness/audible-reference-controls.mp4).

## M2.1 verification

- The JUCE editor embeds pinned React, TypeScript, and React Flow assets from the binary; it has no localhost or network dependency at runtime.
- The left library, central Barr reference graph, right inspector, and native audition strip remain visible at the default 1280 by 800 editor size.
- Pointer/keyboard selection, deletion/reset, inspector updates, and wheel/control zoom were exercised in the browser prototype and embedded standalone.
- Audio uses solid cables and circular ports; control uses dashed cables and diamond ports, with a textual legend.
- The embedded standalone was visually checked at 125% Windows display scaling. The browser layout uses bounded responsive columns and fit/zoom controls for the documented 100%, 125%, and 150% range.
- Web unit tests cover stable graph identity, UI-copy deletion isolation, and signal-semantic classes.
- Default screenshot: [`artifacts/ui/m2-1-schematic-editor/default.png`](../artifacts/ui/m2-1-schematic-editor/default.png).
- Selected-node screenshot: [`artifacts/ui/m2-1-schematic-editor/selected-node.png`](../artifacts/ui/m2-1-schematic-editor/selected-node.png).
- Zoomed screenshot: [`artifacts/ui/m2-1-schematic-editor/zoomed.png`](../artifacts/ui/m2-1-schematic-editor/zoomed.png).
- Pan/zoom/selection/inspector video: [`artifacts/ui/m2-1-schematic-editor/schematic-interactions.mp4`](../artifacts/ui/m2-1-schematic-editor/schematic-interactions.mp4).

## M2.2 verification

- Native immutable runtime descriptors now own every Barr node ID, type, label, role, port, parameter value/unit, and connection.
- DSP preparation, semantic graph construction, and the web runtime snapshot all consume those same descriptors.
- Contract-v1 parsing rejects unsupported contracts/engines, duplicate identities, unsupported types/roles, invalid values/units/signals, and unknown connection endpoints before rendering.
- Selecting a block displays values and units supplied by the native snapshot; the UI visibly reports `RUNTIME BOUND` and the active sample rate.
- Native identity validation compares nodes, ports, parameters, and connections; deliberate rename/value/connection drift is detected by tests and a debug assertion guards snapshot serving.
- Stereo-to-mono normalization, the explicit output branch, audition impulse boundary, and post-patch master gain/safety processing are documented; no hidden sum, polarity, channel conversion, or delay remains.
- Shared Debug verification passes 4/4 web tests and 27/27 native tests, including Standalone and VST3 builds.
- Bound graph screenshot: [`artifacts/ui/m2-2-runtime-binding/runtime-bound-default.png`](../artifacts/ui/m2-2-runtime-binding/runtime-bound-default.png).
- Live inspector screenshot: [`artifacts/ui/m2-2-runtime-binding/runtime-bound-inspector.png`](../artifacts/ui/m2-2-runtime-binding/runtime-bound-inspector.png).

## M2.3 verification

- Every inspector slider sends intermediate values to the native runtime before pointer release; release commits one history transaction.
- The native bridge resolves stable identities off-thread and writes 14 fixed always-lock-free atomic lanes; audio processing performs a bounded 14 loads with no allocation or blocking.
- Gain, low-pass, and allpass-coefficient targets smooth over 20 milliseconds. Allpass delay edits crossfade old/new taps over 20 milliseconds in a preallocated 100-millisecond ring buffer.
- Native tests cover smoothing, finite bounded delay transitions, and live harness application. Web tests cover exact undo/redo values, no-op edits, and redo invalidation.
- Continuous editing design: [Continuous parameter editing](continuous-parameter-editing.md).
- Inspector screenshot: [`artifacts/ui/m2-3-continuous-parameter-editing/continuous-parameter-inspector.png`](../artifacts/ui/m2-3-continuous-parameter-editing/continuous-parameter-inspector.png).
- Intermediate live values and undo/redo video: [`artifacts/ui/m2-3-continuous-parameter-editing/continuous-parameter-editing.mp4`](../artifacts/ui/m2-3-continuous-parameter-editing/continuous-parameter-editing.mp4). The native next-block test provides deterministic audio-side evidence while the UI capture makes the continuous transaction visible.

## M2.4 verification

- Save Patch writes the fixed reference as deterministic schema-v1 JSON containing semantics, current parameters, node positions, and viewport pan/zoom.
- Load Patch parses and validates the entire temporary document before replacing UI state or publishing any runtime parameter.
- A successful interaction restored Tank 2 to `70` milliseconds, its moved node position, and a `72%` viewport from a file.
- An unknown `futureField` produced an actionable schema-policy diagnostic while the restored `70` millisecond graph and `72%` viewport remained unchanged.
- Web tests cover deterministic round trip, exact parameter/position/viewport values, malformed JSON, unsupported schema, out-of-range parameters, and future-field rejection.
- Design and user behavior: [Saving and loading patches](patch-saving-and-loading.md).
- Successful restore screenshot: [`artifacts/ui/m2-4-patch-persistence/successful-patch-load.png`](../artifacts/ui/m2-4-patch-persistence/successful-patch-load.png).
- Atomic invalid-load screenshot: [`artifacts/ui/m2-4-patch-persistence/invalid-load-preserves-patch.png`](../artifacts/ui/m2-4-patch-persistence/invalid-load-preserves-patch.png).
- Save/load/rejection video: [`artifacts/ui/m2-4-patch-persistence/save-load-invalid.mp4`](../artifacts/ui/m2-4-patch-persistence/save-load-invalid.mp4).
