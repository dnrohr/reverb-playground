# Compiled buffer liveness and reuse

Status: implemented in M17.2

The visible schematic remains the program. During off-thread compilation, the runtime now maps its logical signals onto a smaller deterministic set of prepared audio buffers. This changes storage and copying only; it does not fuse, remove, or hide blocks.

## Allocation rules

The compiler records each audio signal's producer, last consumer, and fan-out. A single-consumer, in-place-safe processor can reuse its first input buffer when that value dies at the processor. Otherwise, a new signal may reuse an expired physical buffer. Gain, Sum, Delay, All-pass, Low-pass, and Pitch Shift have explicit in-place-safe rules. General reuse is deterministic because the compiler considers prepared operations and physical slots in stable order.

Control-only Macro, LFO, and Curve Mapper outputs no longer reserve block-sized audio storage. Envelope Follower output remains an audio-rate control signal because a dependent Hold Gate can consume it in the same sample-wise causal region.

The following boundaries retain independent storage:

- silence and stereo runtime inputs;
- signals feeding the graph outputs;
- every value inside feedback or causal sample-wise regions;
- branched values until their final consumer;
- runtime-boundary, capture, inspector, or telemetry-observed values when present.

Delay-line arenas and host crossfade scratch live outside this allocator. The compiler never aliases them with graph work buffers.

## Diagnostics

The diagnostics drawer reports logical audio buffers, all logical signals, physical audio buffers, peak live buffers, saved bytes, in-place aliases, avoided copies, and retention reasons. These are prepared-plan facts, not estimates of elapsed CPU time. A retention reason with zero signals is still shown so the protection contract remains explicit.

`copiesAvoided` counts only first-input copies that the executor actually skips because input and output share a physical buffer.

## Measurement

The Release matrix in `artifacts/measurements/performance-matrix-m17-2.json` covers all 75 flagship/rate/block combinations. At 48 kHz and a 512-sample block:

| Graph | Logical signals | Physical buffers | Saved bytes | Avoided copies | Prepared bytes before | Prepared bytes after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Barr Reference | 11 | 6 | 10,240 | 5 | 137,752 | 127,512 |
| Gravity Diffusion | 53 | 39 | 28,672 | 5 | 434,416 | 405,744 |
| Safe Parallel Shimmer | 29 | 24 | 10,240 | 4 | 460,720 | 450,480 |
| Split-Feedback Shimmer | 26 | 23 | 6,144 | 3 | 364,136 | 357,992 |
| Reverse Cosmic Shimmer | 46 | 35 | 22,528 | 8 | 751,676 | 729,148 |

Every case remained finite and within the existing normal and topology-crossfade budgets. Prepared storage decreased by exactly the reported saved buffer bytes relative to M17.1. Delay storage is reported separately and the existing exact delay-plan tests remain unchanged.

## Safety coverage

Native fixtures cover fan-out retention, exact constructed-graph output, a 252-node maximum linear graph at the 2,048-sample maximum block, output canaries, repeated reset, rapid publication, and two-runtime crossfades. Existing feedback, causal-control, golden-render, and delay-memory suites protect sample ordering and delay arena ownership.
