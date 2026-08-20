# Roadmap progress

Last updated: 2026-08-20

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
| M2.5 Add contextual teaching affordances | Complete | Dismissible learn cards; evidence/implementation labels; offline research reader; milestone demo |
| M3.1 Implement editable node creation and deletion | Complete | Seven schema-backed primitives; stable IDs; atomic delete/undo; required I/O; supported-node round trip; screenshot/video |
| M3.2 Implement typed connection editing | Complete | Mono branching; endpoint/type validation; replace/automatic Sum choices; structural undo; minimum-zoom hit target; video |
| M3.3 Compile acyclic graphs | Complete | Deterministic topological schedule; prepared immutable topology; reachability warnings; direct DSP comparisons; last-valid publication |
| M3.4 Compile feedback graphs | Complete | Deterministic SCC analysis; split-phase Delay execution; exact algebraic-loop paths; nested/multiple-loop tests; bounded compile fixture |
| M3.5 Plan and limit delay memory | Complete | Single prepared arena; exact requested/allocated inspection; 64 MiB budget; sample-rate republish safety; boundary tests |
| M3.6 Complete graph undo/redo and clipboard behavior | Complete | Unified 100-entry/8 MiB history; semantic/document hashes; clean marker; fresh-ID subgraph paste; screenshot/video |
| M4.1 Implement feedback-loop highlighting | Complete | Selected node/cable cycles; active/alternate styling; loop delay/elements; nested/shared fixtures; bounded maximum graph |
| M4.2 Implement impulse audition and capture | Complete | Safe 0.1 impulse; visible length/threshold/input policy; pre-gain stereo capture; prepared lock-free publication; deterministic repeats; screenshot/video |
| M4.3 Implement stereo impulse and decay view | Complete | Solid/dashed stereo lanes; Schroeder decay; bounded 1-256x zoom/pan; tested T30/refusal rules; short/Barr/bloom screenshots; video |
| M4.4 Implement live energy glow | Complete | Ten fixed 30 Hz RMS lanes; atomic seqlock snapshot; tested disable/drop safety; segmented node meters; width/glow cables; reduced motion; video |
| M4.5 Add resource and safety diagnostics | Complete | Static/live/prepared labels; callback timing; exact delay memory; clip counts; revision-bound safety events; edit/undo/recover workflow; complete synthetic violations |
| M5.1 Implement control-rate graph semantics | Complete | Typed parameter sockets; 1 kHz bounded plans; linear ramps; explicit mapping inspector; exact schema-v2 persistence; native 125%-DPI evidence |
| M5.2 Implement LFO and modulation mapping blocks | Complete | Tested sine/triangle and transport semantics; explicit scale/offset/polarity; one-to-many control branching; predicted range; screenshot/video evidence |
| M5.3 Add modulated Delay and Allpass parameters | Complete | Fractional linear taps; bounded coefficient; prepared control-runtime binding; constant/static equivalence; boundary, feedback, and moving-diffusion tests; inspector evidence |
| M5.4 Implement runtime topology publication | Complete | One-entry newest-wins compile queue; bounded block-entry swap; fixed retirement ring; worker reclamation; pending/active/failed diagnostics; 1,000-edit stress test |
| M5.5 Add short topology-change crossfades | Complete | Live schema-v2 editor binding; fixed 10 ms two-runtime ramp; newest-wins transition queue; constructed-graph capture/safety diagnostics; 125%-DPI fill fix; screenshot/video |
| M5.6 Finalize runaway-feedback UX | Complete | 50 ms sustained detector; immediate non-finite/hard-ceiling mute; heuristic loop marking; muted Undo; state-clearing recovery; scaled-work-area sizing; screenshot/video |
| M6.1 Define reverse-reverb architecture requirements | Complete | Four distinct signal/IR contracts; causal Reverse Envelope selected; follower/gate primitive boundary; factory naming guard; documentation tests |
| M6.2 Implement the minimum envelope/gate primitives | Complete | Sample-rate-aware follower/gate DSP; visible audio/control semantics; restricted detector route; exact persistence; delayed-feedback safety; screenshot/video evidence |
| M6.3 Add reverse and gated factory patches | Complete | Two public-primitive factories; deterministic multirate envelope/audio fixtures; UI selection; screenshots/video |
| M6.4 Add visualization teaching overlays | Complete | Measured reverse/gated landmarks; honest RT60 refusal; A/B switching; Learn control; screenshots/video |
| M7.1 Finalize project licensing and provenance | Complete | AGPL-3.0-only; notices, provenance, DCO policy; archive enforcement |
| M7.2 Package standalone and first plugin format | Complete | Reproducible Windows package; standalone/VST3; pluginval, VST3 validator, and two-host validation |
| M7.3 Complete user and developer documentation | Complete | Clean-install tutorial; complete module/view reference; clean-checkout guide; enforced links/commands |
| M7.4 Add factory patch and compatibility tests | Complete | Authoritative catalog; all-factory finite rendering/round trips; v1-v2 migrations; metadata; clean CI |
| M7.5 Run alpha usability and safety validation | In progress | Protocol/ledger and automated accessibility/preflight gates prepared; external participant sessions still required |
| M8.1 Specify Gravity behavior and measurements | Complete | Bipolar control contract; causal inverse boundary; deterministic stereo energy metrics; inverse/bloom/forward targets |
| M8.2 Implement a nonlinear Curve Mapper control block | Complete | Linear/power/exponential mappings; explicit bounds; schema-v2 compatibility; bounded runtime; inspector preview |
| M8.3 Add a visible Macro control-source block | Complete | Named normalized source; explicit branching; 20 ms runtime ramp; reachability/range inspection; exact persistence |
| M8.4 Add the Gravity macro presentation | Complete | Explicit designation; inverse/bloom/forward surface; non-measured envelope guide; Expand/Focus; Learn-off independence |
| M9.1 Design the eight-stage diffusion topology | Complete | 8 stage delays/taps; 12 allpasses; delayed damped return; odd/even stereo trees; exact 192 kHz memory/control/transition budgets; native compile regression |
| M9.2 Implement normalized Gravity weighting | Complete | 8 visible Curve Mapper branches; exact constant-sum stereo weights; monotonic measured envelope sweep; 2.1 dB energy ceiling; finite continuous automation |
| M9.3 Add Size, Feedback, Damping, and Modulation macros | Complete | 5 visible Macros; 35 explicit mappings; independent dual-LFO motion; 32 endpoint combinations; continuous-sweep and safety regressions |
| M9.4 Tune inverse, bloom, and forward reference states | Complete | Fixed five-Macro states; 5 s loudness-matched stereo WAVs; format-v1 measurements/hashes; causal envelope ordering; reload determinism; audition checklist |
| M9.5 Ship the factory patch and teaching view | Complete | Editable 58-node/94-cable factory; honest predicted/measured overlays; Barr/Gravity A/B; reconstruction guide; screenshots/video |
| M9.6 Validate and package the first Gravity implementation | Complete | Multi-rate factory renders; five-Macro sweeps; exact host-state restore; 100/125/150% physical captures; strict pluginval; binary identity/checksum gate |
| M10.1 Specify pitch-shift semantics and budgets | Complete | Mono dual-read-head contract; exact semitone ratios; forward/reverse grain boundary; fixed 600 ms causal latency; automation, quality, storage, and operation ceilings |
| M10.2 Implement the prepared dual-grain DSP | Complete | Prepared mono processor; causal dual read heads; equal-power overlap; 20 ms parameter transitions; octave, boundary, extreme, reset, canary, and latency tests |
| M10.3 Add the visible Pitch Shift block | Complete | Public mono graph block; exact runtime/host persistence; semitone/grain/overlap mapping; honest latency/quality/grain inspector; reduced motion; screenshot/video |
| M10.4 Validate octave identity and feedback safety | Complete | Multi-rate chord bands; fixed-Hz/Doppler contrast; forward/reverse delayed feedback; latch/mute/crossfade/recovery; Release CPU/storage/latency/alias report |
| M11.1 Design the parallel topology | Complete | 28 visible blocks; non-recirculating +12-semitone branch; independent controls; latency, memory, and normalization budgets |
| M11.2 Ship and teach Safe Parallel Shimmer | Complete | Editable factory; +12 halo/no-staircase measurement; teaching view; exact persistence; screenshots/video; clean local and main CI |
| M12.1 Implement independently bounded feedback paths | Complete | Separate normal/shifted gains and delayed returns; visible pre/post filters; 0.72 combined ceiling; invalid-publication and multirate safety tests |

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

## M2.5 verification

