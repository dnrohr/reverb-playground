# Hybrid executor qualification

Status: complete in M17.4

M17 keeps the visible schematic as the authoritative program while compiling a more efficient execution plan underneath it. This qualification compares the exact M16.4 baseline at `b971cc010235` with the published M17.3 executor at `4325c8053bf8`, on the same machine and MSVC 19.44 Release toolchain.

The machine-readable gate is [hybrid-executor-qualification-m17-4.json](../artifacts/measurements/hybrid-executor-qualification-m17-4.json). Regenerate it with:

```powershell
python scripts/qualify_hybrid_executor.py
```

## Result

All gates pass across the complete 75-case matrix.

| Gate | M16 baseline | M17 qualified | Result |
| --- | ---: | ---: | --- |
| Finite cases | 75 | 75 | Pass |
| Normal p95 safety budget | ≤80% | 75/75 | Pass |
| Crossfade p95 safety budget | ≤160% | 75/75 | Pass |
| Worst normal p95 load | 7.251% | 7.116% | Pass; 1.9% lower |
| Worst crossfade p95 load | 13.193% | 13.671% | Pass; 3.6% higher, within 10% gate |
| Worst observed crossfade peak | 71.67% | 84.42% | Pass; below 160% safety envelope |
| Maximum compile time | 861 µs | 896 µs | Pass; 4.1% higher |
| Maximum request-to-active | 889 µs | 929 µs | Pass; 4.5% higher |
| Maximum prepared memory | 1,409,084 B | 1,386,556 B | Pass |
| Prepared-memory non-regression | — | 75/75 cases | Pass; 512–30,720 B saved per case |
| Compiled latency equivalence | — | 75/75 cases | Pass |

Compile and request-to-active results remain inside the M16 10% regression gate, so no exception rationale is required. Compilation remains off the audio thread and completes below one millisecond in the observed worst cases.

## Required shimmer improvement

M16 permits either elapsed callback improvement or a deterministic operation/copy-count reduction at both 48 and 96 kHz. The latter is the reliable gate here because these callback loads are small enough for isolated timing noise to dominate some individual cases.

| Graph | Rate | Block cases | Minimum fused groups | Minimum folded blocks | Worst normal p95 | Worst crossfade p95 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Split-Feedback Shimmer | 48 kHz | 5 | 1 | 2 | 1.61% | 2.95% |
| Split-Feedback Shimmer | 96 kHz | 5 | 1 | 2 | 3.63% | 5.80% |
| Reverse Cosmic Shimmer | 48 kHz | 5 | 3 | 5 | 3.23% | 7.68% |
| Reverse Cosmic Shimmer | 96 kHz | 5 | 3 | 5 | 7.12% | 13.67% |

All 20 required graph/rate/block comparisons contain a measured reduction in executed operation groups. This does not claim that every single short timing sample improved.

## Correctness evidence

The complete native/CLI suite covers released factories and representative constructed user graphs. Its qualification surface includes:

- deterministic silence, impulse, noise, program-material, automation, reset, and serialized-reload renders;
- nested/shared/multiple feedback loops, zero-delay rejection, modulation, causal Envelope Follower/Hold Gate regions, and block partition invariance;
- numerical safety, runaway recovery, capture, processed-file export, and loaded-file audition;
- host-state restoration, active latency reporting, newest-request publication, rapid edits, and two-runtime 10 ms crossfades;
- scalar/SIMD kernel equivalence, in-place alias canaries, maximum nodes/block size, fixed delay arenas, and denormal-safe finite behavior.

The editor suite validates the diagnostics contract and continued schematic semantics. The inspected Reverse Cosmic Shimmer evidence in `artifacts/ui/m17-3-compiled-kernels/` shows the unchanged 45-block/57-cable graph alongside 3 fused kernels, 5 folded blocks, buffer reuse, execution regions, and explicit fusion boundaries.

## Exit decision

M17 is accepted. Large feedback reverbs retain their visible and saved semantics; only SCC-local feedback and causal spans pay sample-wise scheduling cost; acyclic work uses block kernels; prepared buffers are reused; eligible static routing is fused; and publication remains bounded and off-thread.
