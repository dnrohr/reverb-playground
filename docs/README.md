# Keith Barr reverb research

Working notes begun 2026-08-08. This directory connects three related subjects:

1. Keith Barr's reverb design ideas and their evolution.
2. The original Alesis MIDIVerb hardware and microcode.
3. BarrVerb, Gordon Pearce's software interpretation of that machine.

## Reading order

- [Keith Barr architectures](keith-barr-reverb-architectures.md) - the conceptual and historical overview.
- [BarrVerb code review](barrverb-code-review.md) - a line-by-line architectural review, implementation differences, and engineering risks.
- [Source map](sources.md) - primary sources, reverse-engineering artifacts, informed secondary commentary, and leads for further research.
- [Visual reverb constructor specification](visual-reverb-constructor-spec.md) - product definition, interaction model, technical architecture, risks, and open decisions.
- [Architecture decision records](adr/README.md) - accepted implementation, licensing, platform, and distribution choices.
- [Execution roadmap](roadmap.md) - ordered milestones, tasks, acceptance criteria, and delivery policy.
- [Development guide](development.md) - supported toolchain, source layout, build/test commands, and artifact conventions.
- [Windows package installation](windows-package-installation.md) - install, run, verify, and remove the standalone/VST3 alpha package.
- [Barr reference tutorial](getting-started-barr-tutorial.md) - clean-install first run, safe audition, measurement, editing, diagnostics, and save/reload.
- [Module and visualization reference](module-and-visualization-reference.md) - every shipped block, unit, range, socket, display, and constraint.
- [Factory patch catalog and compatibility](factory-patch-compatibility.md) - shipped families, metadata, admission rules, schema migrations, and CI guarantees.
- [Alpha usability and safety protocol](alpha-usability-safety-protocol.md) - anonymous participant method, complete task journey, accessibility matrix, and stop rules.
- [Alpha validation findings](alpha-validation-findings.md) - privacy-preserving session outcomes, prioritized defects, and release-blocker inventory.
- [Windows alpha package and host validation](windows-alpha-package-and-host-validation.md) - reproducible archive contract and named-host evidence.
- [M24 dense-reverb release qualification](m24-release-qualification.md) - current exact-build safety, workflow, host, package, and supported-envelope evidence.
- [Focused workspace shell](focused-workspace-shell.md) - compact application menus, continuously visible patch/safety state, responsive native controls, and measured canvas-space gain.
- [Responsive workspace docks](responsive-workspace-docks.md) - Balanced, Create Focus, and Learn & Inspect arrangements, independent dock controls, narrow overlays, and presentation-only persistence.
- [Unified context dock](unified-context-dock.md) - Inspector/Analyze/Learn routing, revision-bound evidence, dormant telemetry rules, and accessible tab navigation.
- [Primary user journey qualification](primary-user-journey-qualification.md) - measured musician, sound-designer, and learner workflows, fixed internal regressions, and the boundary before non-implementer sessions.
- [Progress log](progress.md) - completed roadmap tasks and verification evidence.
- [Project license](../LICENSE), [third-party notices](../THIRD_PARTY_NOTICES.md), [asset provenance](../ASSET_PROVENANCE.md), and [contribution/DCO policy](../CONTRIBUTING.md) - open-source distribution boundaries.
- [Patch format v1](patch-format.md) - versioned semantic graph, editor layout, typed ports, units, validation, and migration policy.
- [Real-time and safety contract v1](real-time-safety-contract-v1.md) - feedback legality, audio-thread rules, resource bounds, numerical containment, and runtime publication.
- [DSP primitives](dsp-primitives.md) - equations, units, preparation/processing lifetime, and deterministic test tolerances.
- [Barr reference implementation](barr-reference-implementation.md) - fixed stereo/mono channel plan, public-primitive topology, and explicit departures from original hardware.
- [Offline rendering](offline-rendering.md) - headless WAV command, analysis JSON, deterministic inputs, tolerances, and golden-fixture policy.
- [Response measurements](response-measurements.md) - onset, length, peaks, stereo difference, Schroeder decay, RT60 estimation, and refusal rules.
- [Perceptual density measurement](perceptual-density-measurement.md) - windowed echo density, recurrence, coloration, stereo summaries, controlled fixtures, and all-factory baselines.
- [Density inspector](density-inspector.md) - capture-driven density curves, recurrence markers, region summaries, teaching semantics, and responsive evidence.
- [Audible reference harness](audible-reference-harness.md) - standalone/plugin audition controls, device behavior, live safety, and UI evidence.
- [Audio-file source and transport contract](audio-file-source-and-transport-contract.md) - standalone source arbitration, channel/resampling policy, prepared read-ahead ownership, deterministic transport, underrun behavior, and persistence boundaries.
- [Standalone audio-file audition](standalone-audio-file-audition.md) - load/drop workflow, waveform transport, source selection, loop editing, dry comparison, safety, and privacy.
- [Processed-file export](processed-file-export.md) - shared Wet/Dry gain semantics, range selection, PCM24 format, resampling, bounded tails, cancellation, safety, and atomic publication.
- [Audio workflow guide](audio-workflow-guide.md) - when to use file audition, live input, Test Impulse, offline export, or the VST3, plus recovery behavior and limitations.
- [Compact audition drawer](compact-audition-drawer.md) - responsive collapsed/open transport layout, minimum-width guarantees, and current UI evidence.
- [Schematic editor interactions](schematic-editor-interactions.md) - three-pane layout, pointer and keyboard controls, signal semantics, and scaling contract.
- [Runtime graph binding](runtime-graph-binding.md) - native snapshot contract, identity validation, live inspector values, and processing boundaries.
- [Editable node creation](editable-node-creation.md) - primitive defaults, stable IDs, required I/O, structural undo, and draft/runtime boundaries.
- [Typed connection editing](typed-connection-editing.md) - mono branching, endpoint validation, occupied-input choices, automatic Sum insertion, and cable hit targets.
- [Acyclic graph compilation](acyclic-graph-compilation.md) - deterministic scheduling, prepared native runtimes, reachability warnings, and last-valid publication.
- [Feedback graph compilation](feedback-graph-compilation.md) - SCC analysis, exact algebraic-loop diagnostics, split-phase Delay semantics, and compiler budgets.
- [Delay-memory planning](delay-memory-planning.md) - requested/allocated inspection, prepared arenas, project budget, boundaries, and safe sample-rate recalculation.
- [Unified graph history and clipboard](unified-graph-history-and-clipboard.md) - mixed-operation undo/redo, bounded history, clean-state identity, and subgraph copy/paste.
- [Collapsible graph groups](collapsible-graph-groups.md) - named layout-only groups, explicit typed boundaries, loop reveal, persistence, and nesting limits.
- [Feedback-loop highlighting](feedback-loop-highlighting.md) - bounded directed-cycle analysis, active/alternate styling, and topology-derived loop facts.
- [Impulse audition and capture](impulse-audition-and-capture.md) - safe stimulus, visible bounds, live-input isolation, lock-free capture publication, and deterministic repeats.
- [Stereo impulse and decay view](stereo-impulse-and-decay-view.md) - accessible channel waveforms, Schroeder decay, bounded zoom/pan, T30 estimates, and refusal explanations.
- [Live energy telemetry](live-energy-telemetry.md) - fixed, non-blocking RMS snapshots; smoothed node/cable activity; disable and reduced-motion behavior.
- [Runtime resource and safety diagnostics](runtime-resource-safety-diagnostics.md) - labeled estimates/measurements, prepared memory, clipping, revision-bound safety events, and explicit recovery.
- [Hybrid executor qualification](hybrid-executor-qualification.md) - exact-commit M16/M17 comparison, correctness coverage, shimmer operation reduction, timing/memory gates, and M17 exit decision.
- [Runaway-feedback safety and recovery](runaway-feedback-safety.md) - sustained detection, likely-loop guidance, muted Undo, and state-clearing recovery.
- [Reverse, inverse, gated, and Bloom requirements](reverse-and-gated-architecture-requirements.md) - distinct impulse contracts, the causal first method, required primitives, and naming rules.
- [Envelope Follower and Hold Gate](envelope-follower-and-hold-gate.md) - detector equation, gate timing/retrigger semantics, restricted routing, persistence, and feedback safety.
- [Reverse-envelope and gated factory patches](reverse-and-gated-factory-patches.md) - visible topologies, musical controls, deterministic envelope metrics, and audio fixtures.
- [Large modulated, inverse, and shimmer topologies](large-modulated-and-shimmer-reverb-topologies.md) - public behavioral evidence, the original cosmic-reverse graph, and the pitch-shift contract required for honest shimmer.
- [Gravity behavior and measurements](gravity-behavior-and-measurements.md) - bipolar control semantics, causal inverse boundary, deterministic shape metrics, and three reference targets.
- [Curve Mapper](curve-mapper.md) - linear, power, and exponential control equations, validation, runtime interpolation, inspection, and schema-v2 compatibility.
- [Macro control source](macro-control-source.md) - named normalized control, fixed 20 ms smoothing, visible branching, reachable-range inspection, and persistence.
- [Gravity macro presentation](gravity-macro-presentation.md) - explicit factory designation, bipolar instrument control, non-measured envelope guide, and Expand/Focus inspection.
- [Gravity Diffusion topology design](gravity-diffusion-topology-design.md) - eight progressive taps, 12-allpass density plan, delayed damped feedback, stereo extraction, and worst-case budgets.
- [Normalized Gravity weighting](normalized-gravity-weighting.md) - eight visible Curve Mapper branches, constant-sum stereo weighting, measured envelope ordering, energy tolerance, and automation safety.
- [Gravity Diffusion complementary controls](gravity-diffusion-controls.md) - visible Size, Feedback, Damping, and Modulation mappings, dual-LFO motion, runtime boundaries, and extreme-state safety.
- [Gravity Diffusion reference states](gravity-reference-states.md) - tuned inverse/bloom/forward controls, loudness-matched audio, machine-readable shape evidence, and critical audition notes.
- [Gravity Diffusion factory patch and teaching view](gravity-diffusion-factory-and-teaching.md) - project-authored provenance, reconstruction and modification guide, honest prediction/reference/live-measurement hierarchy, A/B workflow, and UI evidence.
- [Gravity Diffusion validation and Windows package](gravity-validation-and-package.md) - multi-rate safety, macro sweeps, named-host restore, physical scaling evidence, and exact package identity.
- [Dense figure-eight reference](dense-figure-eight-design.md) - calculated cross-coupled decay, delay selection, density qualification, factory macros, and UI evidence.
- [Four-line FDN reference](four-line-fdn-design.md) - normalized Hadamard feedback, per-line decay, expanded visible routing, and deterministic safety gates.
- [Assisted delay-set tuning](assisted-delay-set-tuning.md) - deterministic candidate search, independent arithmetic penalties, RT60/memory planning, and rejection policy.
- [Visible Pitch Shift primitive design](pitch-shift-primitive-design.md) - mono dual-read-head semantics, fixed causal latency, grain controls, automation, quality limits, and prepared resource budgets.
- [Pitch Shift inner-loop optimization](pitch-shift-inner-loop-optimization.md) - prepared phase/window/addressing work, transition behavior, same-machine Release measurements, and qualification boundary.
- [Compiled control-ramp processing](compiled-control-ramp-processing.md) - bounded ramp segments, block and causal processor kernels, equivalence coverage, and exact-commit performance evidence.
- [Dense-network performance profile](dense-network-performance-profile.md) - M23 normal, Energy, and crossfade measurements with processor-family attribution and the Four-Line kernel decision.
- [Dense-reverb qualification](dense-reverb-qualification.md) - loudness-matched Barr/Gravity/Figure Eight/Four-Line objective results, listening reels, visible failures, and the retuning gate.
- [M24 comparative listening session](m24-listening-session-template.md) - anonymous eight-reel listening and mono-check worksheet required to close dense-reverb qualification.
- [Reverse grains and stereo decorrelation](reverse-grain-and-stereo-decorrelation.md) - deterministic phase pairing, causal boundaries, transient evidence, compatibility, and multirate budgets.
- [Reverse Cosmic Shimmer topology design](reverse-cosmic-shimmer-topology-design.md) - causal-rise front end, paired reverse-octave returns, dark circulation, slow motion, and unequal stereo extraction.
- [Safe Parallel Shimmer topology design](safe-parallel-shimmer-design.md) - visible post-tank octave branch, structural non-recirculation, stereo extraction, alignment, memory, and loudness budgets.
- [Safe Parallel Shimmer factory patch and teaching view](safe-parallel-shimmer-factory-and-teaching.md) - factory generation, one-pass teaching contract, spectral no-staircase evidence, persistence, and UI evidence.
- [Split-Feedback Shimmer topology design](split-feedback-shimmer-design.md) - independently bounded normal and octave returns, visible filtering, cycle legality, and recovery behavior.
- [Split-Feedback Shimmer validation](split-feedback-shimmer-validation.md) - time-resolved octave buildup, parallel contrast, control independence, quality disclosure, and multirate edit safety.
- [Split-Feedback Shimmer factory patch and teaching view](split-feedback-shimmer-factory-and-teaching.md) - factory generation, independent loop focus, circulation evidence, persistence, safety boundaries, and UI evidence.
- [Visualization teaching overlays and A/B comparison](visualization-teaching-overlays.md) - measured rise/gate landmarks, honest explanatory boundaries, Learn disable behavior, and Barr/design audition switching.
- [Control-rate graph semantics](control-rate-graph-semantics.md) - typed parameter sockets, fixed-rate interpolation, bounded evaluation, mapping formula, and schema-v2 persistence.
- [LFO and control-mapping blocks](lfo-and-control-mapping.md) - sine/triangle generation, free-run/restart behavior, explicit scale/offset/polarity, range preview, and branching.
- [Modulated Delay and Allpass](modulated-delay-and-allpass.md) - fractional linear delay taps, bounded coefficient modulation, audible behavior, memory planning, and runtime boundary.
- [Runtime topology publication](runtime-topology-publication.md) - newest-wins off-thread compilation, bounded block-boundary swaps, off-thread reclamation, failures, and revision diagnostics.
- [Topology-change crossfades](topology-change-crossfades.md) - live editor publication, fixed 10 ms transitions, rapid-edit coalescing, diagnostics, and Windows scaling behavior.

## Local source tree

- `../BarrVerb/` - requested clone of `ErroneousBosh/BarrVerb`, including initialized DPF submodules.
- `../research/sources/MIDIVerb_RE/` - Eric Brombaugh and Paul Schreiber's reverse-engineering archive, retained locally because it is the best available bridge between the hardware and BarrVerb.

## Evidence labels used in these notes

- **Documented** - directly supported by Barr, hardware analysis, code, schematic, manual, or another primary artifact.
- **Reported** - a technically credible secondary source states it, but the underlying artifact has not yet been independently checked here.
- **Inference** - reasoned from code, impulse-response behavior, or related designs; not presented as a known Barr design fact.

This is a research notebook, not a claim that every Alesis preset shared one topology. The evidence instead points to substantial variation between products and programs.
