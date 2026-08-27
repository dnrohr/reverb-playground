# Visual Reverb Constructor / Inspector Roadmap

Status: initial execution roadmap
Date: 2026-08-25
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
| M14. Audio-file audition and export | The standalone app can play source material through any graph and render the result without requiring a DAW | Drop in a file, loop a passage, compare reverbs continuously, and export the processed result |
| M15. Fast and resilient standalone startup | The standalone acknowledges launch immediately while Windows audio connects | See a responsive shell within one second and reach the unchanged editor without blocking the UI |
| M16. Latency truth and performance baselines | Pitch range, graph latency, compile time, and CPU cost are accurate and actionable | Compare factory graphs with honest latency/load diagnostics and compensated DAW playback |
| M17. Hybrid compiled graph execution | Only causal recursive regions execute sample-by-sample; the remaining graph uses optimized block kernels | Run the flagship feedback graphs measurably faster without changing their audio or safety behavior |
| M18. Pitch/control optimization and specialization | Pitch Shift, modulation delivery, and selected hot graph paths meet measured release budgets | Audition octave shimmer at lower latency/load and publish evidence for generic versus specialized execution |
| M18.5. Audition and telemetry truth | Audition, mix, gain, loop/export, and Energy controls match their labels and the active compiled graph | Hear an unmistakable dry/wet comparison, export the intended range, and see energy move through the graph that is actually sounding |
| M19. Perceptual density measurement | Sparse echoes, recurrence, coloration, and stereo buildup become measurable | Compare current factories against controlled sparse and dense fixtures in a density inspector |
| M20. Dense figure-eight tank | A two-branch cross-coupled tank provides a lower-risk dense late reverb | Audition a stable, tunable factory with materially smoother density than the current networks |
| M21. Constrained four-line FDN | A visible, inspectable Hadamard FDN produces a modern dense tail | Follow four delay lines through energy-preserving mixing while hearing predictable decay |
| M22. Assisted delay-set tuning | Deterministic offline search proposes reproducible, measurable tunings | Generate, rank, audition, accept, or reject candidate delay sets without losing the current patch |
| M23. Dense-network optimization | Reusable SIMD/fused kernels accelerate measured FDN and figure-eight hot paths | Run dense tanks faster without hidden factory code or changed audio semantics |
| M24. Dense-reverb product qualification | Dense factories are listening-tested, host-safe, documented, packaged, and releasable | Compare matched factories, automate/reopen/export them, and pass clean Windows release CI |

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

## M14. Audio-file audition and export

Goal: let a user evaluate and teach reverb designs with familiar source material in the standalone application, while retaining the VST3 as the production integration path and reusing the exact graph runtime for offline export.

M14 is intentionally a compact audition deck, not a DAW. Recording, playlists, multitrack routing, time stretching, tempo synchronization, and destructive waveform editing are outside this milestone.

### M14.1 Specify source, transport, and real-time boundaries

Define how live input, a loaded audio file, and the existing test impulse feed the same stereo graph input without changing graph semantics or weakening the audio-thread contract.

Acceptance criteria:

- The design defines mutually exclusive Live Input, Audio File, and Test Impulse source modes and shows their relationship to graph input, audition gain, safety mute, inspection, and device output.
- Mono files feed both graph input channels, stereo files preserve left/right identity, and files with more than two channels are rejected with an actionable explanation rather than silently downmixed.
- Decode, disk access, waveform analysis, path handling, and buffer preparation are assigned outside the audio callback; the callback performs only bounded reads from prepared storage.
- Sample-rate conversion, end-of-file behavior, looping, seeking, underflow, device-rate changes, and topology changes during playback have explicit deterministic policies.
- File and transport state remain outside the patch schema; no audio content is embedded in patch or host state, and missing/moved files cannot invalidate a graph.

### M14.2 Implement the prepared audio-file source

Add a bounded stereo transport that streams supported files into the existing live graph runtime.

Acceptance criteria:

- WAV, AIFF, and FLAC files at representative integer and floating-point formats load through JUCE format readers, with clear unsupported/corrupt-file diagnostics.
- Play, pause, stop, seek, and sample-bounded looping are deterministic across host block partitions and supported device sample rates.
- Read-ahead and resampling are prepared off the audio thread; processing performs no allocation, blocking, locking, logging, decoding, filesystem access, or unbounded work.
- Buffer underrun emits silence for the unavailable frames, increments a visible diagnostic, and cannot repeat stale samples or destabilize feedback processing.
- Automated fixtures cover mono/stereo policy, resampling, start/end/loop boundaries, rapid transport edits, device-rate changes, reset, underrun, and numerical safety.

### M14.3 Add the standalone audition deck

Expose source selection and a deliberately small file transport without crowding the schematic editor.

Acceptance criteria:

- The standalone accepts file-picker and drag-and-drop loading and exposes file identity, duration, play/pause, stop, seek, loop enable/range, and an unclipped stereo waveform overview.
- Live Input, Audio File, and Test Impulse are visibly distinct; switching sources is click-safe and never mixes an unexpected live input into file audition.
- Dry bypass and processed audition are explicit, level changes remain bounded by Master Audition Gain and Emergency Mute, and file playback participates in existing energy, response, and safety diagnostics.
- Factory/topology changes, continuous parameter edits, A/B comparison, and save/reload remain responsive while the file transport continues or stops according to the documented policy.
- Keyboard access, contrast, non-color cues, reduced motion, and 100/125/150% Windows scaling pass; screenshots show loaded/looping states and a video demonstrates load, seek, loop, patch comparison, editing, and emergency mute.

### M14.4 Add deterministic processed-file export

Render a loaded source through the current graph to a new audio file without routing through a DAW or the physical audio device.

Acceptance criteria:

- Export offers explicit Wet Only and Audition Mix modes, preserves stereo output, and writes a documented WAV format without overwriting an existing file unless the user confirms.
- Rendering uses the authoritative offline graph runtime and matches equivalent real-time block processing within a declared numerical tolerance.
- Tail policy is explicit and bounded by user maximum length plus a silence threshold; delayed onset is not mistaken for completion, and runaway/non-finite output fails safely without publishing a partial destination file.
- Work runs off the audio callback, reports progress, supports cancellation, and uses a temporary destination plus atomic finalization where the platform permits.
- Deterministic fixtures cover source sample-rate conversion, loop-disabled full-file rendering, tail capture, cancellation, invalid destinations, safety failure, and repeated export hashes or sample comparisons.

### M14.5 Validate and package the audition workflow

Qualify the complete standalone workflow while proving that plugin input and project compatibility remain unchanged.

Acceptance criteria:

- Representative speech/percussion, sustained chord, and full-mix fixtures run through Barr, reverse, Gravity, parallel shimmer, split-feedback shimmer, and Reverse Cosmic Shimmer without crashes, data loss, dangerous output, or unbounded resource use.
- Standalone restart, audio-device changes, missing/moved source files, graph save/reload, and processed export have documented and tested recovery behavior.
- VST3 continues to consume host-provided stereo input without exposing standalone-only file transport or changing released patch/host-state compatibility.
- User documentation explains when to use standalone file audition, offline export, live input, or the VST3 in a DAW and states supported formats and limitations.
- Current screenshots/video, Windows package identity, local verification, and clean `main` CI pass before completion.

Milestone exit criteria:

- A user can open the standalone, load familiar mono or stereo source material, audition and compare any visible reverb graph safely, loop a useful passage, inspect behavior while editing, and export a deterministic processed stereo file without needing a DAW.

---

## M15. Fast and resilient standalone startup

Goal: make the standalone acknowledge a launch immediately even when Windows audio discovery is slow, while preserving the released editor, device controls, saved state, and VST3 behavior.

M15 changes only the standalone host boundary. It does not move audio work onto the UI thread, change the graph runtime, or alter the plugin wrapper.