- Learn cards live below normal inspector controls and can be dismissed per context or disabled globally; the preference persisted across editor reload during interactive QA.
- Every card separates `DOCUMENTED BARR / MIDIVERB` from `THIS RECONSTRUCTION` and adds a concrete `LISTEN / NOTICE` prompt.
- Mono Sum explains the historical mono-summed input and this plugin's explicit 0.5 left/right sum.
- Left and Right Tap explain distinct stereo views of the same Tank 2 field; tests cover both branches.
- Tank explanations describe Barr's recirculating vocabulary while explicitly disclosing that the M2 reference omits its outer feedback cable.
- The complete [Keith Barr architecture research](keith-barr-reverb-architectures.md) is bundled into a local modal reader with no network dependency.
- Web tests cover the required content distinctions; interaction QA covers context switching, dismissal, disable/persistence, and research opening.
- Mono-sum screenshot: [`artifacts/ui/m2-5-contextual-teaching/mono-sum-teaching.png`](../artifacts/ui/m2-5-contextual-teaching/mono-sum-teaching.png).
- Offline research screenshot: [`artifacts/ui/m2-5-contextual-teaching/offline-research-reader.png`](../artifacts/ui/m2-5-contextual-teaching/offline-research-reader.png).
- Context switching/research video: [`artifacts/ui/m2-5-contextual-teaching/contextual-teaching.mp4`](../artifacts/ui/m2-5-contextual-teaching/contextual-teaching.mp4).
- M2 exit demonstration covering audition controls, continuous editing/undo, save/load validation, and contextual teaching: [`artifacts/ui/m2-5-contextual-teaching/m2-first-vertical-slice.mp4`](../artifacts/ui/m2-5-contextual-teaching/m2-first-vertical-slice.mp4).

## M3.1 verification

- Clicking the library creates Gain / Invert, Sum (`+`), Delay, Allpass, and Low-pass beside the required Stereo Input/Output blocks; all seven definitions have stable readable IDs and range-safe defaults.
- Duplicate required I/O and deletion of required I/O produce useful diagnostics. Schema-v1 loading independently requires exactly one of each.
- Selected-node deletion removes incident edges in the same immutable graph transaction. Undo Structure restores the exact node/edge snapshot; redo reapplies it.
- Supported created/deleted nodes, parameters, positions, cables, and viewport serialize and round-trip. Draft nodes do not publish values into the fixed native runtime.
- Primitive screenshot: [`artifacts/ui/m3-1-editable-node-creation/all-primitives.jpg`](../artifacts/ui/m3-1-editable-node-creation/all-primitives.jpg).
- Create/delete/undo video: [`artifacts/ui/m3-1-editable-node-creation/create-delete-undo.mp4`](../artifacts/ui/m3-1-editable-node-creation/create-delete-undo.mp4).

## M3.2 verification

- Real handle drags created two branches from the same Stereo Input output to different draft inputs.
- A second cable to the occupied Delay input was rejected without mutation and displayed Replace Cable, Insert `+`, and Cancel choices.
- Insert `+` atomically created a Sum block and three correctly typed replacement cables. Undo Structure restored the exact pre-insertion graph.
- Direction, endpoint, and signal-type validation prevents invalid connections from entering the editable graph. Schema-v1 loading also rejects multiple cables on one input; cycle legality remains the compiler's responsibility.
- A 24-unit invisible interaction stroke kept a cable selectable at the documented 40% minimum zoom.
- Occupied-input screenshot: [`artifacts/ui/m3-2-typed-connections/02-occupied-offer.jpg`](../artifacts/ui/m3-2-typed-connections/02-occupied-offer.jpg).
- Interaction video: [`artifacts/ui/m3-2-typed-connections/typed-connection-editing.mp4`](../artifacts/ui/m3-2-typed-connections/typed-connection-editing.mp4).

## M3.3 verification

- Lexicographically tie-broken topological scheduling is identical when semantic node and connection arrays are reversed.
- Disconnected, input-unreachable, and output-unreachable nodes have documented deterministic warnings and defined silence/discard behavior.
- Compilation prepares all graph signal buffers and primitive state off-thread; bounded `noexcept` processing does not resize storage and rejects oversized blocks with silence.
- Constructed Gain/Sum and Delay graphs match direct calculations. The complete acyclic Barr primitive graph matches `BarrReference` sample-for-sample.
- Failed validation/preparation does not exchange the active runtime pointer; a test confirms the last valid graph continues processing audio.
- UI unchanged; no screenshot or video was required.

## M3.4 verification

- Full-graph SCCs identify deterministic feedback components; cutting current-sample dependencies into Delay nodes produces a deterministic executable schedule.
- A one-sample Delay/Gain loop matches the direct `0, 0.5, 0.25, ...` recurrence and produces identical samples across host block partitions.
- A zero-delay two-Gain loop fails with the exact `gain-a -> gain-b -> gain-a` path. Algebraic sub-loops remain invalid even when another path in the SCC contains Delay state.
- Tests compile and render a nested two-loop component plus a separate independent feedback loop deterministically.
- Failed feedback publication leaves the running loop state audible and continuous.
- A 256-node graph with 64 feedback loops compiles below one second and uses less than 8 MiB of prepared routing/short-delay storage.
- UI unchanged; no screenshot or video was required.

## M3.5 verification

- Every compile result and publication result reports delay-line count, requested/allocated samples and bytes, and the 64 MiB project budget, including rejected plans.
- Delay and Allpass nodes use non-owning slices of one exactly sized runtime-owned arena. Repeated processing keeps arena and prepared-storage accounting fixed and performs no storage operation.
- Zero/negative delays fail, positive sub-sample values use the one-sample minimum, exactly 10 seconds succeeds, and values above 10 seconds fail before preparation.
- Allpass plans distinguish logical delay from the at-least-100-millisecond allocation required for continuous read-tap editing.
- A nine-by-ten-second graph fits and publishes at 44.1 kHz, recalculates above budget at 192 kHz, fails without allocating a new runtime, and leaves the preceding runtime active.
- Design and inspection contract: [Delay-memory planning](delay-memory-planning.md).
- UI unchanged; no screenshot or video was required.

## M3.6 verification

- One bounded stack records node, cable, layout, parameter, paste, automatic Sum, and reset transactions; toolbar, inspector, and keyboard controls traverse the same history.
- Automated mixed-edit traversal returns deterministic semantic hashes to every prior value. History retains at most the newest 100 transactions and 8 MiB of snapshots, and clears redo only after a new edit.
- Automatic Sum insertion restores the original occupied-input graph with one undo.
- Copy/paste assigns collision-free node/cable IDs, preserves parameter values and fully internal cables, omits required I/O, converts copies to draft nodes, offsets repeats, and commits atomically.
- The Saved/Unsaved marker uses semantic-plus-layout document identity. Successful save moves the marker without clearing history or mutating the semantic hash; load creates a new clean baseline.
- Screenshot: [`artifacts/ui/m3-6-unified-history/clipboard-paste.png`](../artifacts/ui/m3-6-unified-history/clipboard-paste.png).
- Copy/paste/undo/redo video: [`artifacts/ui/m3-6-unified-history/copy-paste-undo-redo.mp4`](../artifacts/ui/m3-6-unified-history/copy-paste-undo-redo.mp4).

## M4.1 verification

- Selecting a block finds simple directed cycles containing it; selecting a cable restricts results to cycles containing that exact edge. Acyclic selections report no loop.
- The active complete loop is solid amber. Other matching loops remain distinguishable in violet/dashed styling, and Previous/Next cycles the active result without changing graph semantics or history.
- The inspector lists directed constituent IDs, nominal total Delay/Allpass time, Gain/Sum/Allpass polarity and gain parameters, and Low-pass cutoff elements.
- Automated fixtures cover nested alternatives and two loops sharing a selected edge. Decoration tests prove source nodes/cables are unchanged.
- Inspection is capped at 64 results and 100,000 traversed transitions. A 256-block/512-cable stress fixture completes within the 250 ms test budget.
- Reproducible patch: [`artifacts/ui/m4-1-feedback-loop-highlighting/shared-edge-loop-fixture.rvp.json`](../artifacts/ui/m4-1-feedback-loop-highlighting/shared-edge-loop-fixture.rvp.json).
- Long-loop screenshot: [`artifacts/ui/m4-1-feedback-loop-highlighting/01-long-loop.png`](../artifacts/ui/m4-1-feedback-loop-highlighting/01-long-loop.png).
- Short-loop screenshot: [`artifacts/ui/m4-1-feedback-loop-highlighting/02-short-loop.png`](../artifacts/ui/m4-1-feedback-loop-highlighting/02-short-loop.png).
- Selection/cycling video: [`artifacts/ui/m4-1-feedback-loop-highlighting/loop-selection-and-cycling.mp4`](../artifacts/ui/m4-1-feedback-loop-highlighting/loop-selection-and-cycling.mp4).

## M4.2 verification

- The measurement strip exposes 500-10,000 millisecond maximum lengths, -60 through -120 dBFS stop thresholds, and an explicit live-input mute. Native callers are independently clamped to the documented safe finite bounds.
- A fixed 0.1-peak stimulus enters the normalized-sum boundary. Raw stereo capture occurs before master audition gain, while audible output still passes through gain, emergency mute, and numerical guards.
- Threshold completion requires an observed response followed by 100 milliseconds of continuous quiet; the maximum frame count is always a hard stop.
- Three fixed stereo slots are allocated during prepare. The audio callback writes and atomically publishes only; message-thread copying and JSON serialization cannot block it.
- Native tests repeat an unmodulated measurement with different live input and master gain and compare both channels sample-for-sample. Bridge tests reject non-finite or unequal channel payloads.
- Completed-capture screenshot: [`artifacts/ui/m4-2-impulse-audition-capture/03-complete.jpg`](../artifacts/ui/m4-2-impulse-audition-capture/03-complete.jpg).
- Ready/capturing/completed interaction evidence: [`artifacts/ui/m4-2-impulse-audition-capture/impulse-capture-interaction.mp4`](../artifacts/ui/m4-2-impulse-audition-capture/impulse-capture-interaction.mp4).

