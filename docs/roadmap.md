# Visual Reverb Constructor / Inspector Roadmap

Status: initial execution roadmap
Date: 2026-08-08
Product definition: [visual-reverb-constructor-spec.md](visual-reverb-constructor-spec.md)

## Delivery policy

Every task is completed with the `$complete-project-task` skill. A task is done only when its acceptance criteria, relevant tests, documentation, and UI evidence requirements pass, and the resulting commit is verified on `origin/main`.

UI tasks require a current screenshot. Interactive, animated, audio-reactive, or real-time UI tasks also require a short video. Evidence is stored under `artifacts/ui/<task-slug>/`.

## Milestone summary

| Milestone | Outcome | Exit demonstration |
|---|---|---|
| M0. Foundation | Reproducible open-source project skeleton and engineering contracts | Clean checkout builds and tests on the primary platform |
| M1. Audible Barr reference | Tested fixed MIDIVerb-I/Barr-inspired DSP reference | Stereo audio and impulses render deterministically through the reference graph |
| M2. First vertical slice | Fixed Barr graph is visible, editable, and audible | Change a displayed parameter continuously and hear/see the result |
| M3. General graph runtime | Users can construct legal reverb graphs from primitives | Build, connect, save, reload, and render a custom feedback reverb |
| M4. Inspector | Topology and measured behavior explain one another | Highlight a loop, excite it, and inspect stereo IR/decay/energy |
| M5. Modulation and live editing | Modulated delays/allpasses and safe topology edits work in real time | Patch an LFO, edit a live graph, and remain click-safe/stable |
| M6. Reverse reverb | Reverse/gated envelope structures are constructible and teachable | Build and audition a reverse-style patch from visible primitives |
| M7. Open-source alpha | A documented standalone/plugin release is usable by others | Download, install/build, create a patch, save it, and reopen it in a host |
| M8. Gravity control infrastructure | One visible macro continuously reshapes a network from inverse rise to forward decay | Sweep Gravity while its explicit mappings and predicted envelope remain inspectable |
| M9. Gravity Diffusion instrument | An eight-stage modulated diffusion network produces a bounded, measurable Gravity family | Audition, inspect, modify, save, and compare inverse/bloom/forward Gravity states |
| M10. Visible pitch-shift primitive | A deterministic dual-grain Pitch Shift block provides honest semitone transposition | Patch a visible octave shifter, inspect its grains and latency, and verify the octave in rendered audio |
| M11. Safe Parallel Shimmer | A non-recirculating octave branch adds a stable controllable halo | Build and audition a parallel +12-semitone reverb without cumulative pitch rise |
| M12. Split-Feedback Shimmer | Independent normal and shifted feedback paths create controllable harmonic ascent | Hear successive octave energy build while both feedback paths remain visible and bounded |
| M13. Reverse Cosmic Shimmer | Reverse grains, causal rise, modulation, and stereo diffusion form the flagship evolving shimmer | Audition and inspect a swelling, harmonically rising, darkened stereo space |

---

## M0. Foundation

Goal: establish a buildable repository, a stable graph vocabulary, and the quality gates that every later milestone relies on.

### M0.1 Select the primary implementation stack

Decide the C++ plugin/runtime framework, UI prototype stack, primary operating system, first plugin format, build system, and dependency policy. Record evaluated alternatives and licensing consequences.

Acceptance criteria:

- An architecture decision record selects the runtime/plugin framework and UI prototyping approach.
- The selected licenses are compatible with an open-source project and the intended distribution model.
- Primary development OS and first deliverables are explicit; recommended initial target is standalone plus VST3 on Windows.
- The decision explains whether the production UI may use a webview and identifies the checkpoint for revisiting it.
- No transformed MIDIVerb ROM is committed until its redistribution status is resolved.

### M0.2 Create the project skeleton

Create the production source layout, build configuration, dependency declarations, test targets, documentation index, and artifact directories.

Acceptance criteria:

- A clean checkout configures and builds without undocumented manual file placement.
- Separate targets or modules exist for DSP, graph model/compiler, application/plugin wrapper, UI, and tests.
- `artifacts/ui/` and test-fixture conventions are documented without committing unnecessary generated output.
- The build contains a minimal standalone executable or plugin smoke target.
- Repository instructions identify supported toolchain versions.

### M0.3 Establish continuous integration and quality checks

Add deterministic build, unit-test, lint/format, and artifact checks for the primary platform.

Acceptance criteria:

- CI runs on pushes to `main` and fails on compilation or test failure.
- A local documented command reproduces the required CI checks.
- At least one deliberately failing test has been used locally to prove the test step is enforced, then restored.
- Compiler warnings are elevated according to a documented policy.
- CI does not require copyrighted ROM data or developer-local files.

### M0.4 Define the graph schema v1

Specify node identity, typed ports, parameters, audio/control connections, layout data, units, schema versioning, and migration rules.

Acceptance criteria:

- A JSON schema or equivalently testable definition exists.
- Semantic graph data and editor layout are distinguishable.
- Stereo I/O is represented as explicit mono ports.
- Audio and control connections are type-checked.
- Delay values use milliseconds by default and preserve enough precision for round trips.
- Valid and invalid example patches exist as fixtures.
- Serialization round-trip tests preserve all semantic data and stable IDs.

### M0.5 Define real-time and safety contracts

Document what the audio thread may do, legal feedback-cycle rules, numerical safety behavior, maximum resource policies, and runtime publication semantics.

Acceptance criteria:

- Zero-delay algebraic cycles are explicitly illegal.
- Every legal directed cycle contains an explicit stateful delay element.
- Audio-thread allocation, blocking, filesystem/network access, and unbounded work are prohibited.
- NaN, infinity, runaway level, and emergency mute behaviors are specified.
- Parameter smoothing and topology-swap behavior are specified separately.
- Tests can observe violations of the numerical safety contract.

Milestone exit criteria:

- A clean primary-platform checkout builds and passes all checks.
- Schema and safety contracts are versioned and linked from the documentation index.
- The repository is ready to accept DSP work without revisiting foundational layout.

---

## M1. Audible Barr reference

Goal: establish a deterministic, testable sonic reference before making the graph editable.

### M1.1 Implement core DSP primitives

Implement mono gain/invert, explicit sum, delay, coefficient-0.5/general allpass, and low-pass primitives with preparation/reset/process APIs suitable for offline and real-time use.

Acceptance criteria:

- Each primitive has deterministic unit tests.
- Delay timing is correct at two or more sample rates.
- Allpass magnitude response is flat within a documented tolerance for stable coefficients.
- Sum and polarity behavior match signed reference vectors.
- No primitive allocates during processing.
- Reset produces deterministic silence and clears documented state.

### M1.2 Implement the fixed Barr reference graph

Construct a legal graph reflecting the MIDIVerb-I/BarrVerb channel plan: stereo input to mono, input filtering, diffuser/tank stages, and distinct left/right output branches.

Acceptance criteria:

- The graph is built from the same primitives intended for user patches.
- The signal path visibly/documentarily distinguishes mono input summing from stereo output tapping.
- An impulse produces non-identical left and right wet outputs.
- The graph remains finite for the full reference test duration.
- Node and connection identities are stable and serializable.
- Any departure from the original BarrVerb arithmetic/filtering is documented.

### M1.3 Add offline rendering and golden tests

Create a headless renderer for impulses and short audio fixtures, with machine-readable analysis output.

Acceptance criteria:

- A command renders a patch to stereo WAV without opening a UI.
- Fixed input, graph, sample rate, and engine version produce deterministic output within documented platform tolerances.
- Golden tests cover silence, impulse, bounded noise, reset, and program/state reload.
- Failures report useful sample/channel differences rather than only a boolean mismatch.
- Golden files are small, documented, and legally redistributable.

### M1.4 Add reference measurements

Measure impulse length, onset, stereo difference, peak level, decay curve, and approximate RT60 for the reference patch.

Acceptance criteria:

- Measurement code is automated and tested on synthetic known responses.
- Reference measurements are written to a versioned machine-readable artifact.
- Documentation explains which metrics are exact, estimated, or heuristic.
- The RT60 estimator declines to report a value when decay range or noise floor is insufficient.

### M1.5 Create the audible reference harness

Provide a minimal standalone interface or CLI playback path for live input and impulse audition.

Acceptance criteria:

- A user can select an audio device or use a documented default, hear the reference wet output, and trigger an impulse.
- Master audition gain and emergency mute are available.
- Device/sample-rate changes reprepare the engine safely.
- A smoke test covers application startup without audio hardware where practical.
- UI evidence shows the harness; video demonstrates impulse audition and emergency mute.

Milestone exit criteria:

- The fixed reference renders offline and runs live.
- DSP primitives and safety behavior pass automated tests.
- The reference has documented measurements and known differences from BarrVerb/MIDIVerb hardware.

---

## M2. First vertical slice

Goal: prove the central experience with a fixed Barr graph that is simultaneously visible, editable, audible, and saved.

### M2.1 Prototype the schematic editor shell

Build the three-pane editor: module library, patch canvas, and inspector, plus transport/audition status.

Acceptance criteria:

- The Barr reference graph renders legibly at the default window size.
- Pan, zoom, selection, focus, and keyboard deletion follow documented interactions.
- Audio and control styling is distinguishable without color alone.
- Nodes remain readable at supported display scaling values.
- Screenshot evidence covers default, selected-node, and zoomed states.
- Video evidence covers pan, zoom, selection, and inspector updates.

### M2.2 Bind visible nodes to the fixed runtime

Connect the displayed reference graph to the actual DSP node identities and parameters.

Acceptance criteria:

- Every visible DSP block corresponds to the node that processes audio.
- Selecting a node shows its live parameter values and units.
- No hidden summing, polarity inversion, channel conversion, or delay exists inside the reference patch except documented engine safety output.
- A debug assertion or test detects UI/runtime identity mismatch.

### M2.3 Implement continuous parameter editing

Allow gain, allpass coefficient, delay milliseconds, and low-pass settings to update during pointer drag.

Acceptance criteria:

- Changes are audible before pointer release.
- Gain/filter changes are smoothed without zipper noise under the defined test signal.
- Delay changes use the documented clean crossfade policy and remain finite.
- Undo/redo restores exact parameter values.
- Host/UI thread edits do not block or allocate on the audio thread.
- Video evidence demonstrates continuous editing while audio or repeated impulses play.

### M2.4 Save and reload the reference patch

Persist graph semantics, parameters, positions, viewport, and schema version.

Acceptance criteria:

- Saving then loading restores identical graph semantics and parameters.
- Node positions and viewport restore within UI-coordinate tolerance.
- Loading an invalid fixture shows actionable diagnostics without replacing the current valid graph.
- Unknown future fields are handled according to the schema policy.
- Round-trip and invalid-load tests pass.

### M2.5 Add contextual teaching affordances

Provide concise on-demand explanations for the reference patch, selected module, mono-to-stereo structure, and feedback path.

Acceptance criteria:

- Explanations do not obstruct normal patching and can be dismissed/disabled.
- Text distinguishes documented Barr/MIDIVerb facts from this implementation's choices.
- Selecting the input sum explains why the historical reference becomes mono.
- Selecting left/right output branches explains stereo tapping from one tank.
- Documentation links to the longer architecture research without requiring internet access.

Milestone exit criteria:

- A user can hear the fixed reference, understand its visible structure, continuously edit core parameters, undo, save, and reopen it.
- The exit demonstration is captured in a short video.

---

## M3. General graph runtime and construction

Goal: turn the fixed reference into a small reverb construction environment without becoming a general modular platform.

### M3.1 Implement editable node creation and deletion

Add Input, Output, Delay, Allpass, Gain, `+`, and Low-pass modules from the library.

Acceptance criteria:

- Dragging/clicking from the library creates a node with stable ID and safe defaults.
- Deleting a node removes its connections atomically and is undoable.
- Required I/O invariants are enforced with useful messages.
- Create/delete operations serialize and round-trip.
- UI evidence demonstrates every primitive's default appearance.

### M3.2 Implement typed connection editing

Create, replace, branch, and delete mono audio connections with explicit occupied-input behavior.

Acceptance criteria:

- One output can branch to several inputs.
- A single-input port rejects an unintended second cable and offers to insert `+`.
- Invalid direction/type connections cannot enter the semantic graph.
- Connection creation/deletion is undoable.
- Cable hit targets work at minimum supported zoom and scaling.
- Video evidence covers connection, branching, rejection, and automatic `+` offer.

### M3.3 Compile acyclic graphs

Validate and schedule arbitrary legal acyclic graphs into immutable runtime graphs.

Acceptance criteria:

- Topological scheduling is deterministic for the same semantic graph.
- Disconnected and unreachable nodes receive documented warnings or treatment.
- Runtime memory is prepared off the audio thread.
- Offline tests compare representative constructed graphs with direct reference calculations.
- Invalid compilation leaves the last valid runtime audible.

### M3.4 Compile feedback graphs

Detect strongly connected components and schedule legal cycles containing explicit delay state.

Acceptance criteria:

- Legal delay-containing feedback cycles compile and render deterministically.
- Zero-delay algebraic cycles fail with the exact offending loop highlighted.
- Nested and multiple feedback loops have automated tests.
- Execution order and state-update semantics are documented.
- Compile time and memory remain within defined budgets for the MVP graph limit.

### M3.5 Plan and limit delay memory

Allocate delay state in prepared arenas and enforce a visible project budget.

Acceptance criteria:

- Processing performs no dynamic allocation.
- Patch inspection reports total requested and allocated delay memory.
- Over-budget graphs fail safely before runtime publication.
- Sample-rate changes recalculate memory without corrupting the active runtime.
- Boundary tests cover zero, minimum, maximum, and over-limit delays.

### M3.6 Complete graph undo/redo and clipboard behavior

Support atomic history for node, connection, layout, and parameter operations.

Acceptance criteria:

- Undo/redo returns semantic graph hashes to prior values across mixed operations.
- A compound operation such as inserting a sum node is one undo step.
- Copy/paste generates new IDs while preserving internal connections and parameters.
- History has a documented memory limit and clean-state marker.
- Save does not unexpectedly clear or alter graph semantics.

Milestone exit criteria:

- Starting from an empty canvas, a user can construct, hear, save, reload, and safely edit a custom feedback reverb using the core primitives.

---

## M4. Inspector

Goal: make the relationship between topology and sound immediately understandable.

### M4.1 Implement feedback-loop highlighting

Find and present directed loops containing a selected node or cable.

Acceptance criteria:

- Selecting part of a loop highlights the complete loop without changing the graph.
- Multiple loops can be cycled or distinguished.
- The inspector lists constituent nodes, nominal total delay, polarity/gain elements, and filters.
- Results are correct for nested and shared-edge fixtures.
- Highlighting remains responsive at the supported node/edge limit.

### M4.2 Implement impulse audition and capture

Excite the active graph with a controlled impulse and capture a bounded stereo response for analysis.

Acceptance criteria:

- Audition level is safe and independent of patch input gain.
- Capture length and stop threshold are user-visible and bounded.
- Live input can be muted during measurement.
- Capture never blocks the audio thread.
- Repeated measurements of an unmodulated graph are deterministic.

### M4.3 Implement stereo impulse and decay view

Display left/right waveforms, smoothed energy decay, and defensible decay estimates.

Acceptance criteria:

- Zoom and pan reveal early samples and the full tail.
- Channels are distinguishable without color alone.
- Decay smoothing and RT60 estimation match tested synthetic fixtures.
- Insufficient decay range produces an explanation rather than a misleading number.
- Screenshot evidence includes a short reverb, Barr reference, and long/bloom-like response.
- Video evidence demonstrates measurement and navigation.

### M4.4 Implement live energy glow

Send bounded, decimated telemetry from runtime nodes to the UI and illuminate active nodes/cables.

Acceptance criteria:

- Telemetry uses fixed-size, non-blocking communication.
- Disabling animation removes its measurable UI/runtime overhead within tolerance.
- Glow follows measured signal energy and decays smoothly.
- Dropped telemetry frames do not affect audio.
- Accessibility/reduced-motion mode is supported.
- Video evidence shows an impulse entering, diffusing, and recirculating.

### M4.5 Add resource and safety diagnostics

Show CPU estimate/use, delay memory, clipping/runaway events, and active emergency mute.

Acceptance criteria:

- Diagnostics distinguish estimates from live measurements.
- Runaway protection identifies the last active graph revision.
- The user can undo or reduce gain while muted, then explicitly recover.
- Synthetic NaN/infinity/runaway tests exercise the complete diagnostic path.
- Diagnostic rendering never exposes stale pointers or audio-thread state directly.

Milestone exit criteria:

- A user can select a feedback path, understand its composition, excite it, compare stereo behavior, see its decay, and watch measured energy recirculate.

---

## M5. Modulation and safe live editing

Goal: add movement and expressive editing without compromising real-time safety or schematic clarity.

### M5.1 Implement control-rate graph semantics

Add typed control ports, parameter sockets, update rate, scaling, clamping, and base-plus-modulation behavior.

Acceptance criteria:

- Audio/control type mismatches are rejected.
- Control rate and interpolation into audio processing are documented and tested.
- Base value, modulation amount, polarity, and clamping are visible in the inspector.
- Control graphs cannot create unbounded per-block work.
- Saved patches preserve mappings exactly.

### M5.2 Implement LFO and modulation mapping blocks

Provide sine and triangle LFOs initially, plus explicit scale/offset/bipolar mapping.

Acceptance criteria:

- Frequency, phase, waveform, and restart/free-run semantics are tested.
- One control output can modulate multiple parameter sockets.
- Mapping displays the resulting parameter range before connection.
- Modulation nodes and cables are visually distinguishable without color alone.
- UI evidence demonstrates creation and mapping; video shows live modulation.

### M5.3 Add modulated Delay and Allpass parameters

Allow delay time and allpass delay/coefficient modulation within safe documented ranges.

Acceptance criteria:

- Delay modulation uses a selected interpolation method without invalid memory reads.
- Parameter-rate limits prevent alias-prone or unsafe settings, or label advanced behavior accurately.
- Constant control produces the same output as the equivalent static parameter within tolerance.
- Stress tests cover minimum/maximum time and modulation depth.
- Barr-style moving diffusion can be constructed from visible modules.

### M5.4 Implement runtime topology publication

Compile topology changes off-thread and publish immutable runtimes at block boundaries.

Acceptance criteria:

- The audio thread performs an O(1) or documented bounded swap.
- Old runtime state is reclaimed off-thread.
- Failed compilation leaves the previous runtime active.
- Rapid edit stress tests produce no crash, deadlock, invalid output, or unbounded queue growth.
- Diagnostics show pending, active, and failed graph revisions.

### M5.5 Add short topology-change crossfades

Suppress clicks during completed node/connection changes without preserving abandoned tails indefinitely.

Acceptance criteria:

- Crossfade duration is bounded and documented.
- Output remains finite when changing a legal feedback path under signal.
- CPU/memory cost is bounded to at most two active runtimes during transition.
- Rapid edits coalesce or queue according to a tested policy.
- Video evidence demonstrates audible/visible live patch edits.

### M5.6 Finalize runaway-feedback UX

Make instability safe, understandable, and recoverable.

Acceptance criteria:

- Sustained over-threshold, NaN, and infinity paths trigger emergency mute deterministically.
- The graph and most relevant loop are highlighted when possible.
- Undo remains operational while muted.
- Recovery requires an explicit safe action and does not automatically re-excite the bad graph.
- Automated end-to-end tests cover detection, mute, edit/undo, and recovery.

Milestone exit criteria:

- A user can patch an LFO into delay/allpass parameters and structurally edit a sounding feedback graph without audio-thread violations, crashes, or dangerous output.

---

## M6. Reverse and gated reverb

Goal: expand the instrument by constructing reverse/gated behavior from understandable modules and measurements.

### M6.1 Define reverse-reverb architecture requirements

Separate true time reversal, reverse-envelope approximation, gated reverb, and Bloom-like slow attack in product language and DSP requirements.

Acceptance criteria:

- Documentation gives audible and impulse-response distinctions between the four concepts.
- The chosen first reverse method is real-time feasible and clearly named.
- Required new primitives are identified before implementation.
- No factory patch is labeled “reverse” solely because it has a long diffuse attack.

### M6.2 Implement the minimum envelope/gate primitives

Add only the modules required by the chosen reverse/gated construction, such as envelope shaping, hold/gate, or tapped-delay weighting.

Acceptance criteria:

- Each new primitive has deterministic DSP tests and visible signal semantics.
- The modules remain reverb-specific rather than opening an unrestricted modular environment.
- Timing uses milliseconds and behaves consistently across sample rates.
- New feedback/nonlinearity risks are included in safety tests.

### M6.3 Build reverse and gated factory patches

Create at least one reverse-style and one gated patch from visible public primitives.

Acceptance criteria:

- Both patches load without hidden DSP modules.
- Their impulse envelopes are visibly and measurably distinct.
- Parameters expose musically meaningful time, diffusion, tone, and level controls.
- Patches remain finite across supported sample rates and stress inputs.
- Audio fixtures, screenshots, and demonstration video are captured.

### M6.4 Add visualization teaching overlays

Explain how envelope shape, diffusion, and truncation create the effect.

Acceptance criteria:

- The impulse/decay view marks rise, peak, gate/hold, and cutoff regions where applicable.
- Contextual explanations distinguish the construction from literal offline reversal.
- Users can compare the reverse/gated patch against the Barr reference in an A/B workflow.
- Explanations are available offline and can be disabled.

Milestone exit criteria:

- Users can load, inspect, modify, and reconstruct distinct reverse-style and gated reverbs from visible modules.

---

## M7. Open-source alpha

Goal: make the instrument reproducibly usable outside the development workspace.

### M7.1 Finalize project licensing and provenance

Select the project license and audit code, dependencies, fixtures, presets, ROM-derived material, fonts, and media.