### M15.1 Establish a measured startup contract

Measure process launch, first visible shell, audio connection, and editor readiness separately instead of treating startup as one opaque duration.

Acceptance criteria:

- The baseline records at least three launches and identifies whether time is spent in processor construction, WebView/editor creation, or audio-device initialization.
- The product contract requires a visible, correctly scaled native shell within one second on the reference Windows machine.
- The shell explicitly says that audio is connecting; it never implies that the editor or audio device is ready early.
- Editor readiness remains independently measurable and a slow or missing endpoint cannot leave the user staring at no window.

### M15.2 Decouple the visible shell from audio discovery

Replace JUCE's stock standalone application ordering with a project-owned wrapper that shows the shell first and prepares the existing plugin holder away from the message thread.

Acceptance criteria:

- Windows device discovery, saved-device recovery, processor state restoration, and audio start complete before the unchanged editor receives the holder.
- Window creation, editor creation, accessibility state, and the final handoff occur on the message thread.
- Closing during startup joins the worker and cannot access a destroyed application, holder, settings object, or window.
- Startup phases advance monotonically, terminate in Ready or Failed, and have automated transition tests.
- The VST3 continues to use its host-provided audio path and receives no standalone startup UI.

### M15.3 Qualify and document startup recovery

Verify the startup path with saved settings, fresh settings, a slow device scan, normal shutdown, and the existing Release suite.

Acceptance criteria:

- Five warm Release launches meet the one-second shell target and report both shell and editor timings.
- Current screenshots show the connecting shell and the ready editor at Windows display scaling; neither is blank, clipped, or DPI virtualized.
- The complete Release verification gate and standalone/VST3 builds pass without changing patch or host-state compatibility.
- Developer documentation records ownership, shutdown behavior, measurement method, current results, and the remaining limitation that audio readiness still depends on the operating system and driver.

Milestone exit criteria:

- Launching the standalone produces honest visual feedback in under one second on the reference system, the UI remains responsive while Windows audio connects, and the existing schematic replaces the shell automatically when the device path is ready.

---

## M16. Latency truth and performance baselines

Goal: establish accurate, user-visible latency and performance contracts before changing the executor, while narrowing Pitch Shift to the musically useful one-octave range.

M16 distinguishes algorithmic latency, graph-publication delay, audio-callback load, and editor presentation cost. A perfect fifth remains an ordinary `+7`-semitone setting; it does not require a separate processor or quality mode because limiting the interval alone does not remove the core dual-grain work.

### M16.1 Constrain Pitch Shift to one octave and migrate safely — Complete

Change the public Pitch Shift range from `-24...+24` to `-12...+12` semitones and derive storage/latency budgets from that supported range.

Acceptance criteria:

- Native DSP, graph validation, inspector metadata, schema fixtures, factory patches, teaching copy, and documentation expose the same `-12...+12` range.
- `+7` semitones remains available as an ordinary perfect-fifth setting with the same quality and cost model as other values in range.
- Existing saved patches or host states containing values outside the new range follow an explicit compatibility policy: deterministic clamp with a visible migration warning, or a schema migration that preserves the last valid graph; they never fail silently.
- Latency and prepared-storage calculations use the new maximum ratio and checked tests prove the exact sample counts at 44.1, 48, 96, and 192 kHz.
- All released shimmer/reverse factories remain within range, render finite, and preserve their expected octave measurements and save/restore behavior.

### M16.2 Compile graph latency and report it to hosts — Complete

Add a deterministic latency pass to the prepared graph compiler and connect the active result to JUCE host latency reporting.

Acceptance criteria:

- Every latency-bearing node reports an exact prepared sample delay; serial paths add, parallel joins expose their maximum and any uncompensated difference, and feedback cycles do not inflate latency recursively.
- The standalone and VST3 show total active graph latency in samples and milliseconds, with per-path inspection sufficient to locate uncompensated branches.
- The VST3 calls the host latency API only from a permitted non-audio thread and updates it when a newly prepared topology with different latency becomes active.
- Dry/audition and parallel wet paths follow an explicit visible-compensation policy; no hidden delay makes the schematic disagree with audible routing.
- Host tests verify latency reporting, state restoration, graph changes, bypass/dry alignment, and unchanged behavior in hosts that defer or ignore dynamic latency updates.