## M4.3 verification

- A completed stereo capture opens a response inspector with separate upper-solid `L` and lower-dashed `R` waveform lanes plus a labeled Schroeder energy-decay lane. Channel identity therefore does not depend on color.
- Full Tail, Early / 16x, power-of-two zoom through 256x, mouse-wheel zoom, and bounded pan expose both isolated early samples and the complete captured tail. Min/max waveform buckets retain extrema within a fixed 450-column presentation budget.
- Stereo squared energy is backward integrated and normalized before a tested T30 regression over -5 through -35 dB extrapolates RT60. A synthetic 0.75-second exponential fixture recovers 0.75 seconds.
- Empty, noisy/truncated, insufficient-range, and non-decaying captures withhold RT60 and display a specific explanation instead of a misleading value.
- Analysis runs only after the native real-time capture has published an immutable payload; no visualization work enters the audio callback.
- Short response screenshot: [`artifacts/ui/m4-3-stereo-impulse-decay/01-short-response.jpg`](../artifacts/ui/m4-3-stereo-impulse-decay/01-short-response.jpg).
- Runtime Barr full-tail screenshot: [`artifacts/ui/m4-3-stereo-impulse-decay/02-barr-reference-full-tail.png`](../artifacts/ui/m4-3-stereo-impulse-decay/02-barr-reference-full-tail.png).
- Runtime Barr early/panned screenshots: [`03-barr-early-16x.png`](../artifacts/ui/m4-3-stereo-impulse-decay/03-barr-early-16x.png) and [`04-barr-panned-16x.png`](../artifacts/ui/m4-3-stereo-impulse-decay/04-barr-panned-16x.png).
- Long/bloom full-tail and early screenshots: [`05-long-bloom-full-tail.jpg`](../artifacts/ui/m4-3-stereo-impulse-decay/05-long-bloom-full-tail.jpg) and [`06-long-bloom-early.jpg`](../artifacts/ui/m4-3-stereo-impulse-decay/06-long-bloom-early.jpg).
- Measurement/navigation video: [`stereo-response-measurement-and-navigation.mp4`](../artifacts/ui/m4-3-stereo-impulse-decay/stereo-response-measurement-and-navigation.mp4).

## M4.4 verification

- Ten fixed prepared lanes measure runtime-bound stereo I/O, Sum, Low-pass, and six Allpass outputs. The audio thread peak-holds block RMS and publishes at 30 Hz through atomic values guarded by a sequence lock; it never allocates, locks, serializes, or waits.
- Disabled telemetry performs one relaxed flag read per audio block, scans zero sample values, and publishes nothing. The UI simultaneously removes its polling timer and clears presentation decorations.
- Native tests compare a polled runtime with one whose snapshots are dropped and require every rendered sample to remain identical. A bounded message-thread copy is the only state exposed to JSON serialization.
- RMS maps logarithmically over -72 through 0 dBFS. Tested 42 millisecond attack, 260 millisecond release, and stale-generation decay produce responsive but stable activity.
- Five-segment node meters and increasing cable width encode intensity without color alone. Energy styling preserves amber selected-loop styling.
- The operating-system reduced-motion preference disables telemetry/animation and locks the explicit header toggle in **Energy Reduced** state.
- Idle screenshot: [`artifacts/ui/m4-4-live-energy-glow/01-energy-idle.png`](../artifacts/ui/m4-4-live-energy-glow/01-energy-idle.png).
- Entering, diffusing, and tail screenshots: [`02-impulse-entering.png`](../artifacts/ui/m4-4-live-energy-glow/02-impulse-entering.png), [`03-energy-diffusing.png`](../artifacts/ui/m4-4-live-energy-glow/03-energy-diffusing.png), and [`04-energy-tail.png`](../artifacts/ui/m4-4-live-energy-glow/04-energy-tail.png).
- Disabled screenshot: [`05-energy-off.png`](../artifacts/ui/m4-4-live-energy-glow/05-energy-off.png).
- Actual runtime impulse/video evidence: [`live-energy-impulse-recirculation.mp4`](../artifacts/ui/m4-4-live-energy-glow/live-energy-impulse-recirculation.mp4).

## M4.5 verification

- The diagnostics contract labels topology workload as `static-estimate`, callback CPU and clipping as `measured`, and delay storage as `prepared-allocation`; the UI repeats those evidence bases instead of presenting unlike values as equivalent measurements.
- Live CPU is measured against each audio block's deadline with bounded monotonic timestamps and lock-free scalar publication. Prepared memory is read from the six actual Allpass allocations; the browser never recomputes it.
- NaN, positive infinity, and finite runaway signals traverse the complete live harness in native tests. Both outputs mute, the event snapshot coherently records kind/channel/sample and the exact active graph revision, and no unsafe sample escapes.
- Runtime parameter edits advance native revision while safety-muted. Tests prove the event keeps its original revision, later edits remain accepted, and only an explicit reset clears state/latch and increments recovery count.
- The browser receives message-thread JSON made from atomic value copies, never a runtime pointer, span, mutable audio buffer, or stale object reference. Its parser rejects non-finite values, inconsistent mute state, and incoherent event exposure.
- Real native resource panel: [`artifacts/ui/m4-5-resource-safety-diagnostics/01-live-resource-panel.png`](../artifacts/ui/m4-5-resource-safety-diagnostics/01-live-resource-panel.png).
- Safety-latched edit/Undo state: [`02-safety-latched-editable.png`](../artifacts/ui/m4-5-resource-safety-diagnostics/02-safety-latched-editable.png) and [`03-safety-after-undo.png`](../artifacts/ui/m4-5-resource-safety-diagnostics/03-safety-after-undo.png).
- Explicit recovered state, with active revision advanced and event revision retained: [`04-recovered-revision.png`](../artifacts/ui/m4-5-resource-safety-diagnostics/04-recovered-revision.png).
- Native 125%-scale window with the schematic filling the complete embedded-browser bounds: [`05-full-window-125-percent.png`](../artifacts/ui/m4-5-resource-safety-diagnostics/05-full-window-125-percent.png).
- Resource/safety/Undo/recovery video: [`diagnostics-undo-and-recovery.mp4`](../artifacts/ui/m4-5-resource-safety-diagnostics/diagnostics-undo-and-recovery.mp4).

## M5.1 verification

- Parameter sockets are typed control inputs. Native and browser validation reject audio/control endpoint mismatches, duplicate control inputs, invalid mapping sockets, non-finite amounts, invalid polarity, and out-of-range clamps.
- Prepared control plans use a nominal 1 kHz rate, a sample-rate-derived quantum, linear target ramps, and explicit 64-node/128-mapping budgets. Tests prove the effective `clamp(base + amount × normalizedControl)` calculation and the per-block evaluation bound.
- The inspector keeps each base value beside its control socket, formula, polarity, amount, clamp range, and `1 kHz control ticks · linear interpolation to audio rate` policy.
- Patch schema v2 saves every mapping field. Both native and browser round trips require exact preservation, while schema-v1 fixtures migrate deterministically and remain readable.
- Runtime snapshot contract v2 derives parameter socket identities and defaults from the native Barr runtime definition, and identity tests reject drift between the graph, UI, and DSP contract.
- Native screenshot: [`artifacts/ui/m5-1-control-rate-semantics/01-mapped-allpass-inspector.png`](../artifacts/ui/m5-1-control-rate-semantics/01-mapped-allpass-inspector.png).
- Native mapping inspector with live telemetry: [`artifacts/ui/m5-1-control-rate-semantics/control-mapping-inspection.mp4`](../artifacts/ui/m5-1-control-rate-semantics/control-mapping-inspection.mp4).

## M5.2 verification

- Native `ControlLfo` tests cover frequency, phase, sine and triangle waveforms, phase wrapping, explicit restart, transport restart, and free-run reset behavior.
- Scale / Offset clamps finite normalized input after explicit bipolar or unipolar polarity handling. Tests cover positive, negative, offset, clamp, and predicted endpoint ranges.
- Native control-plan compilation records validated LFO and mapper definitions within the existing 64-node/128-mapping bounds. Browser graph tests prove a single mapper output branches to multiple parameter sockets.
- Patch schema v2 and browser persistence preserve LFO, mapper, waveform/run-mode/polarity units, positions, and branched control cables exactly.
- Control nodes combine a dashed outline, double rule, textual waveform/mapping marker, signed value, and moving position marker. Control cables remain dashed and animate during live preview, so neither depends on colour alone.
- Creation, mapping, branching, and predicted-range screenshot: [`artifacts/ui/m5-2-lfo-control-mapping/01-lfo-mapper-branching.png`](../artifacts/ui/m5-2-lfo-control-mapping/01-lfo-mapper-branching.png).
- Eight-second native live-preview video: [`artifacts/ui/m5-2-lfo-control-mapping/live-control-preview.mp4`](../artifacts/ui/m5-2-lfo-control-mapping/live-control-preview.mp4).

