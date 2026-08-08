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

## Local source tree

- `../BarrVerb/` - requested clone of `ErroneousBosh/BarrVerb`, including initialized DPF submodules.
- `../research/sources/MIDIVerb_RE/` - Eric Brombaugh and Paul Schreiber's reverse-engineering archive, retained locally because it is the best available bridge between the hardware and BarrVerb.

## Evidence labels used in these notes

- **Documented** - directly supported by Barr, hardware analysis, code, schematic, manual, or another primary artifact.
- **Reported** - a technically credible secondary source states it, but the underlying artifact has not yet been independently checked here.
- **Inference** - reasoned from code, impulse-response behavior, or related designs; not presented as a known Barr design fact.

This is a research notebook, not a claim that every Alesis preset shared one topology. The evidence instead points to substantial variation between products and programs.