### M16.3 Add graph-specific compile and workload diagnostics — Complete

Replace the fixed Barr workload estimate in constructed-graph mode with information produced by the actual prepared plan.

Acceptance criteria:

- Diagnostics separately label validation/scheduling/preparation time, request-to-active time, aggregate measured callback load, prepared memory, node count, connection count, feedback regions, and topology-crossfade state.
- Static workload estimates are derived from the active operation plan and processor modes rather than the fixed ten-node Barr constant.
- Expensive block families and execution domains are identifiable through an offline/non-real-time profiler; production audio processing does not add per-node clocks, logging, allocation, or locks.
- Compilation measurements preserve newest-request-wins behavior and identify superseded work without making stale results audible.
- The UI explains the difference between estimated operations, measured callback load, algorithmic latency, and compile/publication delay.

### M16.4 Establish the representative performance matrix — Complete

Measure Barr Reference, Gravity Diffusion, Safe Parallel Shimmer, Split-Feedback Shimmer, and Reverse Cosmic Shimmer before executor optimization.

Acceptance criteria:

- Release builds are measured at 44.1, 48, and 96 kHz with representative 32/64/128/256/512-sample host blocks on at least the reference development machine.
- Reports record median and high-percentile callback load, peak load, compile/request-to-active time, graph latency, memory, underruns, and topology-transition overhead.
- Normal processing and the 10 ms two-runtime crossfade are reported separately.
- Repeatable headless benchmarks use deterministic source material and publish machine/toolchain metadata without claiming cross-machine comparability where it is not valid.
- Measured budgets and regression thresholds are documented for M17/M18; a graph that misses a release budget is identified rather than hidden behind an aggregate pass.

Milestone exit criteria:

- Users and developers can distinguish audible latency from CPU load and edit-publication delay, DAWs receive the active graph latency, and every flagship graph has a repeatable pre-optimization baseline.

---

## M17. Hybrid compiled graph execution

Goal: preserve the visual graph's exact semantics while compiling it into mixed execution domains that avoid running an entire feedback graph one sample at a time.

M17 extends the existing prepared execution plan; it does not introduce runtime JSON interpretation, executable-memory JIT code, or audio-thread compilation.

### M17.1 Partition sample-wise and block-wise execution domains — Complete

Use strongly connected components and causal control dependencies to isolate the regions that genuinely require per-sample evaluation.

Acceptance criteria:

- Delay-containing feedback SCCs use the required Delay-read/evaluate/Delay-write schedule while acyclic upstream and downstream regions process whole blocks.
- Envelope Follower and Hold Gate force sample-wise ordering only across the dependent causal region, not unrelated branches or the entire graph.
- Nested, shared, and multiple feedback loops compile deterministically; zero-delay algebraic loops retain their exact rejection diagnostics.
- The schedule is fixed and fully prepared before publication, with no discovery, allocation, locking, or unbounded traversal in the audio callback.
- Golden renders, host partition tests, modulation tests, safety recovery, and state restoration prove equivalence with the previous executor within documented floating-point tolerance.

### M17.2 Add buffer liveness, reuse, and safe aliasing — Complete

Compile buffer ownership from connection fan-out and last-use information instead of reserving and copying one full block buffer for every output.

Acceptance criteria:

- Single-consumer in-place-safe paths alias or reuse storage; branched, feedback, capture, and inspector-visible values retain independent storage where required.
- A deterministic liveness plan reports logical signals, physical buffers, peak live buffers, bytes saved, and reasons that prevent aliasing.
- Silence/input/output buffers, transition scratch, delay arenas, and telemetry snapshots cannot be accidentally overwritten.
- Maximum-node and maximum-block fixtures demonstrate lower or equal prepared buffer memory and fewer copies without increasing delay storage.
- Sanitizer/canary, repeated reset, rapid publication, and two-runtime crossfade tests detect lifetime or alias corruption.