## M5.3 verification

- Delay time uses a two-point fractional linear circular-buffer read. Connected mapping clamps determine prepared storage plus one interpolation guard sample, and existing line-count/64 MiB rejection occurs before construction.
- Allpass time uses the same moving tap; coefficient control is evaluated per sample and hard-limited to `-0.95` through `+0.95`.
- Prepared control operations run at 1 kHz and fill preallocated audio-rate ramp buffers. Processing performs no allocation, resizing, locking, logging, or I/O.
- Native tests prove constant mapped control matches equivalent static Delay and Allpass output, and stress minimum/maximum time, full-depth sweeps, extreme coefficient values, and finite output.
- A modulated Delay feedback loop renders identically across different host block partitions. Existing invalid-publication tests continue to protect the last valid runtime.
- A compiled visible LFO -> Scale / Offset -> Allpass graph branches one control output into delay and coefficient and renders finite non-silent moving diffusion.
- The inspector labels fractional linear interpolation, the 100 Hz control-source ceiling, intentional Doppler/pitch effects, and the coefficient stability boundary.
- Reviewed native screenshot: [`artifacts/ui/m5-3-modulated-delay-allpass/01-allpass-modulation-policy.png`](../artifacts/ui/m5-3-modulated-delay-allpass/01-allpass-modulation-policy.png).
- UI animation did not change, so no new video was required; M5.2 retains the live-control animation evidence and native render tests cover the new DSP behavior.

## M5.4 verification

- Every asynchronous request receives a monotonic revision. The compiler worker owns validation, preparation, allocation, and replacement of obsolete queued or pending work.
- Audio checks one pending pointer at block entry. A successful publication uses lock-free pointer/scalar atomics and at most one fixed-ring write; the runtime remains fixed for the rest of that block.
- Previous active envelopes enter a fixed 16-slot SPSC retirement ring and are deleted by the compiler worker. When it has no capacity, audio keeps the current graph and defers the swap.
- The request queue and prepared pending slot each hold at most one item. Newer edits supersede older work instead of creating an unbounded backlog.
- Diagnostics expose requested, pending, active, failed, superseded, completed, and reclaimed identities/counts plus the latest failure text.
- A state test observes pending -> active, submits an invalid graph, observes its failed revision, and proves the preceding valid output remains audible.
- A stress test submits 1,000 graphs while a separate audio thread continuously processes. It requires finite output, processed blocks, final-revision activation, superseded work, fewer compilations than requests, and off-thread reclamation.
- Existing synchronous publication and feedback-failure tests remain green.
- UI unchanged; no screenshot or video was required.

## M5.5 verification

- Audible editor semantics are debounced for 35 ms, parsed through the native schema-v2 contract, compiled away from audio, and identified as requested, pending, active, or failed revisions.
- A valid replacement executes beside the preceding graph for exactly 10 ms with a linear old-to-new stereo ramp. Only two runtimes execute; all scratch storage is prepared, and abandoned state is reclaimed by the worker.
- A newer prepared graph waits for the active transition; still-newer requests replace that one slot. The 1,000-edit concurrent stress test reaches the final revision with finite output, an empty pending slot, a finished transition, bounded compilations/crossfades, and reclaimed runtimes.
- Legal feedback-path changes remain finite. Invalid compilation leaves the preceding audible runtime active. Constructed feedback reset produces repeatable measurement output.
- Constructed-graph mode retains audition gain, manual mute, impulse injection, deterministic pre-gain capture, numerical guards, live CPU/clipping accounting, active revision identity, and prepared delay-memory reporting.
- JUCE's logical display bounds are used once and WebView2 receives its parent's exact remaining logical bounds. At the tested 125% scale, the schematic fills all space below the intentional native control strip without a second scale conversion.
- Reviewed native screenshot: [`artifacts/ui/m5-5-topology-crossfade/01-live-crossfade-diagnostics.png`](../artifacts/ui/m5-5-topology-crossfade/01-live-crossfade-diagnostics.png).
- Reviewed live-edit video: [`artifacts/ui/m5-5-topology-crossfade/live-topology-edit.mp4`](../artifacts/ui/m5-5-topology-crossfade/live-topology-edit.mp4). The video is visual-only; native render tests prove finite audible transitions.

## M5.6 verification

- NaN/infinity and a finite sample above absolute 16 mute immediately. Absolute output above 4 for exactly 50 continuous milliseconds also mutes; a lower sample resets the consecutive count, making the result independent of host block partitions.
- A latched constructed graph stays silent while edits and Undo remain available. The first event retains its kind, channel, sample index, and graph revision.
- The editor searches explicit Delay-containing cycles within the existing 100,000-transition bound, ranks by visible absolute loop gain divided by nominal delay, labels the result heuristic, and marks the highest-ranked path in red. Absence of an identifiable loop is reported explicitly.
- Explicit recovery resets active and crossfading constructed runtimes before clearing both guards, so abandoned delay/feedback energy cannot resume. The end-to-end native test remains muted through a safe publication and recovers to deterministic silence.
- The preferred 1280-by-800 editor size is clamped to JUCE's logical desktop work area. Native QA at 125% on a 1536-by-960 display kept the complete diagnostics panel and recovery actions reachable.
- Shared Debug verification passed 49 browser tests and 80 native/audio tests with warnings as errors.
- Latched screenshot: [`artifacts/ui/m5-6-runaway-feedback/01-runaway-loop-and-recovery.png`](../artifacts/ui/m5-6-runaway-feedback/01-runaway-loop-and-recovery.png).
- Recovered screenshot: [`artifacts/ui/m5-6-runaway-feedback/03-recovered-silence.png`](../artifacts/ui/m5-6-runaway-feedback/03-recovered-silence.png).
- Safe-reset/Undo/recovery video: [`artifacts/ui/m5-6-runaway-feedback/runaway-mute-edit-undo-recovery.mp4`](../artifacts/ui/m5-6-runaway-feedback/runaway-mute-edit-undo-recovery.mp4).

## M6.1 verification

- Product language separately defines true sample-order reversal, a causal rising-envelope approximation, level/fixed gated reverb, and Bloom-like slow diffusion. Each definition states its audible behavior and impulse-response evidence.
- **Causal Reverse Envelope** is the chosen first method. Its visible increasing Delay/Gain branches are causal, real-time feasible, and explicitly do not claim lookahead, pre-echo, or literal reversal.
- The chosen reverse construction requires no hidden primitive. M6.2 is bounded to two reverb-specific additions for gated behavior: a visible-control Envelope Follower and a control-driven Hold Gate with millisecond timing.
- Normative naming rules prohibit a bare **Reverse** label and specifically prohibit calling a patch reverse solely because it has a long diffuse attack. Bloom requires a smooth nonzero decay; Gated requires measurable truncation.
- Three browser documentation-contract tests enforce the four concepts, selected architecture, primitive boundary, and naming prohibition.
- Source notes distinguish primary product behavior descriptions from inferred topology. Deferred convolution, lookahead, IR import, and polyphonic trigger work is recorded without expanding M6.2.
- UI unchanged; no screenshot or video was required.

## M6.2 verification

- Envelope Follower visibly converts mono audio to normalized `0...1` control with an absolute-peak one-pole detector, separate attack/release milliseconds, finite clamping, and deterministic reset.
- Hold Gate visibly multiplies mono audio by detector-derived gain. Threshold, attack, hold, and release are explicit; retriggers reload hold and resume attack from current gain; gain remains within `0...1`.
- Millisecond behavior is deterministic at 44.1, 48, and 96 kHz. Focused tests cover the follower's one-time-constant response and exact gate attack/hold/release samples.
- The control route is deliberately reverb-specific: Envelope Follower may drive Hold Gate directly or through exactly one Scale / Offset. LFO and broader control routing fail before publication.
- Base-only follower/gate controls omit misleading modulation sockets. Browser and native schema-v2 round trips preserve their exact values, ports, cables, positions, and absence of hidden mappings; existing mapped parameters remain exact.
- A legal delayed feedback fixture containing Hold Gate stays finite, never exceeds its injected peak, and produces no numerical-safety violation. Processing remains allocation/lock/log free with all state and buffers prepared before publication.
- Reviewed native screenshot: [`artifacts/ui/m6-2-envelope-hold-gate/01-envelope-follower-hold-gate.png`](../artifacts/ui/m6-2-envelope-hold-gate/01-envelope-follower-hold-gate.png).
- Reviewed module-inspection video: [`artifacts/ui/m6-2-envelope-hold-gate/envelope-gate-module-inspection.mp4`](../artifacts/ui/m6-2-envelope-hold-gate/envelope-gate-module-inspection.mp4). It shows selection moving between both new blocks in the native editor; deterministic native tests are authoritative for signal timing and safety.

## M6.3 verification

