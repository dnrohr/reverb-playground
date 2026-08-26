# Graph runtime diagnostics

Status: implemented in M16.3

## What the drawer reports

Constructed-graph diagnostics now come from the active prepared plan rather than the fixed Barr Reference estimate.

- **Work / plan estimate** is a deterministic weighted scalar-operation estimate for the exact node families in the active plan. It is useful for relative complexity and regression triage, not a CPU benchmark.
- **Live / measured** is aggregate audio-callback load and peak load. Production processing takes one block-level clock sample; it never times individual nodes.
- **Memory / prepared** is the runtime's prepared audio-buffer plus delay allocation. Delay bytes and line count remain visible separately.
- **Latency / compiled** is audible prepared sample delay, not compute time.
- **Graph / prepared** gives node, cable, and feedback-region counts for the active revision.
- **Compile timing** separates validation, causal scheduling/analysis, runtime preparation, total compile time, and request-to-active publication time.

The offline plan profile ranks processor families by estimated operations per sample and labels the current executor domain as `block-wise` or `sample-wise`. The current executor makes an entire graph sample-wise when it contains feedback or causal Envelope Follower/Hold Gate work; M17 will use this baseline to partition only the required regions.

## Static cost model

Weights intentionally describe approximate scalar work, not instruction counts: Gain 2, Sum 2, Delay 5, Allpass 9, Low-pass 6, Pitch Shift 48, Envelope Follower 8, and Hold Gate 7 operations per sample. LFO and Curve Mapper contribute their small normalized control cost. A sample-wise plan adds two dispatch operations per audio processor. The model is compiled off-thread from the actual node inventory and processor mode, so adding or removing an expensive family changes the estimate deterministically.

M16.4 measures real Release callback distributions. Those measurements must not be inferred from this cost model or compared across machines as if the weights were elapsed time.

## Compilation and publication

All validation, schedule construction, family profiling, allocation, and preparation happen on the compiler or calling non-audio thread. The prepared plan is immutable when queued. On activation the audio callback publishes scalar revision, memory, latency, and request-to-active counters without allocation, locks, logging, or per-node clocks.

Newest-request-wins behavior remains authoritative. A request replaced before compilation increments the superseded-request count. Work that completed compilation but became stale increments the separate superseded-compilation count and records its compile duration. Neither stale result can become audible. Active details remain keyed to the active revision while bounded historical entries are reclaimed off-thread.

## Verification

Native tests prove graph-specific family totals, block-wise versus sample-wise labeling, exact active graph counts and memory, separated phase timing, positive request-to-active publication, compiled-work supersession, and unchanged crossfade/newest-request behavior. The web contract rejects malformed families, domains, timings, and counters.

Current standalone evidence is stored in `artifacts/ui/m16-3-plan-diagnostics/`: the summary capture shows graph-specific work, memory, latency, and topology cards; the scrolled capture shows the ranked offline family profile and the explanatory basis/compensation copy.
