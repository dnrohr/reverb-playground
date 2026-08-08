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