- Two checked-in schema-v2 factory documents use only public editable primitives. Native and browser tests independently load and round-trip both; constructed graphs execute through the public compiler/runtime rather than a patch-identity DSP shortcut.
- **Causal Reverse Envelope** exposes three visible increasing Delay/Gain branches (45/115/210 ms and 0.25/0.55/0.95), explicit Sum blocks, diffusion Allpasses, 6.5 kHz tone, 0.75 level, and unequal stereo output diffusion.
- **Level-Gated Room** exposes three diffusion Allpasses, 7.2 kHz tone, 0.8 level, one 0.1/20 ms Envelope Follower, and two 0.004-threshold, 2/120/8 ms Hold Gates. The detector opens for both a unit fixture and the product's safe 0.1-peak audition impulse at 44.1, 48, and 96 kHz.
- At 48 kHz the reverse patch reaches its smoothed peak 195 ms after onset and drops at most 3.87 dB per 10 ms window. The gated patch peaks at 5 ms, crosses -40 dB 205 ms later, and has an 84.79 dB single-window cutoff. RT60 is marked meaningful only for the gradual reverse-envelope tail.
- Both patches remain finite and at or below unity for one-second impulses and bounded stereo-noise stress at 44.1, 48, and 96 kHz. Residual post-cutoff energy remains below `1e-4`; stereo outputs differ.
- Checked-in deterministic WAV, analysis, response-measurement, and envelope-measurement fixtures live under [`artifacts/audio/m6-3-factory-patches/`](../artifacts/audio/m6-3-factory-patches/).
- The header factory menu loads Barr/reference, reverse-envelope, and gated graphs, follows their identity in graph/save/reset labels, and publishes each visible graph for continuous audition. Wide factories may fit below 40% without changing the user's general zoom range.
- The Windows sizing defect is removed at both boundaries: the editor no longer divides JUCE logical work-area dimensions by scale, and WebView2 receives exact parent bounds. Full-window evidence: [`00-barr-reference-full-window.png`](../artifacts/ui/m6-3-factory-patches/00-barr-reference-full-window.png).
- Reviewed factory screenshots: [`01-causal-reverse-envelope.png`](../artifacts/ui/m6-3-factory-patches/01-causal-reverse-envelope.png) and [`02-level-gated-room.png`](../artifacts/ui/m6-3-factory-patches/02-level-gated-room.png).
- Reviewed selection/publication video: [`factory-patch-switching.mp4`](../artifacts/ui/m6-3-factory-patches/factory-patch-switching.mp4). Deterministic offline fixtures and native tests are authoritative for audio-envelope differences.

## M6.4 verification

- Response overlays derive onset, peak, and -40 dB cutoff from the displayed stereo capture's non-overlapping 10 ms energy windows. They clip to zoom/pan rather than pinning off-screen landmarks to a false edge.
- Causal Reverse Envelope marks **Rising Energy**, **Late Peak**, and -40 dB. Its visible explanation explicitly says weighted delays construct a causal swell, do not reverse sample order, and cannot place wet sound before the trigger.
- Level-Gated Room marks **Gate Open**, **Hold**, **Release**, and measured **Cutoff** using the captured graph's visible follower/gate milliseconds. Copy identifies level-triggered retriggering rather than a fixed window.
- Gated captures label RT60 **Not Meaningful** and explain that abrupt truncation invalidates exponential extrapolation, even when a pre-cutoff T30 regression can be computed.
- **A / Barr** and remembered **B / Reverse Env** or **B / Gated** buttons publish ordinary visible factory graphs through the existing off-thread compiler and 10 ms crossfade. Text plus `aria-pressed` exposes active state.
- The existing persistent **Learn On/Off** control now governs inspector cards, response regions, and architecture prose together. Learn Off retains raw waveform, decay, metrics, and honest RT60 refusal.
- The live product's 0.1-peak audition exposed a formerly silent gated factory despite unit-fixture success. The visible threshold is now 0.004; a new native regression proves non-silent, finite, non-amplifying response at 44.1, 48, and 96 kHz under that exact stimulus.
- Eighteen browser files / 62 tests cover overlay landmarks and text boundaries, silent/Barr/custom refusal, abrupt-cutoff explanation, and remembered comparison target.
- Reverse overlay: [`01-reverse-rise-peak-overlay.png`](../artifacts/ui/m6-4-teaching-overlays/01-reverse-rise-peak-overlay.png). Gated overlay: [`02-gated-hold-cutoff-overlay.png`](../artifacts/ui/m6-4-teaching-overlays/02-gated-hold-cutoff-overlay.png). Disabled teaching: [`03-learn-off-raw-response.png`](../artifacts/ui/m6-4-teaching-overlays/03-learn-off-raw-response.png).
- Reviewed A/B and response-overlay video: [`ab-response-overlays.mp4`](../artifacts/ui/m6-4-teaching-overlays/ab-response-overlays.mp4). Native captures are visual/audio-path evidence; deterministic tests remain authoritative for multirate timing and bounds.

## M7.1 verification

- The accepted AGPLv3 open-source decision is now executable: the complete AGPL-3.0-only text is at the repository root and the README links the license, notices, provenance, and contribution policy.
- `THIRD_PARTY_NOTICES.md` records every direct native/runtime/build dependency and the complete bundled production JavaScript license families. Pinned versions remain authoritative in `CMakeLists.txt` and `web/pnpm-lock.yaml`.
- `ASSET_PROVENANCE.md` covers every tracked binary/generated-data family, its preferred source or generator, ownership boundary, and redistribution terms. No external font, icon, photo, sample library, impulse response, or model asset is bundled.
- The Barr reference contains only original public-primitive code/graphs, project-authored parameter choices, and generated fixtures. BarrVerb, transformed `rom.h`, firmware, and decoded ROM instructions remain research-only and prohibited from tracked/archive paths.
- `CONTRIBUTING.md` requires redistributable provenance, proportionate tests/docs/UI evidence, real-time safety, and a DCO 1.1 sign-off. `DCO` preserves the verbatim certificate; copyright remains with contributors.
- Repository verification rejects excluded research/build roots, ROM/firmware filenames and extensions, and undocumented asset families in both the tracked file set and a real `git archive` listing.
- UI unchanged; no screenshot or video was required.

## M7.2 verification

- A clean-tree Windows packaging command builds Release standalone/VST3 artifacts, stages licenses and notices, writes version/commit `build-info.json`, normalizes archive order/time/permissions, and emits an adjacent SHA-256 checksum. A Python regression proves identical inputs produce identical ZIP bytes.
- The package includes a current-user VST3 installer and a clean-machine-oriented installation/removal guide. The packaged standalone launches directly without an installer or development checkout.
- Tracktion pluginval 1.0.4 passed at strictness 10 across scan/open/editor/state/automation/thread/bus/fuzz checks and 44.1/48/96 kHz processing at five block sizes.
- Steinberg VST3 validator 3.8.1 build 84 passed 47/47 tests. Its first run exposed an unnamed program and low-rate filter exception; both are fixed and covered by regressions.
- JUCE 8.0.13 AudioPluginHost scanned and instantiated the Release VST3, routed stereo input/output as four mono host cables, kept processing active at 48 kHz, and passed its state save/load command.
- A 13-block/16-cable Level-Gated Room selected through keyboard input survived editor close/reopen and host state round-trip with parameters, layout, and viewport intact. Valid graph edits notify hosts that non-parameter state changed.
- At 125% Windows scaling, the editor resized from its preferred size to a maximized 1536-by-960 host window and filled all available content bounds. The former 1920-by-1200 maximum is replaced by an effectively non-limiting native bound.
- Version `0.1.0` and the 12-character source commit are visible in the editor and repeated in the package metadata.
- Full method and release decision: [`windows-alpha-package-and-host-validation.md`](windows-alpha-package-and-host-validation.md). UI evidence: [`artifacts/ui/m7-2-windows-package/`](../artifacts/ui/m7-2-windows-package/).

## M7.3 verification

- A clean-install [Barr reference tutorial](getting-started-barr-tutorial.md) now covers package identity, safe audition, explicit stereo/mono signal tracing, energy telemetry, bounded impulse capture, waveform/decay inspection, continuous Allpass editing, diagnostics/recovery, and patch save/load.
- The consolidated [module and visualization reference](module-and-visualization-reference.md) documents all 11 shipped modules and seven visualization families. It records every socket, unit, inclusive range, default, step, modulation boundary, measurement range, and important safety or interpretation constraint.
- The [development guide](development.md) now begins at `git clone`, lists the complete Windows/Node/pnpm/Python toolchain, provides the one-command CI-equivalent verifier and its canonical component commands, locates both built formats, and explicitly requires no BarrVerb or MIDIVerb research checkout.
- `check_documentation.py` derives the shipped module set from `web/src/modules.ts`, requires one reference marker per module/visualization, and locks the clean-checkout commands plus required tutorial actions. Three focused regressions prove missing modules, visualizations, commands, and tutorial steps fail the check.
- The repository's existing Markdown checker verified all local targets, including the new tutorial screenshot and cross-document links. The tutorial uses the reviewed packaged 0.1.0 standalone screenshot from [`artifacts/ui/m7-2-windows-package/03-packaged-standalone.png`](../artifacts/ui/m7-2-windows-package/03-packaged-standalone.png), which was visually inspected against the released editor. UI unchanged; no new capture was required.
- Full Debug verification passed 19 browser files / 64 tests, six Python policy/documentation tests, the production web build, standalone/VST3 compilation, and all 99 native/audio tests.