### M17.3 Fuse and specialize prepared block kernels — Complete

Combine eligible linear operation chains and select typed processing kernels during compilation.

Acceptance criteria:

- The compiler can fuse documented safe patterns such as Gain/Sum/Low-pass routing without hiding a user-visible block or changing its inspectable identity.
- Operation dispatch is selected before publication through typed records or prepared function targets; no string lookup or processor-type discovery occurs per sample.
- Eligible gain, sum, copy, and mix kernels use proven block/SIMD paths where the platform supports them, with scalar reference fallbacks.
- Modulated, feedback, nonlinear, tapped, branched, and telemetry-observed boundaries prevent unsafe fusion explicitly.
- Exact/scalar-reference comparisons, denormal handling, clipping/safety behavior, and deterministic reload remain within documented tolerance.

### M17.4 Qualify the hybrid executor — Complete

Compare the new plan with the M16 baseline and gate it on both correctness and material benefit.

Acceptance criteria:

- Every released factory and representative user graph passes deterministic render, automation, feedback, safety, capture, export, and host-state suites.
- Reverse Cosmic Shimmer and Split-Feedback Shimmer show a documented reduction in callback load or operation/copy count at 48 and 96 kHz; regressions require an explicit rationale.
- Topology compile/request-to-active time, prepared memory, and 10 ms crossfade peaks remain within the M16 budgets.
- Diagnostics expose block-wise/sample-wise region counts, reused buffers, and fused kernels without overstating measured CPU improvement.
- Current visual evidence explains the optimized regions while preserving the visible schematic as the authoritative program.

Milestone exit criteria:

- Large feedback reverbs retain identical visible semantics and safe publication while unrelated portions no longer pay the global sample-wise execution cost.

---

## M18. Pitch/control optimization and specialization

Goal: reduce the remaining hot-path cost and octave-shifter latency after the hybrid executor is measured, then decide whether any factory topology warrants a specialized native kernel.

### M18.1 Optimize the dual-grain Pitch Shift inner loop

Status: Complete (2026-08-26). See the
[qualification report](pitch-shift-inner-loop-optimization.md) and checked
[measurement artifact](../artifacts/measurements/pitch-shift-validation-v1.json).

Replace avoidable per-sample transcendental, wrapping, modulo, and parameter work with prepared state while retaining the public dual-grain behavior.

Acceptance criteria:

- Phase accumulators, window/coefficient preparation, and circular addressing remove avoidable per-sample `pow`, `sin`, `floor`, or general modulo operations where measurements prove equivalent bounded alternatives.
- Static parameters use a fast steady-state path; semitone ramps and grain/direction transitions retain their 20 ms click-safe behavior.
- Forward/reverse pitch accuracy, latency, transient shape, alias measurements, deterministic reset, stereo decorrelation, and feedback recovery remain within published tolerances.
- Benchmarks report steady state, parameter transition, two Pitch Shift voices, and whole-topology crossfade separately at supported rates.
- No optimization introduces allocation, locks, I/O, mutable lookup tables, or architecture-specific behavior without a scalar fallback.

### M18.2 Compile control-rate ramps into block processors

Status: Complete (2026-08-26). See the
[implementation and qualification report](compiled-control-ramp-processing.md).

Deliver 1 kHz control results to audio processors without repeatedly invoking one-sample setters and one-sample span processing across the host block.

Acceptance criteria:

- The prepared plan emits bounded ramp segments or block parameter views for Gain, Delay, Allpass, Low-pass, and Pitch Shift.
- Processors consume constant, ramped, and transition states through explicit kernels while preserving sample-accurate causal results at control-tick boundaries.
- Static/unmodulated graphs pay no modulation-buffer or per-sample parameter-dispatch cost.
- Macro smoothing, LFO restart/free-run, Curve Mapper behavior, automation, block-partition determinism, and save/restore remain unchanged.
- M16 diagnostics show the reduction in setter calls, one-sample dispatches, and measured callback load for modulation-heavy Gravity and shimmer graphs.

