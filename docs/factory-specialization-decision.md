# Factory specialization and JIT decision

Milestone 18.4 keeps the optimized prepared graph executor as the shipping path. It does **not** add factory-specific AOT kernels or runtime native-code JIT compilation.

## Evidence

The checked-in [paired Release comparison](../artifacts/measurements/barr-execution-comparison-m18-4.json) runs Barr's hand-written direct/reference engine and the optimized visible-graph executor with the same deterministic stereo input, state progression, sample rates, block sizes, and 2,000 timed callbacks per case.

Across 44.1, 48, and 96 kHz and 32/64/128/256/512-sample blocks:

- all 15 direct/generic output pairs are sample-identical;
- both paths have zero deadline misses;
- generic p95 callback time is 0.995x to 1.077x the direct path, with a median ratio of 1.010x;
- the largest relative gap occurs in the tiny 32-sample cases, where it is only 0.1 microseconds on this machine;
- at 512 samples the paths are effectively equal.

The direct Barr implementation is the strongest useful fixed-topology AOT comparison because it already expresses that exact factory as ordinary C++ without graph dispatch. The remaining generic overhead is too small to justify an additional generated factory prototype. Creating one would duplicate runtime state, publication, automation, telemetry, safety, and identity behavior for a result bounded here by roughly 8% of an already sub-2%-of-deadline callback.

The existing identity tests remain important: the visible Barr graph and the direct DSP reference must render identically. The direct path stays as a reference and qualification oracle, not a hidden substitute for the editor's graph.

## Why JIT is rejected

A runtime JIT has no measured performance problem left to solve in the Barr comparison or the M18.2 flagship matrix. It would add executable-memory policy, Windows code-signing and security review, host compatibility, crash containment, cache invalidation, debugging/symbolization, architecture-specific code generation, and another equivalence surface. Those costs are not accepted for a 0-8% Barr dispatch gap.

Prepared typed kernels, SCC-local scheduling, buffer reuse, safe in-place aliases, fused copy/gain/mix operations, and tick-bounded control ramps provide the useful compilation benefits while keeping ordinary portable C++ and visible graph semantics. This decision makes no claim that arbitrary future graphs can never need specialization. A future proposal must show a repeatable remaining bottleneck on representative graphs, at least a material double-digit callback reduction beyond the current executor, and exact audio/identity equivalence before AOT or JIT work resumes.

## Supported release envelope

- Sample rates: 44.1, 48, and 96 kHz in the full flagship performance matrix; Pitch Shift correctness is additionally qualified at 192 kHz.
- Host blocks: 32, 64, 128, 256, and 512 samples in the matrix; graph execution remains partition-deterministic for other valid host partitions covered by tests.
- Flagship graphs: Barr Reference, Gravity Diffusion, Safe Parallel Shimmer, Split Feedback Shimmer, and Reverse Cosmic Shimmer.
- Quality: Draft, Normal, and High are explicit saved policies. Normal is the released reference. All use the same latency, 1 kHz modulation plan, and 30 Hz telemetry rate.
- Pitch: continuous -12 through +12 semitones, including ordinary +/-7-semitone fifths.
- Limits: the compiler's published node, connection, feedback, control-mapping, prepared-memory, and delay-memory limits remain authoritative; unsupported graphs fail before publication rather than falling back to hidden code.

The final release gate builds standalone and VST3 targets, runs native/CLI/web/script tests, checks documentation/accessibility/provenance/release contracts, packages the Windows archive, and validates its manifest, checksums, standalone copy, and VST3 layout.
