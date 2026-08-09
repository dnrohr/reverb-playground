# Acyclic graph compilation

M3.3 introduces the first general native runtime for user-constructed patches. `compileAcyclicGraph` validates a semantic `GraphDocument`, creates a deterministic execution schedule, allocates every signal buffer and DSP state object, and returns a prepared runtime. It supports Stereo Input/Output, Gain, Sum, Delay, Allpass, and Low-pass nodes connected by mono audio cables.

## Deterministic schedule

The compiler uses Kahn topological sorting with node ID as the stable tie-breaker. Node and connection array order therefore cannot change the schedule for the same semantic graph. This entry point intentionally rejects every directed cycle, including delay-containing cycles; the companion [feedback compiler](feedback-graph-compilation.md) applies split-phase Delay semantics.

Exactly one Stereo Input and Stereo Output are required. Node port/parameter contracts, connection endpoints and types, and one-cable-per-input occupancy are checked before preparation. Current safety limits are also enforced: 256 nodes, 512 connections, 64 delay-bearing primitives, 10 seconds per delay, and 192 kHz maximum sample rate.

## Disconnected and unreachable nodes

Legal acyclic nodes are scheduled even if they do not contribute to output, keeping compilation deterministic and diagnostics non-destructive. An unconnected input reads a prepared silence buffer. The compiler emits sorted warnings for:

- a disconnected non-I/O node, which processes silence and is discarded;
- a node unreachable from Stereo Input;
- a node that cannot reach Stereo Output, whose result is discarded.

Warnings do not prevent publication. Errors do.

## Preparation, processing, and publication

Compilation and DSP preparation run entirely on the caller/control thread. The prepared runtime owns fixed-capacity signal buffers sized to the host's maximum block, plus each primitive's state. `process` is `noexcept`, performs no topology discovery or container resizing, and returns silence for an oversized or inconsistent block.

`AcyclicRuntimeHost` publishes a fully prepared pointer atomically. The audio callback marks its bounded processing interval with an atomic flag; publication may wait on the control thread before reclaiming the retired runtime. The audio thread never waits, locks, allocates, or destroys a runtime. A compilation error does not exchange the active pointer, so the last valid runtime remains audible.

This runtime is not yet connected to the editor/native bridge. Feedback scheduling is now available through the same prepared runtime; later M3 integration makes a complete constructed reverb the active plugin graph.

## Verification

Native tests prove schedule identity after node/connection reordering, compare Gain/Sum and Delay graphs with direct calculations, compare the full acyclic Barr primitive graph sample-for-sample with `BarrReference`, verify deterministic warnings and bounded storage, and confirm failed publication leaves the preceding runtime processing audio.

UI unchanged; no screenshot or video was required for this compiler-only task.