### M18.3 Add bounded quality modes

Status: Complete.

Define quality choices only where they produce a measured latency/load benefit without making saved graphs ambiguous.

Acceptance criteria:

- Draft, Normal, or High policies specify Pitch Shift window/interpolation behavior, visualization/telemetry rate, and any permitted modulation-resolution differences independently.
- The default preserves released sound closely; lower-cost modes disclose changed latency, bandwidth, aliasing, or motion resolution before selection.
- Quality is stored/restored through an explicit product or graph policy and cannot change silently between standalone, VST3, offline export, and reopened projects.
- A perfect fifth remains a normal semitone setting rather than a hidden cheap algorithm; any interval-dependent fast path must be measurement-equivalent and documented.
- Automated measurements and listening fixtures compare cost and artifacts at each supported quality level.

### M18.4 Decide factory specialization versus generic execution

Status: Complete.

Use the completed measurements to determine whether selected factory graphs need ahead-of-time C++ kernels and whether native-code JIT compilation is justified.

Acceptance criteria:

- Barr's existing direct/reference path, the optimized generic executor, and any prototype specialized factory path are compared with the same audio and CPU fixtures.
- A specialized factory remains semantically generated from or checked against its visible graph; hidden divergence fails identity/golden tests.
- Runtime JIT is adopted only if measured benefit remains material after M17/M18.1-3 and its executable-memory, code-signing, crash, security, debugging, and host-compatibility costs are explicitly accepted.
- If JIT is rejected, the decision records why prepared typed/fused kernels are sufficient and identifies no unsupported performance claim.
- Final Release benchmarks, package/host validation, documentation, and clean `main` CI establish the supported graph-size/sample-rate/quality envelope.

Milestone exit criteria:

- Octave shimmer runs with honest reduced latency and measured lower cost, control-heavy graphs avoid unnecessary audio-rate dispatch, and the project has evidence for using generic, specialized, or JIT execution rather than relying on intuition.

---

## M18.5. Audition and telemetry truth

Resolve the current control-semantic gaps before density measurements depend on audition and Energy behavior.

### M18.5.1 Replace audition modes with independent Wet and Dry gains

Tasks: remove Processed/Dry Bypass and the Wet Only/Audition Mix choice; add independent linear Wet Gain and Dry Gain controls shared by live audition and export; default new state to Wet 0.5 and Dry 0.0 to preserve the current default Master Audition Gain loudness; smooth continuous changes; define summing, clipping, and host-latency behavior explicitly.

Acceptance criteria:

- Output is `Wet Gain x graph output + Dry Gain x selected source`, using two independent 0...1 linear gains with no hidden normalization.
- Wet 1/Dry 0 is wet-only, Wet 0/Dry 1 is dry-only, and nonzero values for both create the same documented mix in live audition and export.
- Continuous gain edits are click-safe and deterministic; independent gains may sum above unity, so metering/safety and documentation disclose headroom rather than silently scaling the result.
- Dry remains immediate while wet retains the graph's visible latency/predelay; host latency follows an explicit earliest-audible-path policy and never silently delays dry by a long Pitch Shift path.
- Automated fixtures make wet-only, dry-only, and combined output measurably distinct and verify saved-state compatibility with the retired master/processed controls.

### M18.5.2 Remove redundant audition and capture controls

Tasks: remove Master Audition Gain, the processed toggle, export mix mode, and `Mute Live Input`; make impulse capture always silence every selected audition source while injecting its measurement impulse; retain Emergency Mute as the explicit safety control; migrate legacy master gain conservatively without changing old projects unexpectedly.

Acceptance criteria:

- Impulse capture never includes live input, loaded audio, or test-source leakage and therefore needs no user-facing input-mute option.
- Master Audition Gain, Processed/Dry Bypass, Wet Only, and Audition Mix no longer appear in the UI or new saved state.
- Existing `masterGain` host state migrates exactly to Wet Gain with Dry Gain 0.0, while new state stores both gains explicitly.
- Emergency Mute and the numerical safety latch remain after the wet/dry sum.
- Live audition and exported samples use the same tested Wet/Dry law.

### M18.5.3 Define loop-range export

Tasks: add an explicit Entire File versus Selected Loop export range; pass source-frame bounds to the offline exporter; define tail rendering at the selected end; keep loop playback crossfade separate from exported range semantics.

Acceptance criteria:

- Entire File exports the complete source once plus its bounded tail.
- Selected Loop exports exactly the selected source interval once plus its bounded tail unless a repeated-loop count is explicitly chosen later.
- Output duration, resampling boundaries, and cancellation are deterministic and tested.
- The export dialog states the selected range before rendering.

### M18.5.4 Bind Energy to compiled graph execution

Tasks: add bounded per-node telemetry lanes to the prepared graph plan; publish coherent 30 Hz snapshots from the active revision; keep telemetry free when disabled; map node energy to downstream cables; handle topology crossfades and graph replacement safely.

Acceptance criteria:

- Energy On visibly responds on Barr and every compiled factory while audio plays.
- Energy Off performs no observation work in the audio callback.
- Snapshot node IDs and revision match the active visible graph; stale frames cannot decorate a replacement graph.
- Polling, dropped frames, and the visualization cannot change rendered audio.
- Reduced-motion behavior remains accessible and explicit.

Milestone exit criteria:

- Audition and export labels describe what users actually hear or receive, loop range is explicit, and Energy visualizes the active compiled graph rather than the legacy fixed engine.

---

## M19. Perceptual density measurement

### M19.1 Add deterministic density analysis

Tasks: measure windowed echo density, active-peak count, crest factor, kurtosis, energy variation, autocorrelation recurrence, spectral flatness, and early/middle/late stereo correlation; qualify sparse and dense reference fixtures; baseline every factory.

Acceptance criteria:

- Metrics distinguish controlled sparse and dense fixtures and remain stable at 44.1, 48, and 96 kHz.
- Each metric has a documented perceptual meaning and limitation.
- Versioned machine-readable reports preserve current factory baselines.

### M19.2 Add a density inspector

Tasks: plot density over time, mark prominent recurrence periods, expose early/middle/late summaries, and integrate explanations into teaching mode.

Acceptance criteria:

- Sparse and dense regions are distinguishable without color alone at every supported editor size.
- The inspector adds no audio-thread work while closed.
- Tests, screenshot, and an interaction video cover the finished UI.

---

## M20. Dense figure-eight tank

### M20.1 Implement a calculated two-branch reference

Tasks: build cross-coupled branches with input diffusion, distributed tank diffusion, in-loop damping, unequal delays, independent output taps, subtle modulation, and delay-aware RT60 gains.

Acceptance criteria:

- All feedback is visible, delayed, bounded, partition-deterministic, and finite at supported rates and parameter extremes.
- Both branches track the requested decay within a documented tolerance.
- Density growth materially exceeds Barr and Gravity baselines without merely darkening discrete repeats.

### M20.2 Tune and publish Dense Figure Eight

Tasks: search candidate delay sets, reject recurrence relationships, tune diffusion/damping/modulation/output taps, expose useful macros, and add a teaching overlay.

Acceptance criteria:

- Qualified recurrence, decay, stereo, pitch-stability, CPU, memory, and listening fixtures pass.
- The shipped patch exactly matches its visible/native graph and remains editable.

---

## M21. Constrained four-line FDN

### M21.1 Implement the four-line Hadamard network

Tasks: add four unequal delay lines, normalized 4x4 Hadamard mixing, per-line delay-aware gains and damping, bounded modulation, and explicit input/output vectors.

Acceptance criteria:

- The matrix preserves energy within floating-point tolerance and does not control decay implicitly.
- RT60 mapping, reset, serialization, block partitioning, storage, and safety pass deterministic tests.

### M21.2 Make matrix mixing inspectable

Tasks: add a transparent four-input/four-output Matrix Mixer compound with coefficients, polarity, energy, and an expanded equivalent sum/gain view.

Acceptance criteria:

- DSP semantics are persisted explicitly and never exist only in presentation code.
- Expansion exposes the complete equivalent routing without making the minimum-size canvas unusable.
- Invalid or amplifying matrices follow a documented reject/normalize policy.

### M21.3 Publish a four-line Dense Room factory

Tasks: qualify delay set, diffusion, injection/pickup vectors, damping, modulation, size/decay/width controls, and circulation teaching overlays.

Acceptance criteria:

- Density and recurrence improve materially over current factories and the figure-eight reference.
- The factory stays finite, decorrelated, mono-compatible, inspectable, and exact across supported rates and saved-state round trips.

---

## M22. Assisted delay-set tuning

### M22.1 Generate deterministic candidates

Tasks: search bounded delay ranges; penalize common factors, repeated differences, and near periods; calculate RT60 gains; enforce memory/modulation margins; record the seed and rejection reasons.

Acceptance criteria:

- Identical inputs reproduce identical rankings and invalid/over-budget candidates never render.
- Intentionally good and poor fixtures validate each scoring component.

### M22.2 Rank rendered responses

Tasks: render impulse, noise-burst, percussion, and tonal fixtures; score density, recurrence, coloration, decay, and stereo behavior independently; export candidates as ordinary patches.

Acceptance criteria:

- No unstable candidate can pass and no single composite score hides a failed dimension.
- Top candidates have normalized listening fixtures and complete reproduction provenance.

### M22.3 Add non-destructive assisted tuning

Tasks: offer smoother/less-metallic/wider/less-modulated alternatives, explain proposed changes, preview safely, and support accept/cancel/undo.

Acceptance criteria:

- Suggestions never silently mutate saved state; cancel restores the exact graph/runtime.
- Preview switching is click-safe, level matched, measurable, and covered by screenshot/video evidence.

---

## M23. Dense-network optimization

### M23.1 Profile dense tanks by processor family

Tasks: separate delay, matrix, damping, modulation, routing, telemetry, normal, and crossfade costs across supported rates and blocks.

Acceptance criteria:

- Exact-commit baselines identify a measured dominant cost before any specialized implementation is accepted.

### M23.2 Add reusable dense-network kernels

Tasks: evaluate SIMD across FDN lines, Hadamard butterflies, batched filters, incremental fractional cursors, fused read/damp/gain/mix/write paths, and shared modulation tables.

Acceptance criteria:

- Retained kernels improve their target fixture by at least 10-15%, preserve visible semantics and audio equivalence, allocate/lock nowhere in the callback, and fall back safely for unsupported arrangements.

---

## M24. Dense-reverb product qualification

### M24.1 Run comparative listening and measurement qualification

Tasks: loudness-match Barr, Gravity, Figure Eight, and FDN across percussion, speech, piano, pads, noise, room/hall, dark, and modulated settings; record objective results separately from listening notes.

Acceptance criteria:

- Dense designs demonstrate less repeat-like tails while explicitly evaluating ringing, smearing, pitch motion, and mono compatibility; failed fixtures remain visible and drive retuning.

### M24.2 Qualify hosts, safety, documentation, and release artifacts

Tasks: exercise extreme controls, continuous editing, automation, save/reopen, offline export, sample-rate changes, standalone/VST3 packaging, and the full release gate.

Acceptance criteria:

- No non-finite output, runaway publication, callback allocation, or invalid state replacement occurs.
- Supported graph/rate/block/quality limits are documented, the Windows package validates, and clean `main` CI passes.

Milestone exit criteria:

- The project ships at least one convincingly dense, teachable, editable reverb with reproducible tuning evidence and honest audition/export/telemetry behavior.

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