Acceptance criteria:

- The root license and dependency notices are complete.
- Every shipped binary/data asset has documented provenance and redistribution terms.
- The Barr reference uses only material the project is authorized to distribute.
- Contributor expectations and certificate/DCO policy are documented.
- A clean source archive contains no accidental research ROM or unrelated cloned repositories.

### M7.2 Package standalone and first plugin format

Produce reproducible artifacts for the primary OS and validate them in representative hosts.

Acceptance criteria:

- Clean machines can install/run the standalone application using documented steps.
- The plugin scans, opens, processes audio, saves state, and restores state in at least two named hosts.
- Scaling, focus, keyboard input, and window reopening are tested.
- Plugin validation tooling passes or every exception is documented and release-blocking status decided.
- Packaged artifacts report version and commit identity.

### M7.3 Complete user and developer documentation

Document installation, first patch, feedback safety, modules, modulation, inspection, patch schema, build/test workflow, and architecture.

Acceptance criteria:

- A new user can reproduce the Barr reference tutorial from a clean install.
- A new contributor can build and run tests from a clean checkout.
- Every shipped module and visualization is documented with units and constraints.
- Screenshots match the released UI.
- Broken-link and documentation command checks pass.

### M7.4 Add factory patch and compatibility tests

Treat shipped patches as versioned product assets.

Acceptance criteria:

- Every factory patch loads, validates, renders finite audio, and round-trips in CI.
- Barr, short room/baseline, Bloom-like, reverse-style, and gated families are represented only if complete.
- Schema migration tests cover every released schema version.
- Factory patches declare engine/schema version and license/provenance metadata.

### M7.5 Run alpha usability and safety validation

Exercise the complete first-run and patch-building journeys with users other than the implementer.

Acceptance criteria:

- A written protocol covers installation, Barr tutorial, building a loop, causing/fixing an invalid cycle, modulation, save/reload, and inspection.
- Findings are recorded without personal data and prioritized.
- No known crash, data-loss, dangerous-output, or audio-thread violation remains open at release.
- Accessibility checks cover keyboard access, contrast, non-color distinctions, scaling, and reduced motion.

### M7.6 Publish alpha release

Tag, build, publish checksummed artifacts, and provide release notes with known limitations.

Acceptance criteria:

- The release commit is on `main`, tagged, and reproduced by CI.
- Artifacts and checksums are downloadable.
- Release notes enumerate supported OS/formats, major capabilities, known limitations, and feedback channel.
- The repository landing page links directly to installation, tutorial, roadmap, and issue reporting.
- A release demonstration video shows the Barr reference, construction, modulation, and inspection workflow.

Milestone exit criteria:

- An external user can obtain the project, build or install it, construct and inspect a safe reverb, save it, and reopen it in the supported environment.

---

## M8. Gravity control infrastructure

Goal: add a reusable, transparent macro-control path that can reshape several visible parameters continuously without hiding a reverb algorithm or recompiling topology.

Every M8 task follows the delivery policy: relevant tests and documentation, current screenshot evidence for visible UI changes, a short video for continuous or interactive behavior, one scoped commit, and a verified direct push to `main`.

### M8.1 Specify Gravity behavior and measurements

Define Gravity as a normalized `-1` to `+1` envelope-shape control, with negative values emphasizing progressively deeper network energy and positive values emphasizing early energy. Keep decay length, size, damping, and modulation conceptually separate.

Acceptance criteria:

- Documentation distinguishes Gravity-style inverse decay from sample reversal, reverse-grain processing, and offline pre-reverb.
- The sign convention, center behavior, units, center detent, automation range, and reset/default value are fixed.
- Time-to-peak, early/late energy ratio, peak level, integrated energy, and decay metrics have deterministic definitions.
- At least three reference targets are specified: inverse rise, clustered/bloom center, and forward decay.
- The contract states that no negative-Gravity state may emit wet energy before causal input arrives.
- Public Eventide behavior is cited only as inspiration; the project topology is explicitly original.

### M8.2 Implement a nonlinear Curve Mapper control block

Extend visible control routing beyond linear Scale / Offset so one normalized source can create exponential or power-shaped parameter mappings.

Acceptance criteria:

- The block exposes curve family, amount/exponent, scale, offset, polarity, and clamp bounds with documented equations.
- Linear mode exactly matches the existing Scale / Offset result within tolerance.
- Exponential and power modes are monotonic for every supported parameter range and remain finite at endpoints.
- Control-rate evaluation is bounded, allocation-free after preparation, and interpolated without sample discontinuities.
- Schema-v2 persistence round-trips every mapping field; invalid, non-finite, and unsupported curves fail before publication.
- The inspector previews input/output ranges and distinguishes the block and its cables without relying on color.

### M8.3 Add a visible Macro control-source block

Create a user-named, automatable control source whose output can branch through ordinary Curve Mapper blocks to multiple parameter sockets.

Acceptance criteria:

- Macro exposes name, normalized value, default value, and optional center detent while retaining one explicit control output.
- One Macro output can branch to every required Gravity mapping without hidden destinations or factory-only routing.
- Selecting the Macro lists and highlights all reachable mapped parameters and their predicted ranges.
- Continuous edits are smoothed according to a documented fixed policy and never trigger topology compilation.
- Undo/redo, copy/paste, save/load, host state, and schema migration preserve the Macro and all stable IDs.
- Automated tests cover branching limits, invalid routes, rapid automation, deterministic reset, and saved-state restoration.

### M8.4 Add the Gravity macro presentation

