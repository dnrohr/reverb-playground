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
- [Progress log](progress.md) - completed roadmap tasks and verification evidence.
- [Patch format v1](patch-format.md) - versioned semantic graph, editor layout, typed ports, units, validation, and migration policy.
- [Real-time and safety contract v1](real-time-safety-contract-v1.md) - feedback legality, audio-thread rules, resource bounds, numerical containment, and runtime publication.
- [DSP primitives](dsp-primitives.md) - equations, units, preparation/processing lifetime, and deterministic test tolerances.
- [Barr reference implementation](barr-reference-implementation.md) - fixed stereo/mono channel plan, public-primitive topology, and explicit departures from original hardware.
- [Offline rendering](offline-rendering.md) - headless WAV command, analysis JSON, deterministic inputs, tolerances, and golden-fixture policy.
- [Response measurements](response-measurements.md) - onset, length, peaks, stereo difference, Schroeder decay, RT60 estimation, and refusal rules.
- [Audible reference harness](audible-reference-harness.md) - standalone/plugin audition controls, device behavior, live safety, and UI evidence.
- [Schematic editor interactions](schematic-editor-interactions.md) - three-pane layout, pointer and keyboard controls, signal semantics, and scaling contract.
- [Runtime graph binding](runtime-graph-binding.md) - native snapshot contract, identity validation, live inspector values, and processing boundaries.
- [Editable node creation](editable-node-creation.md) - primitive defaults, stable IDs, required I/O, structural undo, and draft/runtime boundaries.
- [Typed connection editing](typed-connection-editing.md) - mono branching, endpoint validation, occupied-input choices, automatic Sum insertion, and cable hit targets.
- [Acyclic graph compilation](acyclic-graph-compilation.md) - deterministic scheduling, prepared native runtimes, reachability warnings, and last-valid publication.
- [Feedback graph compilation](feedback-graph-compilation.md) - SCC analysis, exact algebraic-loop diagnostics, split-phase Delay semantics, and compiler budgets.
- [Delay-memory planning](delay-memory-planning.md) - requested/allocated inspection, prepared arenas, project budget, boundaries, and safe sample-rate recalculation.
- [Unified graph history and clipboard](unified-graph-history-and-clipboard.md) - mixed-operation undo/redo, bounded history, clean-state identity, and subgraph copy/paste.
- [Feedback-loop highlighting](feedback-loop-highlighting.md) - bounded directed-cycle analysis, active/alternate styling, and topology-derived loop facts.
- [Impulse audition and capture](impulse-audition-and-capture.md) - safe stimulus, visible bounds, live-input isolation, lock-free capture publication, and deterministic repeats.
- [Stereo impulse and decay view](stereo-impulse-and-decay-view.md) - accessible channel waveforms, Schroeder decay, bounded zoom/pan, T30 estimates, and refusal explanations.
- [Live energy telemetry](live-energy-telemetry.md) - fixed, non-blocking RMS snapshots; smoothed node/cable activity; disable and reduced-motion behavior.
- [Runtime resource and safety diagnostics](runtime-resource-safety-diagnostics.md) - labeled estimates/measurements, prepared memory, clipping, revision-bound safety events, and explicit recovery.
- [Runaway-feedback safety and recovery](runaway-feedback-safety.md) - sustained detection, likely-loop guidance, muted Undo, and state-clearing recovery.
- [Reverse, inverse, gated, and Bloom requirements](reverse-and-gated-architecture-requirements.md) - distinct impulse contracts, the causal first method, required primitives, and naming rules.
- [Envelope Follower and Hold Gate](envelope-follower-and-hold-gate.md) - detector equation, gate timing/retrigger semantics, restricted routing, persistence, and feedback safety.
- [Reverse-envelope and gated factory patches](reverse-and-gated-factory-patches.md) - visible topologies, musical controls, deterministic envelope metrics, and audio fixtures.
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