## M7.4 verification

- [`factory-patches/catalog.json`](../factory-patches/catalog.json) is now the authoritative shipped set: Barr reference, reverse-style, and gated. Every entry is marked complete and declares document kind/path, schema and engine versions, SPDX license/file, and project-authored provenance/source/description.
- Barr remains generated from the native graph/runtime identity instead of a duplicate JSON document. Reverse-envelope, gated, and the catalog itself are deterministic output from the factory generator; `--check` is now a full-verifier gate against checked-in drift.
- One catalog-derived native loop loads, graph-validates, compiles, impulse-renders, version-checks, serializes, reparses, and byte-stably rewrites all three factories. A second loop applies bounded stereo noise to every catalog entry at 44.1, 48, and 96 kHz and requires finite output at or below unity.
- Browser tests independently require exact catalog/menu identity and complete license/provenance metadata while preserving existing visible-public-primitive and schema-v2 round trips for checked-in documents.
- A shared schema-v1 fixture now proves native and browser readers preserve a legacy Gain value, graph, layout, and viewport; add the defined `gain-mod` mapping; emit schema v2; reparse identically; and produce a byte-stable second write. The tests explicitly bind the released readable range of v1 through v2.
- Short-room/baseline and Bloom-like families remain intentionally absent until their product topology, behavioral fixtures, metadata, and full compatibility evidence are complete. The admission rule is documented in [Factory patch catalog and compatibility](factory-patch-compatibility.md).
- Full Debug verification passed 19 browser files / 66 tests, six Python policy/documentation tests, deterministic factory generation, the production web build, standalone/VST3 compilation, and all 101 native/audio tests.
- UI unchanged; no screenshot or video was required.

## M7.5 preparation (milestone remains open)

- The participant protocol now covers clean installation/identity/safe mute, the complete Barr tutorial, a legal delayed feedback loop, an intentionally invalid algebraic cycle and repair, visible LFO mapping, save/close/reopen, response/energy/diagnostic inspection, and explicit stop rules.
- Three anonymous non-implementer sessions are required, including plugin/node experience diversity and a keyboard-only segment. The ledger prohibits names, contact/account/location data, faces, voices, and unrelated screen content; outcomes and findings use `Pnn` and severity IDs only.
- A preparation checker locks every required journey, keyboard/contrast/non-color/scaling/reduced-motion coverage, privacy template, and P0/P1 inventory. Its separate `--release` mode currently fails by design until three external sessions exist and both blocker counts are exactly zero.
- Static accessibility verification checks reviewed foreground/background pairs against 4.5:1, required focusable graph/ARIA/non-color tokens, the reduced-motion rule, resizable bounds, and exact WebView fill behavior. Three formerly low-contrast 8-9 px labels now use `#788892`, yielding approximately 4.74-5.26:1 on their real surfaces.
- The internal identity/visual preflight exposed a stale CMake cache commit (`908b1f9d9a7a` while HEAD was `a3e890646bf0`). Git checkouts now force-refresh identity on every configure; the verifier compares the configured cache with HEAD. Explicit overrides remain available only when Git identity is unavailable, such as a source archive.
- Findings and resolution evidence are in [Alpha validation findings](alpha-validation-findings.md). Discovery screenshot: [`01-contrast-reviewed-full-window.png`](../artifacts/ui/m7-5-alpha-validation-preparation/01-contrast-reviewed-full-window.png). Corrected identity/contrast screenshot: [`02-current-identity-and-contrast.png`](../artifacts/ui/m7-5-alpha-validation-preparation/02-current-identity-and-contrast.png).
- Full preparation verification passed 19 browser files / 66 tests, 13 Python policy/documentation/accessibility/study tests, factory generation and build-identity gates, the production web build, standalone/VST3 compilation, and all 101 native/audio tests.
- This preparation and internal dry run do **not** satisfy the roadmap's non-implementer requirement. M7.5 remains open pending external sessions, prioritized findings, fixes, and rerun evidence.

## M7.6 release preparation

- M7.5 is explicitly deferred rather than marked complete. The alpha notes say
  that its three non-implementer sessions have not run and avoid representing
  this prerelease as externally usability- or accessibility-validated.
- Tag `v0.1.0-alpha.1` drives a Windows Release verifier, deterministic package
  build, retained workflow artifact, and GitHub prerelease publication. The
  release receives the ZIP, adjacent SHA-256 file, checked-in notes, and demo.
- Release notes enumerate Windows 10/11 x64, Standalone/VST3 support, major
  construction/inspection capabilities, known platform/processing/factory and
  validation limitations, and a GitHub Issues feedback channel.
- The repository landing page links directly to the downloadable prerelease,
  installation, Barr tutorial, roadmap, issue reporting, and demonstration.
- The reviewed 38.7-second, 1280-by-720 H.264 demonstration has four labeled
  chapters: Barr Reference, Construct and Edit, Modulate, and Capture and
  Inspect. It is generated solely from earlier reviewed project UI recordings.
  Evidence: [`reverb-playground-alpha-demo.mp4`](../artifacts/ui/m7-6-alpha-release/reverb-playground-alpha-demo.mp4).
- `check_release.py` locks product/tag/package identity, tag-workflow gates,
  required note disclosures, landing-page links, and a nontrivial demo asset.

## M8.1 verification

- Gravity is fixed as a normalized, unitless `-1...+1` control: negative is
  Inverse, zero is the default/reset Bloom detent, and positive is Forward.
- The contract separates envelope shape from Size, Feedback/Decay, Damping,
  and Modulation, and requires all eventual mappings to remain visible.
- Negative Gravity is explicitly causal and distinct from sample reversal,
  reverse-grain processing, offline pre-reverb, and undisclosed lookahead.
- Stereo energy, 20 ms smoothing, tie handling, time to peak, early/late ratio,
  peak level, integrated energy, post-peak energy, decay slope, and RT60 refusal
  behavior have deterministic definitions.
- Fixed `-1`, `0`, and `+1` reference targets establish ordered inverse-rise,
  clustered/Bloom, and forward-decay behavior without prematurely tuning M9's
  numeric topology tolerances.
- Official Eventide descriptions are cited as behavioral inspiration only; the
  planned graph, mappings, measurements, and UI are identified as original.
- The documentation checker and its regression test require the complete
  Gravity contract. UI unchanged; no screenshot or video was required.

## M8.2 verification

- The released `control-map` node is now labeled Curve Mapper and exposes
  Linear, Power, and Exponential families, curve amount, exponent, scale,
  offset, polarity, and explicit output clamps.
- Native and browser evaluators share the documented normalized equations.
  Linear mode matches the original Scale / Offset result; sampled unipolar and
  bipolar Power/Exponential sweeps are monotonic and finite at both endpoints.
- The prepared 1 kHz control runtime performs constant bounded scalar work and
  uses the existing audio-rate linear ramp for connected parameter targets.
  Invalid selectors, non-finite fields, out-of-range values, and reversed
  clamps fail compilation before runtime publication.
- Newly saved schema-v2 nodes round-trip every curve field exactly. Released
  three-parameter schema-v2 Scale / Offset nodes load as neutral Linear Curve
  Mappers and write the complete representation on their next save.
- The inspector names the active family without relying on color, animates the
  mapped value, and previews output endpoints after curve, scale, offset, and
  clamping. Screenshot:
  [`curve-mapper-inspector.png`](../artifacts/ui/m8-2-curve-mapper/curve-mapper-inspector.png).
  Six-second interaction evidence:
  [`curve-mapper-preview.mp4`](../artifacts/ui/m8-2-curve-mapper/curve-mapper-preview.mp4).

## M8.3 verification

- Macro is a public control-source block with a 1–64 character user name,
  normalized current/default values, optional center detent, and exactly one
  explicit dashed control output.
- Its output branches through ordinary visible Curve Mapper nodes. Selecting a
  Macro traverses those cables, highlights every reachable mapper/destination,
  and lists each mapped parameter's predicted endpoint range and unit.
- Current values enter a fixed 64-slot lock-free mailbox keyed by stable node
  ID and ramp over 20 control ticks (20 ms). The current value is excluded from
  the audible topology fingerprint, so 1,000 rapid automation writes request no
  compilation; structural Macro settings still publish normally.
- Reset clears pending automation and returns to the prepared default.
  Compilation rejects invalid routes, malformed settings, slot collisions, and
  graphs beyond the existing 64-control-node/128-mapping budgets.
- Unified history includes Macro names; copy/paste retains name/settings while
  assigning fresh IDs. Schema-v2 save/load and native host state preserve the
  name, values, stable node/cable IDs, layout, and viewport exactly. Existing
  schema documents remain readable because `name` is optional outside the
  required Macro contract.
- UI evidence: [`macro-destinations.png`](../artifacts/ui/m8-3-macro-control/macro-destinations.png)
  and [`macro-automation.mp4`](../artifacts/ui/m8-3-macro-control/macro-automation.mp4).

## M8.4 verification

- Schema v2 and native host state preserve an explicit `gravity` presentation
  designation. It is accepted only for Macro nodes and has no DSP or routing
  meaning; ordinary Macros remain unchanged.
