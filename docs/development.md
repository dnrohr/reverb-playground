# Development guide

## Supported foundation toolchain

The primary M0 development environment is:

- Windows 10 or 11 x64;
- Visual Studio 2022 Build Tools with the Desktop development with C++ workload;
- MSVC x64 compiler and Windows SDK installed by that workload;
- CMake 3.25 or newer;
- Git;
- an internet connection during initial dependency population.

The repository was first verified with CMake 4.4.2 and Visual Studio 2022 Build Tools. The authoritative architecture and licensing decision is [ADR 0001](adr/0001-primary-stack-and-delivery-target.md).

## Configure, build, and test

From a Developer PowerShell or any shell where CMake is available:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The first configure downloads the pinned JUCE and Catch2 source revisions into the ignored build directory. It does not require the local BarrVerb or MIDIVerb_RE research checkouts.

Build products are written beneath `build/windows-msvc/`. The relevant smoke products are:

- `src/app/ReverbPlayground_artefacts/Debug/Standalone/Reverb Playground.exe`
- `src/app/ReverbPlayground_artefacts/Debug/VST3/Reverb Playground.vst3`
- `tests/Debug/reverb_tests.exe`

The application/plugin currently passes stereo audio through unchanged and displays a minimal editor shell. It is a build and integration smoke target, not yet a reverb implementation.

## Source layout

| Path | Responsibility |
|---|---|
| `src/dsp/` | Framework-light, real-time-safe DSP primitives |
| `src/graph/` | Semantic graph model, validation, compilation, and serialization |
| `src/ui/` | Editor components and schema/command presentation |
| `src/app/` | JUCE standalone/plugin wrapper and integration boundaries |
| `tests/` | Native deterministic tests and test-only fixtures |
| `docs/` | Product, architecture, research, and contributor documentation |
| `artifacts/ui/<task-slug>/` | Reviewed screenshots/videos required by completed UI tasks |

Keep reusable DSP fixtures under `tests/fixtures/` once required. Fixtures must be small, deterministic, documented, and legally redistributable. Generated build output and dependency sources remain under `build/` and are never staged.

## Dependency policy

JUCE and Catch2 are declared with pinned release tags in the root `CMakeLists.txt`. Changing a revision requires:

1. reviewing upstream release notes and license changes;
2. running a clean configure, build, and test;
3. recording material compatibility changes in documentation or an ADR.

Do not introduce a dependency on local research clones, ROM images, developer home-directory files, or environment-specific absolute paths.

## UI evidence

When a task changes visible UI, store a reviewed screenshot under `artifacts/ui/<task-slug>/`. Add a short video when interaction, animation, live telemetry, modulation, or audio-reactive behavior cannot be proven by a still image. Non-UI tasks state that no capture was required.

Do not place temporary captures in `artifacts/`; only reviewed completion evidence belongs there.
