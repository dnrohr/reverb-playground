# Compiled block kernels

Status: implemented in M17.3

The graph compiler now specializes eligible block-wise routing before publication. The schematic and its stable node schedule remain authoritative: fusion changes an execution record, not the saved graph, node IDs, inspector identity, cables, or host state.

## Prepared kernels

The block-kernel layer provides copy, static gain, scaled sum, and weighted sum operations over contiguous spans. On SSE2-capable builds it processes four floats per iteration with unaligned-safe loads and stores; every kernel retains a scalar remainder and a complete scalar fallback for other platforms. Source/destination aliasing is supported where the graph allocator permits it.

The compiler currently recognizes these static linear routes:

- consecutive Gain blocks become one combined Gain kernel;
- Sum followed by Gain becomes one scaled-sum kernel;
- Gain followed by a static Low-pass applies input scale inside the filter recurrence;
- static Gain inputs feeding Sum become one weighted-sum kernel;
- static Low-pass followed by Gain scales the filter output while preserving its unscaled state;
- Sum → Gain → static Low-pass becomes one summed/scaled filter kernel.

The original operation records remain in the prepared schedule. Folded records are marked as represented by the surviving typed kernel, so diagnostics can report both the visible node count and the smaller executed group count.

## Hard boundaries

Fusion is prohibited across feedback or causal sample-wise regions, modulation, fan-out/taps, nonlinear or delay-bearing processors, and inspector/telemetry-observed signals. Pitch Shift, Delay, All-pass, Envelope Follower, and Hold Gate remain independent stateful operations. These reasons are compiled and exposed in the diagnostics drawer rather than inferred in the audio callback.

No node-type strings, processor discovery, allocation, or graph traversal occurs per sample. Operation and fused-kernel enums are selected off-thread and immutable at publication.

## Measurement

`artifacts/measurements/performance-matrix-m17-3.json` contains the same 75 Release cases as M17.2. At 48 kHz and 512 samples:

| Graph | Fused groups | Folded visible blocks | SIMD kernels | Prepared-memory change |
| --- | ---: | ---: | ---: | ---: |
| Barr Reference | 0 | 0 | 1 | 0 B |
| Gravity Diffusion | 1 | 2 | 1 | -2,048 B |
| Safe Parallel Shimmer | 1 | 2 | 2 | 0 B |
| Split-Feedback Shimmer | 1 | 2 | 1 | -2,048 B |
| Reverse Cosmic Shimmer | 3 | 5 | 3 | 0 B |

All cases remain finite and within the normal and topology-crossfade budgets, with delay storage unchanged. Callback timings at these small loads are noisy and are not presented as proof of CPU improvement; M17.4 performs the broader qualification. The deterministic reduction in executed operation groups is already explicit for both flagship feedback shimmer graphs at 48 and 96 kHz.

## Verification

Tests compare SIMD-capable copy/gain/sum/mix kernels with scalar references, including aliased buffers and non-multiple-of-four tails. A Sum → Gain → Low-pass fixture compares the fused result with the separate scalar processors while proving the original schedule is unchanged. The 252-node maximum graph, denormal/numerical safety, golden renders, every factory topology, deterministic reload, feedback, crossfade, and host-state suites remain authoritative regression coverage.