- The instrument surface labels Inverse, Bloom, and Forward, retains a signed
  keyboard-editable value and center detent, and drives the existing 20 ms
  runtime-only Macro path.
- A continuously updating envelope guide orders late/center/early design peaks
  and states **Not Measured Audio** in text and accessibility output. Actual
  response still requires impulse capture.
- The dashed/non-color reachability highlight and predicted parameter list
  remain visible while Gravity moves. Expand / Focus fits the complete
  Macro/Mapper/destination set from that same bounded traversal; reduced motion
  removes its animation.
- Learn Off affects only optional teaching cards and response annotations. It
  does not remove or disable the Gravity control, exact value, prediction,
  mapped destinations, highlighting, or focus action.
- Evidence: [`01-gravity-inverse.png`](../artifacts/ui/m8-4-gravity-presentation/01-gravity-inverse.png),
  [`02-gravity-bloom.png`](../artifacts/ui/m8-4-gravity-presentation/02-gravity-bloom.png),
  [`03-gravity-forward-learn-off.png`](../artifacts/ui/m8-4-gravity-presentation/03-gravity-forward-learn-off.png),
  and [`gravity-sweep-focus.mp4`](../artifacts/ui/m8-4-gravity-presentation/gravity-sweep-focus.mp4).

## M9.1 verification

- The checked-in Mermaid diagram and exact table specify four input Allpasses,
  eight stage Delay/Allpass pairs, eight progressively deeper tap gains,
  balanced odd/even stereo Sum trees, return damping/gain, and an explicit
  97 ms feedback Delay. Every compact diagram box is declared as separate
  public primitives and mono cables.
- The fully expanded design is 43 audio nodes and 51 audio cables. A native
  regression permits only public audio node types, requires 12 Allpasses and
  eight tap gains, and compiles it through the existing feedback validator at
  44.1, 48, 96, and 192 kHz.
- All rates produce exactly one legal feedback component, no algebraic-loop
  diagnostic, and 21 delay-bearing lines. At 192 kHz the exact arena is 325,836
  float samples / 1,303,344 bytes, leaving 98.1% of the 64 MiB graph budget.
- The planned M9.2/M9.3 graph uses five Macros, eight visible Curve Mappers, two
  LFOs, and 35 explicit parameter mappings: 15/64 control nodes and 35/128
  mappings. No hidden destination table is introduced.
- Conservative implementation ceilings are 320 scalar/buffer operations per
  sample for one runtime and 640 during the existing 10 ms two-runtime
  transition; maximum crossfade delay arenas total 2,606,688 bytes. M9.6 must
  replace these planning ceilings with measured host evidence.
- UI unchanged. The checked-in topology diagram is the visual deliverable for
  this design-only task; no product screenshot or interaction video was needed.

## M9.2 verification

- The production Gravity Diffusion builder expands the M9.1 audio skeleton with
  one prominent Gravity Macro, eight named linear Curve Mappers, eight visible
  control branches, and eight tap-gain modulation sockets. It introduces no
  factory-only audio node or hidden destination table.
- Paired affine weights keep the eight-tap sum exactly `1.0`, each stereo side
  exactly `0.5`, and every tap inside `0...0.24` across the complete sweep. The
  equation, constants, endpoint table, and measurement procedure are recorded
  in [Normalized Gravity weighting](normalized-gravity-weighting.md).
- Five-state 48 kHz renders order the early/late ratio from `-5.679 dB` at
  Inverse to `+12.649 dB` at Forward. Time to peak falls from `493.312 ms` to
  `62.417 ms`; the raw integrated-energy spread is `1.992 dB`, below the
  declared `2.1 dB` ceiling.
- The reusable runtime now applies ordinary control mappings to Gain parameters
  as well as Delay and Allpass parameters. Existing 1 kHz evaluation and linear
  interpolation keep 64-sample-boundary endpoint automation finite and below a
  `0.10` adjacent-sample discontinuity ceiling for the declared test signal.
- Native regressions cover the two endpoints, center, two intermediate states,
  causal onset and finite renders at 44.1/48/96 kHz, exact reset, JSON
  round-trip, feedback legality, and compilation at 192 kHz.
- No editor component or styling changed in M9.2, so the UI-capture policy does
  not require a new screenshot or video. The expanded graph remains composed of
  the already-released Macro, Curve Mapper, Gain, and typed-cable presentation;
  structural tests prove that all eight branches are present and persisted.

## M9.3 verification

- The complete builder adds Size, Feedback, Damping, and Modulation beside
  Gravity. Thirteen Size cables scale fixed Delay times, Feedback controls the
  delayed return Gain, Damping controls its Low-pass, and Modulation controls
  four input-diffusion coefficients. The exact equations and clamps are in
  [Gravity Diffusion complementary controls](gravity-diffusion-controls.md).
- Motion A (`0.11 Hz` sine, phase zero) moves odd stage Allpasses; Motion B
  (`0.073 Hz` triangle, quarter-cycle phase) moves even stages. All eight
  `+/-1.25 ms` mappings are visible and bounded, with no hidden control sum.
- The expanded graph is 58 nodes / 94 cables with 15 control nodes and 35
  mappings. It validates, compiles as one legal delayed-feedback component,
  and round-trips through schema-v2 JSON exactly.
- Native renders verify later Size onset while retaining the Gravity sign,
  longer full-tail energy at high Feedback, reduced energy/high-frequency
  difference content at high Damping, and different finite sample streams at
  the Modulation endpoints.
- All 32 five-Macro endpoint combinations remain finite for silence, impulse,
  and full-scale bounded noise without latching the numerical guard. A
  4,000-block continuous five-Macro sweep accepts runtime-only changes and
  remains finite; the full suite retains emergency-mute, Undo, and recovery
  regressions.
- UI unchanged; the controls use the released Macro/LFO/typed-cable editor
  presentation and are not catalog-shipped until M9.5, so no capture was
  required for this graph/runtime task.

## M9.4 verification

- A checked-in native generator produces three five-second, 48 kHz stereo
  PCM16 impulse fixtures and adjacent format-v1 measurement JSON from fixed
  visible Macro values. One documented scalar per state matches complete wet
  energy to Bloom at `-20.5829 dB` while preserving mandatory raw metrics.
- Inverse begins causally at frame 5,766, has a `-2.523 dB` early/late ratio,
  and peaks at `419.48 ms`. Bloom peaks at `153.92 ms` with `73.2%` post-peak
  energy. Bloom's first 700 ms has 15 occupied 10 ms windows and only 32.5% of
  its energy in the strongest three. Forward peaks at `8.15 ms` under the centered-smoothing definition;
  its first actual sample remains causal at frame 852.
- Native tests lock all six channel PCM16 hashes, finite output, peak ordering,
  inverse rise, Bloom tail density proxy, matched energy, headroom, and exact
  floating renders after schema-v2 serialization/reload.
- [Gravity Diffusion reference states](gravity-reference-states.md) records the
  generation command, controls, measurements, smoothing edge effect, and a
  critical listening checklist covering ringing, flutter, coloration, stereo
  motion, stepping, and failure modes without presenting preference as fact.
- UI unchanged; this task adds offline evidence rather than the M9.5 shipped
  factory/teaching view, so no screenshot or video was required.

## M9.5 verification

- Gravity Diffusion is the fifth complete catalog entry. Its checked schema-v2
  document is exported from the project-authored native builder and admitted by
  exact hash; catalog metadata uses the original `gravity-diffusion` identity
  and explicitly rejects proprietary-algorithm reconstruction claims.
- Native and browser round-trip tests load the 58-node / 94-cable document using
  only public blocks, validate and compile it, serialize it twice identically,
  and retain every stored layout position without factory-only state.
- The prominent Gravity inspector overlays the violet coordinate prediction
  with a teal checked M9.4 envelope and exact reference controls. Its text says
  the fixture is not the current capture; the response viewer describes a live
  capture as authoritative and leaves disagreement visible.
- Barr/Gravity A/B preserves Gravity as the selected B design, labels it
  **B / Gravity**, and continues to use ordinary graph replacement rather than
  a hidden second engine.
- [Gravity Diffusion factory patch and teaching view](gravity-diffusion-factory-and-teaching.md)
  documents provenance, deterministic regeneration, reconstruction order,
  feedback/control safety rules, save/reload, evidence hierarchy, and the three
  principal states.
- Reviewed evidence under `artifacts/ui/m9-5-gravity-factory/` includes the
  complete fitted graph, Inverse/Bloom/Forward inspector states, and a 12-second
  workflow recording covering A/B selection, sweep, capture presentation,
  mapping focus, schema-v2 save, and reload. The recording's deterministic
  local capture transport was removed before verification; production capture
  behavior remains test-backed and M9.6 owns packaged-host validation.

## M9.6 verification

- The checked factory file renders finite, bounded, non-silent stereo impulses
  and bounded noise at 44.1, 48, and 96 kHz. Its five Macros sweep continuously
  through a 4,000-block combined trajectory without recompilation or a
  full-scale discontinuity; the earlier 32-endpoint safety matrix remains green.
- A fresh JUCE host processor restores all 58 nodes, 94 cables, 58 stored
  positions, and five deliberately non-default macro values before preparation
  and retains them after 96 kHz preparation.
