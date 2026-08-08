# ADR 0001: Primary stack and initial delivery target

- Status: Accepted
- Date: 2026-08-08
- Roadmap task: M0.1

## Context

Reverb Playground needs two capabilities with different constraints:

1. A deterministic, real-time-safe C++ DSP and graph runtime that can ship as an audio plugin and standalone application.
2. A polished schematic editor that can be prototyped quickly without determining the DSP architecture or patch format.

The project is open source. Its first useful slice must run inside representative DAW hosts, but UI framework risk should not delay validation of the reverb-construction experience.

The existing BarrVerb repository is an important research reference, not the production foundation. It uses DPF successfully, but it also bundles a transformed original MIDIVerb ROM whose redistribution status is not suitable as an assumption for this project.

## Decision

### Runtime, application, and plugin framework

Use **JUCE 8**, **C++20**, and **CMake** for the production runtime, standalone application, and plugin wrapper.

- Use JUCE under its **AGPLv3** option while Reverb Playground is distributed as an open-source AGPLv3 application.
- Keep DSP primitives, semantic graph types, graph validation/compilation, serialization, and offline analysis in framework-light C++ libraries. JUCE may supply containers or utilities at integration boundaries, but the core design must not depend on UI component classes.
- Use JUCE's plugin and audio-device facilities for the first standalone and VST3 deliverables.
- Do not use `juce::AudioProcessorGraph` as the semantic or production DSP graph. Its model is broader than the small sample/state scheduling model required here. It remains a reference and possible integration helper.

JUCE modules are dual-licensed under AGPLv3 and the commercial JUCE license. A future non-AGPL or closed-source distribution would require a new licensing decision and likely a commercial JUCE license.

### UI prototype

Use **React**, **TypeScript**, and **React Flow** for the first schematic-editor interaction prototype.

- React Flow is MIT licensed and provides node placement, connection handles, selection, pan/zoom, and keyboard interaction.
- The prototype edits the versioned project graph schema. It never executes the production audio graph.
- UI-to-native messages must use a versioned bridge derived from the same schema and commands used by tests.
- Do not use React Flow Pro-only example code unless its separate terms are reviewed and recorded.

### Production UI checkpoint

An embedded web UI is a candidate, not yet a permanent commitment.

At the end of M2.1, test the editor shell in the JUCE standalone application and at least two Windows VST3 hosts. Evaluate:

- keyboard focus and shortcuts;
- pointer capture and cable dragging;
- DPI scaling and host-driven resize;
- GPU/CPU idle cost;
- startup time and packaged size;
- offline asset loading and content-security boundaries;
- accessibility and reduced-motion support;
- window close/reopen and editor recreation.

If the checkpoint fails materially, retain the graph schema and interaction findings but implement the production editor as a native JUCE canvas. No production DSP or saved patch may depend on React component state.

### Initial platform and deliverables

The primary development and alpha target is:

- Windows 10/11 x64;
- Visual Studio 2022 with the MSVC toolchain;
- CMake 3.25 or newer;
- C++20;
- standalone application;
- VST3 plugin;
- 64-bit floating-point-capable offline test renderer, with real-time sample precision selected during DSP scaffolding.

macOS/AU, Linux/CLAP/LV2, and AAX are deferred until the Windows standalone/VST3 vertical slice is proven. The framework choice must not deliberately prevent later macOS and Linux support.

### Dependency policy

- Pin source dependencies to reviewed tags or commit hashes.
- Prefer git submodules or CMake dependency declarations whose resolved revision is committed; do not follow floating branches.
- Commit lockfiles for JavaScript tooling and pin the package-manager version.
- Keep production dependencies minimal and list their license and purpose.
- Use Catch2 for native tests under its Boost Software License 1.0 unless M0.2 finds a smaller justified alternative.
- Use nlohmann/json for framework-light patch serialization under its MIT license.
- Do not download code, models, ROMs, fonts, or media during a release build unless the exact source and integrity check are declared.
- Never require MIDIVerb ROM data, the local BarrVerb clone, or the MIDIVerb_RE research checkout to configure, build, test, or package the project.

### ROM and historical-material policy

- Do not commit BarrVerb's transformed `rom.h`, an original MIDIVerb/MIDIFex ROM, or derived instruction tables while redistribution authority is unresolved.
- Keep local reverse-engineering checkouts outside tracked production sources.
- Build the first distributable Barr-inspired reference patch from documented DSP primitives, original project parameter choices, and legally redistributable generated fixtures.
- Any future exact ROM interpreter or ROM importer requires a separate ADR covering provenance, user-supplied data, clean-room boundaries, and distribution.

## Alternatives considered

### DPF/DGL

Advantages:

- Permissive ISC licensing.
- Small audio-plugin-focused framework.
- Proven by BarrVerb and supports relevant plugin formats.

Reasons not selected initially:

- The planned editor and inspection surface require more custom UI/platform work.
- JUCE provides a more direct path to a standalone audio application, device selection, host testing, accessibility primitives, and web/native UI experimentation.

DPF remains the strongest fallback if AGPL/commercial JUCE licensing or binary footprint becomes unacceptable.

### Native JUCE editor from the start

Advantages:

- Fewer runtime layers and no embedded browser deployment.
- Direct access to native accessibility and graphics facilities.

Reasons not selected for discovery:

- It spends early engineering time rebuilding graph-editor interactions before the product model has been validated.
- React Flow can validate node, port, connection, selection, and inspector behavior more quickly.

Native JUCE remains the defined production fallback at the M2.1 checkpoint.

### LiteGraph.js or Rete.js

LiteGraph is lightweight and MIT licensed but has a more tool-like Canvas2D presentation. Rete.js offers a larger visual-programming framework, but its processing abstractions are unnecessary here and some advanced packages use noncommercial terms. React Flow is the narrower and more presentation-flexible prototype choice.

### Tracktion Engine

Tracktion Engine provides a broad application/DAW engine, but its scope and separate GPL/commercial licensing add complexity not required for a small reverb constructor.

## Consequences

Positive:

- The plugin/device/build path is conventional and well documented.
- The UI can be prototyped rapidly while the graph schema and DSP remain independent.
- AGPLv3 is compatible with the stated open-source goal.
- Windows standalone and VST3 provide a focused first validation environment.

Costs and risks:

- AGPLv3 is a strong-copyleft commitment; closed-source or differently licensed distribution requires revisiting JUCE licensing.
- An embedded browser can increase binary size and host-specific UI risk.
- Maintaining a schema-driven native bridge adds discipline and test surface.
- macOS and Linux issues will not be discovered as early as Windows issues.

## Evidence and references

- JUCE repository and license: <https://github.com/juce-framework/JUCE>
- JUCE license terms: <https://github.com/juce-framework/JUCE/blob/master/LICENSE.md>
- JUCE 8 EULA: <https://juce.com/legal/juce-8-licence/>
- React Flow: <https://reactflow.dev/>
- Catch2: <https://github.com/catchorg/Catch2>
- JSON for Modern C++: <https://github.com/nlohmann/json>
- DPF: <https://github.com/DISTRHO/DPF>
- Tracktion Engine: <https://github.com/Tracktion/tracktion_engine>

## Review triggers

Create a superseding ADR if any of these occur:

- the project will not be distributed under AGPLv3;
- JUCE licensing or required modules become incompatible with project goals;
- the M2.1 webview checkpoint fails;
- Windows standalone/VST3 no longer represents the first product slice;
- a historical ROM or derived instruction data is proposed for distribution;
- a dependency introduces incompatible licensing or release-time network requirements.
