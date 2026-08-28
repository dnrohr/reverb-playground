# Dense-network performance profile

M23.1 establishes the pre-kernel baseline for the Dense Figure Eight and Four-Line Dense Room. The checked report is [`artifacts/measurements/dense-network-profile-m23-1.json`](../artifacts/measurements/dense-network-profile-m23-1.json). It was produced by the Release build at commit `69660a4832bf` on the recorded Intel machine with MSVC 19.44.

## What is timed

Each graph is measured for 1,000 normal callbacks at 44.1, 48, and 96 kHz and block sizes 32, 64, 128, 256, and 512. The same cases independently time Energy-enabled callbacks and twenty topology crossfades. Source generation and graph preparation are outside callback timing. All 30 cases are finite, have no measured underruns, and remain inside the declared normal callback budget.

Processor-family values are deliberately described as **attributions**, not independent stopwatch measurements. Normal measured callback time is apportioned using the prepared plan's scalar-work model plus explicit routing units. This avoids inserting thousands of timer reads into sample-wise feedback loops, which would overwhelm the work being measured. Telemetry and crossfade overhead are independently timed.

## Result

The Four-Line FDN is the optimization target. At 48 kHz / 128 samples it measures 145.5 us median and 156.6 us p95 (5.87% of the callback deadline). Its attributed p95 composition is:

| Family | Share | Attributed p95 |
| --- | ---: | ---: |
| Routing and sample-wise dispatch | 49.05% | 76.81 us |
| Delay/allpass work | 24.33% | 38.11 us |
| Matrix gain/sum work | 21.67% | 33.94 us |
| Damping filters | 4.56% | 7.15 us |
| Control modulation | 0.38% | 0.60 us |

Across the supported matrix, Four-Line FDN p95 load ranges from 5.37% to 13.19%. Energy adds 3.7% median cost on average; topology crossfade averages 1.89x normal cost. Dense Figure Eight is delay-dominant and ranges from 1.79% to 4.67% p95 load.

## M23.2 decision

The first retained kernel should attack the Four-Line FDN's sample-wise routing and matrix together. A four-lane fused read/damp/gain/Hadamard/write pass can remove generic per-node dispatch and intermediate routing while keeping the visible 4x4 matrix authoritative. A standalone delay-only optimization is secondary: it cannot address the measured dominant family by itself. SIMD or a specialized path will be retained only if the same-machine target fixture improves by at least 10–15%, remains sample-equivalent within the declared tolerance, and preserves the generic fallback.

## M23.2 result

The retained implementation extends the prepared executor's existing static gain/sum/filter fusions into sample-wise feedback and causal regions. It is topology-generic: eligibility comes from signal dependency, fan-out, modulation, and region boundaries rather than a factory ID. The four-line graph prepares ten feedback-region fused kernels; Dense Figure Eight prepares two. Unsupported, modulated, tapped, or cross-region arrangements continue through the generic operation path.

The exact-commit optimized report is [`artifacts/measurements/dense-network-profile-m23-2.json`](../artifacts/measurements/dense-network-profile-m23-2.json), produced at `5e19c7850d08` with 2,000 callbacks per case. Compared with M23.1 on the same machine:

| Graph | Mean median improvement | Worst median improvement | Mean p95 improvement | Worst p95 improvement |
| --- | ---: | ---: | ---: | ---: |
| Four-Line FDN | 37.42% | 34.24% | 39.05% | 31.53% |
| Dense Figure Eight | 25.17% | 21.43% | 26.99% | 22.58% |

Every one of the 30 rate/block cases clears the 15% median and p95 retention gate. A paired optimized/generic render test drives 128,000 deterministic stereo samples through both prepared paths and bounds the maximum sample error below `1e-5`. Existing fixed-arena, callback-allocation, feedback stability, partition, reset, telemetry, and crossfade tests remain applicable because the kernel owns no new storage and performs no allocation or locking while processing.
