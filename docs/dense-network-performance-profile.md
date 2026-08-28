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