Present a factory-designated Macro as a prominent bipolar Gravity control while preserving the underlying blocks and cables as the source of truth.

Acceptance criteria:

- The control reads `INVERSE` at the negative end, `BLOOM` at center, and `FORWARD` at the positive end.
- The exact numeric value remains visible and keyboard-editable; zero has a reliable center detent.
- Moving Gravity highlights affected blocks and shows the currently predicted envelope without claiming measured audio.
- An Expand/Focus action brings every contributing Macro, Curve Mapper, and destination block into view.
- Disabling teaching overlays does not disable control or inspection.
- Screenshot evidence covers inverse, center, and forward states; video shows a continuous sweep and destination highlighting.

Milestone exit criteria:

- A user can construct and inspect a visible Gravity macro, branch it through nonlinear mappings, automate it continuously, save/reload it, and understand every affected base parameter without hidden DSP.

---

## M9. Gravity Diffusion instrument

Goal: build the first musical Gravity implementation as an original eight-stage, progressively diffused, modulated feedback network controlled by the M8 infrastructure.

Every M9 task follows the delivery policy: deterministic DSP/render tests, affected documentation, current screenshot/video evidence, one scoped commit, and a verified direct push to `main`.

### M9.1 Design the eight-stage diffusion topology

Specify stage delays, allpass groups, tap depths, stereo extraction points, damping location, global delayed feedback, and memory/CPU budgets before tuning the macro.

Acceptance criteria:

- The graph uses only public visible primitives plus M8 control blocks and contains no factory-only audio processor.
- At least eight progressively deeper energy taps and 12–16 allpasses create increasing echo density rather than three isolated echoes.
- Every directed feedback cycle crosses an explicit Delay and passes the existing legality validator.
- Left and right outputs use different internal stages or taps while cables remain mono.
- Maximum delay memory, control mappings, runtime work, and transition cost fit documented project budgets at every supported sample rate.
- A checked-in diagram identifies input diffusion, depth taps, weighted sum, damping, feedback, modulation, and stereo extraction.

### M9.2 Implement normalized Gravity weighting

Use visible Curve Mapper branches to move energy between early and deep taps while compensating total gain.

Acceptance criteria:

- The implemented weighting equation and normalization method are documented and reproducible outside the UI.
- Gravity `+1` produces the highest early/late energy ratio; `-1` produces the lowest; measured intermediate states are monotonic within stated tolerance.
- Negative Gravity increases time-to-peak without reversing samples or producing pre-input output.
- Equal-power or measured-energy normalization keeps integrated wet energy within a specified tolerance across the sweep.
- Rapid and block-boundary Gravity automation remains finite and free of discontinuities above the defined threshold.
- Tests cover endpoints, center, representative intermediate values, reset, and supported sample rates.

### M9.3 Add Size, Feedback, Damping, and Modulation macros

Expose the minimum complementary controls needed to turn the Gravity network into a useful instrument without conflating their responsibilities.

Acceptance criteria:

- Size changes internal time scale and density buildup without changing the Gravity sign convention.
- Feedback changes recirculation length independently enough that inverse and forward envelopes remain identifiable.
- Damping prevents repeated shifted/modulated high-frequency energy from accumulating excessively.
- Two independent slow LFO paths move selected allpass delays with visible bounded mappings.
- All macro combinations remain finite under silence, impulse, full-scale bounded noise, and continuous parameter sweeps.
- The numerical safety latch, emergency mute, Undo, and explicit recovery behavior remain operational at extreme settings.

### M9.4 Tune inverse, bloom, and forward reference states

Create measurable reference settings before naming or shipping presets.

Acceptance criteria:

- Inverse reference has a clearly rising smoothed envelope and a substantially later peak than the forward reference.
- Bloom center has a soft attack and dense clustered tail without obvious three-tap stepping.
- Forward reference presents early energy followed by a conventional decaying envelope.
- Loudness-matched stereo audio fixtures and machine-readable measurements are checked in for all three states.
- Listening notes identify ringing, flutter, coloration, stereo motion, and failure modes without treating preference as measurement.
- Tuning is deterministic after reset and serialized patch reload.

### M9.5 Ship the Gravity Diffusion factory patch and teaching view

Add the complete graph to the factory catalog and make its macro relationships and measured response understandable in the editor.

Acceptance criteria:

- The patch loads as an ordinary editable schema-v2 graph and round-trips without hidden state.
- Factory metadata declares project-authored provenance and does not use the Blackhole product name as the patch identity.
- The teaching view contrasts predicted macro shape with the measured impulse envelope and labels any disagreement honestly.
- A/B comparison can switch between Barr Reference and the selected Gravity state without losing the selected design.
- Screenshot evidence shows the entire graph and each principal Gravity state; video shows selection, sweep, measurement, highlighting, save, and reload.
- User and developer documentation explain how to reconstruct and safely modify the topology.

### M9.6 Validate and package the first Gravity implementation

Run the complete safety, compatibility, UI, and distribution workflow against the new factory graph.

Acceptance criteria:

- Factory impulse and bounded-noise renders remain finite at 44.1, 48, and 96 kHz.
- Continuous sweeps across every macro and representative combined extremes pass numerical and topology-transition tests.
- Standalone and VST3 state restore the complete graph and macro values in named validation hosts.
- Windows 100%, 125%, and 150% physical-resolution captures show the WebView filling its client area with usable controls.
- The clean package embeds the exact source commit, contains current standalone/VST3 binaries, and passes checksum verification.
- Local verification and the clean `main` CI run pass before the milestone is declared complete.

Milestone exit criteria:

- An external user can load Gravity Diffusion, sweep continuously between inverse, bloom, and forward behavior, inspect how every visible mapping creates the result, modify and save it, and reopen the same bounded sound in the supported standalone and plugin formats.

---

## M10. Visible pitch-shift primitive

Goal: add the smallest honest, inspectable pitch-shifting block that can support shimmer without hiding time-domain behavior or compromising the audio-thread contract.

Every M10 task follows the delivery policy: deterministic DSP tests, documentation, current screenshot/video evidence for visible or moving behavior, one scoped commit, and a verified direct push to `main`.

### M10.1 Specify pitch-shift semantics and budgets

Define a mono dual-read-head granular shifter with semitone ratio, grain/window length, overlap, forward/reverse direction, latency, reset, modulation, and channel composition rules.

Acceptance criteria:

- Semitones map to resampling ratio as `2^(semitones / 12)` over a documented bounded range.
- Grain length and overlap use explicit millisecond/normalized units with safe defaults and limits.
- Forward and reverse describe individual grain playback direction and never imply whole-response reversal or pre-input output.
- Reported latency, maximum prepared storage, worst-case work per sample/block, and supported sample rates are specified.
- Parameter automation, reset, silence, discontinuity, aliasing, and feedback-use expectations are testable and documented.

### M10.2 Implement the prepared dual-grain DSP

Implement overlapping windowed read heads using storage allocated during preparation, with deterministic reset and no audio-thread allocation, locking, logging, or resizing.

Acceptance criteria:

- A sine input shifted by +12/-12 semitones measures within the defined frequency tolerance at 44.1, 48, and 96 kHz.
- Overlapping windows maintain bounded output and suppress grain-boundary discontinuities below the specified threshold.
- Silence remains silence; finite bounded input produces finite bounded output for every supported parameter extreme.
- Reset and identical serialized configuration produce sample-deterministic offline renders on the primary toolchain.
- Continuous semitone/grain edits use a documented smoothing or crossfade policy and never read outside prepared storage.

### M10.3 Add the visible Pitch Shift block

Expose one mono audio input/output and visible controls for semitones, grain length, overlap, and grain direction, plus read-only latency and quality information.

Acceptance criteria:

- The module can be created, connected, copied, deleted, undone/redone, saved, loaded, and restored by the host.
- Inspector and block labels distinguish Pitch Shift from Frequency Shift, delay modulation, and sample-order reversal.
- The graph schema migrates older documents deterministically and round-trips every new field without hidden factory-only state.
- The moving grain/read-head visualization has a reduced-motion alternative and does not imply sample-accurate telemetry unless measured.
- Screenshot evidence shows the complete block/inspector; video shows continuous edits and grain motion while audio remains active.

### M10.4 Validate octave identity and feedback safety

Establish reusable measurements before any shimmer factory patch is named complete.

Acceptance criteria:

- Spectral fixtures demonstrate energy moving from representative tones/chords to the expected octave bands rather than only chorusing.
- Tests distinguish musical pitch shift from fixed-hertz frequency shift and Doppler delay modulation.
- A conservative delayed feedback harness remains finite for forward and reverse grains under impulse and bounded noise.
- Numerical-safety latch, emergency mute, topology crossfade, and explicit recovery work with Pitch Shift in a loop.
- CPU, storage, latency, and aliasing measurements are recorded for all supported sample rates and quality settings.

Milestone exit criteria:

- A user can place one visible Pitch Shift block, hear and measure a true octave shift, understand its grains and latency, automate it safely, and save/reopen the same graph.

---

## M11. Safe Parallel Shimmer

Goal: prove a musical shimmer voice outside feedback before allowing cumulative octave recirculation.

Every M11 task follows the delivery policy, including deterministic audio fixtures and current screenshot/video evidence.

### M11.1 Design the parallel topology

Split the diffused input into an ordinary reverb branch and a +12-semitone branch, diffuse the shifted branch, damp it, and recombine both explicitly.

Acceptance criteria:

- The complete construction uses ordinary visible blocks and mono cables with no hidden shimmer processor.
- Shimmer level, damping, and wet balance have separate responsibilities from reverb decay.
- The shifted branch cannot recirculate through Pitch Shift, so successive octave accumulation is structurally absent.
- Stereo extraction uses documented unequal taps/diffusion while preserving deterministic mono-cable semantics.
- Delay memory, latency alignment, and mix normalization fit documented resource and loudness budgets.

### M11.2 Ship and teach Safe Parallel Shimmer

Add the graph to the factory catalog with a teaching view that identifies normal and octave paths.

Acceptance criteria:

- Factory selection, A/B comparison, save/load, Undo, and host-state restoration preserve the complete schema graph.
- Spectral measurements show a +12-semitone halo while later frames do not form an octave staircase.
- Impulse and bounded-noise renders remain finite at 44.1, 48, and 96 kHz.
- Teaching copy calls the design parallel shimmer and explicitly contrasts it with classic feedback shimmer.
- Screenshot evidence shows both branches; video demonstrates selection, shimmer-level editing, measurement, save, and reload.

Milestone exit criteria:

- A user can audition, inspect, modify, and save a stable octave-halo reverb whose decay and shimmer level remain independently understandable.

---

## M12. Split-Feedback Shimmer

Goal: create the canonical evolving shimmer with independent ordinary and pitch-shifted feedback gains.

Every M12 task follows the delivery policy, with feedback legality, real-time safety, spectral evolution, and failure recovery treated as release gates.

### M12.1 Implement independently bounded feedback paths

Branch the tank return into a damped normal loop and a high-passed, +12-semitone, low-passed shifted loop before their explicit recombination.

Acceptance criteria:

- Every directed cycle crosses an explicit Delay and passes the existing feedback validator.
- Normal feedback can lengthen decay with shifted feedback at zero; shifted feedback controls harmonic ascent without owning the whole decay.
- Pre-shift high-pass and post-shift low-pass locations and slopes are visible and documented.
- Conservative parameter limits and normalization keep combined loop gain within the defined operating region.
- Invalid edits leave the last valid runtime audible and extreme settings remain recoverable through the existing safety UX.

### M12.2 Prove cumulative harmonic ascent

Add measurements that distinguish feedback shimmer from a parallel octave layer.

Acceptance criteria:

- Time-resolved spectral fixtures show octave energy increasing on later circulations.
- Tests identify expected +12 and +24-semitone regions within documented tolerance without requiring inaudible higher stages.
- Increasing shifted feedback measurably increases later octave energy while normal feedback can change decay independently.
- Forward-grain artifacts, aliasing, damping loss, and stereo correlation are measured and disclosed.
- Automation of both loop gains, pitch, damping, and size remains finite and click-safe at supported sample rates.

### M12.3 Ship the Split-Feedback Shimmer factory patch

Publish a musical reference graph and make both feedback stories legible in the editor.

Acceptance criteria:

- Loop highlighting can isolate the normal loop, shifted loop, and their shared delayed region.
- The teaching overlay relates visible circulation count to measured harmonic buildup without claiming proprietary reconstruction.
- Factory metadata, compatibility checks, schema round-trip, deterministic audio fixtures, and host restore all pass.
- Screenshot evidence shows both complete loops; video shows highlighting, continuous feedback edits, impulse/spectral inspection, save, and reload.
- Full local verification and clean `main` CI pass before completion.

Milestone exit criteria:

- A user can separately control decay and harmonic ascent, see exactly which loop produces each behavior, and hear a bounded classic feedback shimmer survive save/reload and host restoration.

---

## M13. Reverse Cosmic Shimmer

Goal: combine reverse-grain octave shifting with the existing causal-rise vocabulary, slow allpass motion, dark feedback, and decorrelated stereo extraction.

Every M13 task follows the delivery policy and retains the naming boundary between reverse grains, causal reverse-style envelopes, and true sample-order reverse reverb.

### M13.1 Add reverse-grain mode and stereo decorrelation

Extend Pitch Shift so individual grains can run backward and two mono instances can use decorrelated phase/read-head state without nondeterminism.

Acceptance criteria:

- Reverse-grain playback remains causal and cannot emit wet output before input and declared latency.
- Forward/reverse modes preserve the requested pitch ratio within their separately documented tolerances.
- Seed/phase behavior is deterministic after reset while paired instances avoid identical grain boundaries.
- Reverse grains reduce or reshape forward splice attacks according to checked-in transient/envelope measurements.
- CPU, storage, latency, and feedback-safety budgets remain satisfied at every supported sample rate.

### M13.2 Construct the cosmic shimmer topology

Feed the existing causal-rise front end into split normal/shifted feedback, reverse-grain octave voices, slow independent allpass modulation, damping, and unequal stereo output diffusion.

Acceptance criteria:

- The graph is composed entirely of visible public primitives and contains no Blackhole, Valhalla, or other proprietary product identity.
- The response has a measured late rise followed by sustained harmonic evolution rather than an immediate pitched echo.
- Each circulation is darkened sufficiently to prevent uncontrolled high-frequency/alias accumulation.
- Left/right outputs remain related but non-identical, with stereo correlation and mono compatibility measured.
- Macro or exposed controls keep rise shape, size, normal feedback, shimmer feedback, damping, and modulation conceptually separable.

### M13.3 Tune and publish Reverse Cosmic Shimmer

Create conservative reference settings, factory metadata, teaching overlays, audio fixtures, and distribution evidence.

Acceptance criteria:

- Checked-in impulse, chord, and bounded-noise fixtures demonstrate causal swell, octave evolution, stereo motion, and finite decay at 44.1, 48, and 96 kHz.
- Teaching text distinguishes reverse grains from true reversed audio and describes the design as project-authored behavioral synthesis.
- A/B comparison includes Barr Reference, Modulated Cosmic Reverse, Split-Feedback Shimmer, and the new design without losing selection.
- Screenshot evidence shows the fitted complete graph and principal inspectors; video demonstrates selection, modulation, measurement, continuous editing, safety recovery, save, and reload.
- Standalone/VST3 restoration, compatibility tests, package identity, local verification, and clean `main` CI pass.

Milestone exit criteria:

- An external user can load a swelling, modulated, harmonically ascending stereo reverb; trace every causal, reverse-grain, feedback, damping, and modulation path; modify it safely; and reopen the same sound in supported formats.

---

## Recommended first execution sequence

1. M0.1 - select the stack and primary targets.
2. M0.4 - freeze enough schema to describe the Barr reference.
3. M0.5 - define real-time and safety contracts.
4. M0.2/M0.3 - build skeleton and CI around those decisions.
5. M1.1-M1.3 - create tested primitives, reference graph, and offline renderer.
6. M2.1-M2.3 - build the first audible visual slice before general graph editing.

This order deliberately reaches the central product experience early while keeping the DSP reference deterministic and testable.

## Roadmap change policy

- Milestone outcomes are stable product commitments; task implementation details may change as evidence accumulates.
- A task may be split when its acceptance criteria cannot be demonstrated coherently in one commit.
- New scope enters the backlog or a later milestone unless it is required to satisfy current acceptance criteria safely.
- Completed acceptance criteria are changed only with an explicit rationale and migration note.
