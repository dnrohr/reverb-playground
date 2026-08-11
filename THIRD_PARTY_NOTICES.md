# Third-party notices

Reverb Playground is distributed under AGPL-3.0-only. The following pinned dependencies retain their own licenses and notices. This inventory describes source/build inputs and code included in the standalone or VST3 output; it does not relicense them.

## Runtime and plugin dependencies

| Component | Pinned version | Purpose | License |
|---|---:|---|---|
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.13 | Application, audio-device, WebView, standalone, and VST3 framework | AGPLv3 option; JUCE also offers a commercial license |
| [Microsoft WebView2 SDK](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4078.44) | 1.0.4078.44 | Windows WebView2 headers/loader used by JUCE | Microsoft software license terms included in the NuGet package |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Graph and measurement JSON | MIT; bundled portions also identify MIT, Apache-2.0, and CC0-1.0 material upstream |
| [React](https://github.com/facebook/react) and React DOM | 19.2.8 | Embedded editor UI | MIT |
| [React Flow / `@xyflow/react`](https://github.com/xyflow/xyflow) | 12.11.2 | Node editor, connections, pan, and zoom | MIT |

The bundled React Flow dependency tree is pinned by `web/pnpm-lock.yaml`. Its shipped transitive packages are:

- MIT: `@xyflow/system` 0.0.79, `classcat` 5.0.5, `csstype` 3.2.3, `scheduler` 0.27.0, `use-sync-external-store` 1.6.0, `zustand` 4.5.7, and the relevant `@types/*` packages.
- ISC: `d3-color` 3.1.0, `d3-dispatch` 3.0.1, `d3-drag` 3.0.0, `d3-interpolate` 3.0.1, `d3-selection` 3.0.0, `d3-timer` 3.0.1, `d3-transition` 3.0.1, and `d3-zoom` 3.0.0.
- BSD-3-Clause: `d3-ease` 3.0.1.

The editor uses operating-system fonts only. No font file is bundled. React Flow's visible attribution remains enabled.

## Build and test dependencies

| Component | Pinned version | Purpose | License |
|---|---:|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | 3.15.0 | Native tests only | Boost Software License 1.0 |
| TypeScript | 7.0.2 | Editor type checking | Apache-2.0 |
| Vite / `@vitejs/plugin-react` | 8.2.1 / 6.0.5 | Editor build | MIT |
| Vitest | 4.1.10 | Editor tests | MIT |

JUCE itself declares framework dependencies under several permissive, public-domain, and platform SDK licenses. The authoritative list ships in JUCE's `LICENSE.md` at the pinned tag. Windows, Visual Studio, CMake, pnpm, Python, WebView2 Runtime, and DAW hosts are external toolchain/runtime prerequisites and are not redistributed in this source tree.

## Historical research boundary

BarrVerb is an ISC-licensed research reference, but its transformed Alesis/MIDIVerb program ROM is not part of this project. Reverb Playground contains no BarrVerb source, `rom.h`, ROM image, decoded instruction table, copied factory program, or hardware firmware. The Barr-inspired graph, parameter choices, generated fixtures, and documentation are original project work built from public architectural descriptions. See [Asset provenance](ASSET_PROVENANCE.md) for the auditable file boundary.