- Tracktion pluginval 1.0.4 passes strictness 10 with editor, state,
  restoration, automation, thread, bus, fuzz, and 44.1/48/96 kHz processing
  coverage. The complete log is checked in under
  `artifacts/validation/m9-6-gravity/`.
- Native physical-window captures at real Windows 100%, 125%, and 150% scale
  show the maximized Release standalone and WebView meeting at every client
  edge. The 150% capture shows the selected complete Gravity factory and usable
  library, canvas, inspector, measurement, A/B, and editing controls. The
  workstation was restored to its original 125% setting afterward.
- Package validation now verifies the adjacent SHA-256, required distribution
  members, Standalone/VST3 declaration, exact `build-info.json` commit, and the
  same embedded commit string in both binaries. See
  [Gravity Diffusion validation and Windows package](gravity-validation-and-package.md).

## Standalone maximized-window correction

- The first work-area correction was rejected by visual review because its
  guessed 32-by-64 frame margin remained plainly visible at the right and
  bottom edges. Standalone startup now explicitly maximizes the outer JUCE
  window, matching the already-correct result of using the maximize control.
- Hosted VST3 editors deliberately retain a predictable 1280-by-800 preferred
  size because the DAW owns their outer window; both formats remain resizable
  from 640-by-400 through 8192-by-8192.
- A native sizing-policy regression preserves the stable editor content size
  used before the standalone outer window exists. The repository checker locks
  the explicit maximize request and exact WebView fill.
- Standalone startup is automatically maximized; the earlier logical-resolution
  screenshot was removed after it proved incapable of showing the complete
  physical display boundary.

## WebView2 physical scaling correction

- Full-resolution review of the packaged build exposed that the earlier evidence
  captured only the 1536-by-960 logical upper-left portion of a 1920-by-1200
  physical display. The actual WebView left a 384-pixel right strip and a
  240-pixel bottom strip at 125% scaling.
- The build now preserves JUCE's monitor-scale conversion when assigning
  WebView2 controller bounds. Evidence is captured at the panel's full physical
  resolution rather than through a DPI-virtualized screenshot API.
- Reviewed evidence:
  [`01-webview-fills-physical-125-percent.png`](../artifacts/ui/window-sizing-work-area/01-webview-fills-physical-125-percent.png)
  shows the packaged-layout candidate filling the complete 1920-by-1200 panel
  above the Windows taskbar at 125% scaling.

## M10.3 verification

- `pitch-shift` is a public mono block with explicit semitone, grain,
  overlap, and direction fields; its three continuous controls retain the
  existing typed modulation mapping. Runtime compilation binds the node to
  the prepared `PitchShift` processor and plans its exact latency/storage in
  the shared arena.
- Browser tests cover creation, typed audio connection, copy/paste, delete,
  Undo/Redo, exact schema-v2 save/load, and the reduced-motion/telemetry copy.
  Native tests compare graph output sample-for-sample with direct DSP use and
  prove complete host-state restoration before and after audio preparation.
- Block and inspector labels distinguish musical ratio pitch shift from
  frequency shift, moving Delay/Doppler behavior, whole-response reversal, and
  pre-input audio. Read-only quality and rate-derived latency remain visible.
- Current reviewed screenshots:
  [block and grain view](../artifacts/ui/m10-3-visible-pitch-shift/01-pitch-shift-block-and-grains.jpg)
  and [lower inspector controls](../artifacts/ui/m10-3-visible-pitch-shift/02-pitch-shift-complete-inspector.jpg).
  The [six-second interaction recording](../artifacts/ui/m10-3-visible-pitch-shift/pitch-shift-continuous-edit.mp4)
  shows a continuous `-12...+12 st` edit, moving grain markers, and the bound
  48 kHz editor status. Native tests remain authoritative for active audio.

## M10.4 verification

- A three-tone chord fixture proves energy moves into each expected +12 st band
  at 44.1, 48, and 96 kHz. Two unequal input tones prove ratio-dependent hertz
  offsets, while an explicitly modulated Delay retains its carrier/sidebands
  and does not create the octave target.
- A visible-primitives feedback graph crosses an 11 ms Delay and uses a 0.35
  shifted return. Forward and reverse grains remain finite under impulse plus
  deterministic bounded noise for two seconds at all three rates.
- The same loop completes a 480-sample/10 ms direction-change topology
  crossfade. Native guard and full processor tests cover numerical latch,
  manual emergency mute, muted output, explicit state-clearing recovery, and
  recovered silence.
- The reproducible Release CLI records both directions for the one shipped
  quality in
  [`pitch-shift-validation-v1.json`](../artifacts/measurements/pitch-shift-validation-v1.json).
  Exact latency/storage and measured CPU are included for every qualified rate.
  The folded-alias fixture discloses near-reference-level aliasing at 48/96 kHz
  and therefore gates later shimmer work on explicit band-limiting.
- UI unchanged; no new screenshot or video was required. M10.3 retains the
  current block, inspector, continuous-edit, and grain-motion evidence.

## M11.1 verification

- Safe Parallel Shimmer is authored by `makeSafeParallelShimmerGraph` as 28
  ordinary public nodes and 32 mono audio cables. There is no hidden shimmer
  processor or factory-only block.
- The only feedback component is the delayed, damped ordinary tank. Structural
  reachability tests prove that neither Pitch Shift nor Shimmer Level can reach
  the tank entry, so repeated +12-semitone accumulation is absent by design.
- Reverb Decay, Shimmer Level, post-shift Shimmer Damping, and final Wet Balance
  are independent named nodes with separate bounded parameters. The octave
  branch also exposes its `x - Low-pass(x)` pre-filter and two Allpasses.
- A 600.01 ms normal-path Delay aligns within two samples of Pitch Shift's fixed
  latency at 44.1, 48, 96, and 192 kHz. Maximum-rate preparation uses twelve
  planned delay-bearing processors and less than 4 MiB of the 64 MiB budget.
- Qualified-rate two-second impulse-plus-noise renders are finite, below full
  scale, non-silent, and stereo-different through unequal 11.9/19.7 ms output
  Allpasses. Exact schema-v2 JSON round-trip is covered.
- [Safe Parallel Shimmer topology design](safe-parallel-shimmer-design.md)
  records signal flow, responsibility boundaries, non-recirculation proof,
  alias-filtering limitation, latency, memory, normalization, and M11.2 scope.
  UI unchanged; no screenshot or video was required for this architecture task.

## M11.2 verification

- Safe Parallel Shimmer is the sixth complete catalog family. Its 28-node,
  32-cable schema-v2 factory is exported directly from the checked M11.1 native
  builder and admitted by exact SHA-256; no hidden processor or private state is
  introduced.
- The factory menu, Barr/design A/B selection, schema save/load, Shimmer Level
  Undo/Redo, and complete plugin-host restoration retain the graph, layout,
  names, mappings, and an edited value before and after 96 kHz preparation.
- A deterministic 330 Hz fixture measures a persistent 660 Hz halo at early and
  late windows. The 1320 and 2640 Hz bands remain at least 50 dB below the halo,
  proving that later frames do not form a cumulative octave staircase. Checked
  values live in `artifacts/measurements/safe-parallel-shimmer-v1.json`.
- Factory impulse and bounded-noise renders are finite and below full scale at
  44.1, 48, and 96 kHz. Unequal output Allpasses retain stereo difference.
- The inspector calls the design **Parallel Shimmer / One Pitch Pass**, names
  both branches, and explicitly contrasts its post-tank octave halo with classic
  feedback shimmer. [Factory and teaching documentation](safe-parallel-shimmer-factory-and-teaching.md)
  records generation, claims, measurements, editing behavior, and evidence.
- Reviewed screenshots and the selection/edit/measurement/save/reload recording
  are stored under `artifacts/ui/m11-2-safe-parallel-shimmer/`. The recording's
  deterministic browser-only capture fixture was removed before verification;
  native render and capture tests remain authoritative.

## M12.1 verification

- `makeSplitFeedbackShimmerGraph` authors a 25-block, 29-cable public graph with
  a shared damped tank and separately named normal and shifted return branches.
- Normal Feedback is independently bounded to 0...0.58 and Shifted Feedback to
  0...0.14. Their simultaneous maximum is the documented 0.72 operating
  ceiling; a deterministic render confirms the normal branch sustains late
  decay while shifted feedback is zero.
- The shifted return visibly implements a one-pole subtractive high-pass before
  the +12-semitone Pitch Shift and a one-pole low-pass afterward. Their locations,
  approximately 6 dB/octave slopes, ranges, and aliasing limitations are
  documented in `split-feedback-shimmer-design.md`.
- Feedback compilation proves the complete graph has no algebraic sub-loop and
  preserves both explicit return Delays. A deliberately illegal edit that
  bypasses the tank and shifted-return Delays is rejected while the last valid
  runtime remains audible.
- Three-second maximum-setting noise renders at 44.1, 48, and 96 kHz remain
  finite, non-silent, below full scale, and within the prepared delay-memory
  budget. Existing safety mute and explicit state reset remain operational.
- The task adds no factory entry or editor behavior. UI unchanged; screenshot
  and video evidence are deferred to the M12.3 publishing task.
