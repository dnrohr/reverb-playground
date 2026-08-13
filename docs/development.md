# Development guide

## Supported foundation toolchain

The primary M0 development environment is:

- Windows 10 or 11 x64;
- Visual Studio 2022 Build Tools with the Desktop development with C++ workload;
- MSVC x64 compiler and Windows SDK installed by that workload;
- CMake 3.25 or newer;
- Git;
- Node.js 24;
- pnpm 11.16.0 (the version pinned by `web/package.json`);
- Python 3.11 or newer for repository, documentation, and packaging checks;
- an internet connection during initial dependency population.

The repository was first verified with CMake 4.4.2 and Visual Studio 2022 Build Tools. The authoritative architecture and licensing decision is [ADR 0001](adr/0001-primary-stack-and-delivery-target.md).

## Clean-checkout quick start

Open a Developer PowerShell, then clone and enter the repository:

```powershell
git clone https://github.com/dnrohr/reverb-playground.git
cd reverb-playground
```

Run the single verification entry point below. It fetches only the pinned
package/source dependencies declared by the checkout and produces ignored
outputs under `web/node_modules/`, `web/dist/`, and `build/windows-msvc/`.
Neither a local BarrVerb clone nor MIDIVerb research material is required.

## Configure, build, and test

The required local verification command is the same command used by CI:

```powershell
.\scripts\verify.ps1 -Configuration Debug
```

It installs the locked web dependencies; runs TypeScript, web, repository,
documentation, native, and audio tests; rejects ROM-derived filenames and
developer-local paths; builds the web assets, standalone, and VST3; and checks
all documented local links and canonical contributor commands.

The equivalent individual commands are:

From a Developer PowerShell or any shell where CMake is available:

```powershell
pnpm --dir web install --frozen-lockfile
pnpm --dir web typecheck
pnpm --dir web test
pnpm --dir web build
python -m unittest discover -s scripts -p 'test_*.py'
python scripts/check_repository.py
python scripts/check_documentation.py
python scripts/check_accessibility.py
python scripts/check_alpha_validation.py
cmake --preset windows-msvc
python scripts/check_build_identity.py
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
```

The first configure downloads the pinned JUCE and Catch2 source revisions into the ignored build directory. It does not require the local BarrVerb or MIDIVerb_RE research checkouts.

When `.git` is available, each configure force-refreshes the embedded
12-character commit from the checkout, replacing any older value left by a
packaging configure. A source archive without Git may provide
`-DREVERB_BUILD_COMMIT=<commit>`. `check_build_identity.py` rejects a configured
checkout whose cache does not match `HEAD`.

After a successful run, open
`build/windows-msvc/src/app/ReverbPlayground_artefacts/Debug/Standalone/Reverb Playground.exe`
for the standalone, or scan
`build/windows-msvc/src/app/ReverbPlayground_artefacts/Debug/VST3/Reverb Playground.vst3`
in a VST3 host.

## Windows release package

From a clean committed checkout, run:

```powershell
.\scripts\package_windows.ps1
```

This builds Release standalone and VST3 targets, stages the license/notices/install guide, records the exact version and 12-character source commit in `build-info.json`, and creates a sorted timestamp-normalized ZIP plus SHA-256 file under `out/packages/`. `-AllowDirty` exists only for local packaging-script validation; publishable artifacts must come from a clean commit.

The public alpha is reproduced by `.github/workflows/release.yml`. A maintainer
first runs the full Release verifier and packaging rehearsal on a clean `main`,
then pushes the annotated `v0.1.0-alpha.1` tag at that verified release commit.
The tag workflow checks the tag/release-note contract, repeats the full Release
build and tests, regenerates the deterministic package, retains it as a workflow
artifact, and creates the GitHub prerelease with ZIP, checksum, demonstration,
and the checked-in notes. Never move or reuse a published release tag.

The release demonstration is reproducible from reviewed interaction evidence:

```powershell
.\scripts\create_release_demo.ps1
python scripts\check_release.py --tag v0.1.0-alpha.1
```

Build products are written beneath `build/windows-msvc/`. The relevant smoke products are:

- `src/app/ReverbPlayground_artefacts/Debug/Standalone/Reverb Playground.exe`
- `src/app/ReverbPlayground_artefacts/Debug/VST3/Reverb Playground.vst3`
- `tests/Debug/reverb_tests.exe`

The application and VST3 run the editable native graph, including the shipped
Barr, reverse-envelope, and gated factory designs. The standalone also owns
audio-device selection; a plugin host owns device and transport settings for
the VST3.

## Source layout

| Path | Responsibility |
|---|---|
| `src/dsp/` | Framework-light, real-time-safe DSP primitives |
| `src/graph/` | Semantic graph model, validation, compilation, and serialization |
| `src/render/` | Headless rendering, deterministic analysis, WAV output, and renderer CLI |
| `src/ui/` | Editor components and schema/command presentation |
| `src/app/` | JUCE standalone/plugin wrapper and integration boundaries |
| `tests/` | Native deterministic tests and test-only fixtures |
| `docs/` | Product, architecture, research, and contributor documentation |
| `artifacts/ui/<task-slug>/` | Reviewed screenshots/videos required by completed UI tasks |

Keep reusable DSP fixtures under `tests/fixtures/` once required. Fixtures must be small, deterministic, documented, and legally redistributable. Generated build output and dependency sources remain under `build/` and are never staged.

## Dependency policy

JUCE, nlohmann/json, and Catch2 are declared with pinned release tags in the root `CMakeLists.txt`. Changing a revision requires:

1. reviewing upstream release notes and license changes;
2. running a clean configure, build, and test;
3. recording material compatibility changes in documentation or an ADR.

Do not introduce a dependency on local research clones, ROM images, developer home-directory files, or environment-specific absolute paths.

## Warning and formatting policy

- Project-owned C++ targets compile with `/W4 /WX /permissive-` on MSVC and `-Wall -Wextra -Wpedantic -Werror` on supported Clang/GCC builds.
- Tracked text is UTF-8, LF-terminated, has a final newline, and contains no trailing whitespace.
- CMake, native tests, repository checks, and Markdown link checks are required CI gates.
- Third-party dependencies are built from pinned sources. Warnings originating entirely inside dependency targets are addressed through upgrades or narrowly documented upstream-compatible workarounds, not broad suppression of project warnings.
- Formatting automation will be introduced with the first non-trivial DSP/schema implementation. Until then, repository checks enforce deterministic whitespace and line-ending rules.

## UI evidence

When a task changes visible UI, store a reviewed screenshot under `artifacts/ui/<task-slug>/`. Add a short video when interaction, animation, live telemetry, modulation, or audio-reactive behavior cannot be proven by a still image. Non-UI tasks state that no capture was required.

Runtime topology publication uses the bounded ownership protocol in [Runtime topology publication](runtime-topology-publication.md). Do not replace its pending/active raw envelopes with callback-owned smart pointers or reclaim a retired runtime from `process`; both changes can move destruction or a non-lock-free reference-count operation onto the audio thread.

Topology transitions use the fixed policy in [Topology-change crossfades](topology-change-crossfades.md). Crossfade output scratch must remain prepared with the runtime, and no third graph may execute while a transition is active. On Windows, preserve the exact logical bounds in `EditorShell::resized`; JUCE/WebView2 already translate them for monitor DPI. The standalone chooses the primary display work area minus a small 32-by-64 logical frame margin, while hosted VST3 editors retain a stable 1280-by-800 preferred size and remain freely resizable. Do not restore the former 1280-by-800 cap to the standalone path.

Do not place temporary captures in `artifacts/`; only reviewed completion evidence belongs there.
