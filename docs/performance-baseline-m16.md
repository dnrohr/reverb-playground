# M16 performance baseline

Status: complete in M16.4

This is the pre-optimization reference for M17 and M18. The checked-in [machine-readable report](../artifacts/measurements/performance-matrix-v1.json) covers all 75 combinations of five flagship graphs, 44.1/48/96 kHz, and 32/64/128/256/512-sample blocks.

## Method

`scripts/generate_performance_matrix.ps1` builds the headless runner in Release mode and processes deterministic LCG noise plus a 311 Hz sine. Input preparation and graph compilation are outside callback timing. Each case measures 200 normal callbacks, then 20 separate 10 ms two-runtime topology transitions. Callback deadline misses are reported as benchmark `underruns`; they are not hardware-driver counters.

Run from the repository root:

```powershell
.\scripts\generate_performance_matrix.ps1
```

The report includes sample counts, median/95th-percentile/peak callback time and deadline load, transition overhead, split compile and request-to-active time, latency, prepared memory, graph size, feedback regions, estimated operations, and execution domain. Results are meaningful for before/after comparisons on the same machine and toolchain, not as cross-machine product claims.

## Reference result

- Machine: Intel64 Family 6 Model 154 Stepping 3, GenuineIntel; 20 logical threads
- Toolchain: MSVC 19.44, Release
- Source revision: `b971cc010235` plus the M16.4 benchmark changes
- Cases: 75; all outputs finite; zero measured callback deadline misses
- Worst normal p95 deadline load: Reverse Cosmic Shimmer, 7.251% at 96 kHz / 512 samples
- Worst transition p95 deadline load: Gravity Diffusion, 13.192% at 96 kHz / 128 samples
- Maximum compile time: 861 us; maximum request-to-active time: 889 us
- Maximum prepared-plan memory: 1,409,084 bytes

Every individual case passes the release budgets; this is not inferred from an aggregate average.

## Budgets and regression gates

The per-case release safety budgets are normal p95 callback load at or below 80%, no normal deadline misses, and topology-transition p95 load at or below 160%. These deliberately leave scheduling headroom and allow the short two-runtime transition to cost more than steady state. Any failure must name the graph/rate/block combination.

M17 must preserve finite output, zero normal misses, the safety budgets, and graph semantics. Reverse Cosmic Shimmer and Split-Feedback Shimmer must improve normal p95 callback time or measured operation/copy count at both 48 and 96 kHz. Compile, request-to-active, prepared memory, and transition results may not regress by more than 10% without an explicit measured rationale.

M18 uses the same safety and 10% regression gates. Pitch-bearing graphs must improve normal p95 callback time by at least 15% at both 48 and 96 kHz before an optimization is accepted; quality modes are compared independently at matching settings. A specialized AOT or JIT path is justified only after the portable executor and pitch/control work, with equivalent renders and a material additional gain.

Absolute microsecond values can vary with power state and background activity. Qualification therefore repeats the full matrix on the same development machine and compares per-case distributions, not isolated peaks.
