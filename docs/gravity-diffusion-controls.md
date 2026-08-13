# Gravity Diffusion complementary controls

M9.3 adds the minimum controls needed to turn the normalized Gravity network
into an instrument without making Gravity responsible for unrelated behavior.
The complete builder overload emits five named Macros, two independent LFOs,
eight Gravity Curve Mappers, and 35 ordinary parameter mappings. Every cable,
base value, amount, polarity, and clamp remains part of the schema-v2 graph.

The original `makeGravityDiffusionGraph(gravity)` overload remains the static
M9.2 weighting reference. `makeGravityDiffusionGraph(controls)` adds the
complementary M9.3 paths; this keeps the versioned weighting measurements
reproducible while allowing instrument tests to exercise the moving network.

## Responsibility table

| Macro | Visible destinations | Exact mapping | Responsibility |
|---|---|---|---|
| Gravity | Eight Curve Mappers to eight tap Gains | [Normalized Gravity weighting](normalized-gravity-weighting.md) | Move energy between early and deep taps. |
| Size | Four input-Allpass delays, eight stage Delays, and the 97 ms feedback Delay | `delay = base * (1 + 0.35 * Size)`, clamped to `0.65...1.35` times base | Scale causal arrival times and density buildup without changing tap-weight sign. |
| Feedback | Return Gain | `gain = clamp(0.58 + 0.23 * Feedback, 0.35, 0.81)` | Change recirculation and tail energy independently of Gravity. |
| Damping | Return Low-pass cutoff | `cutoff = clamp(5800 - 4200 * Damping, 1600, 10000)` Hz | Reduce repeated high-frequency energy as Damping increases. |
| Modulation | Coefficients of the four input Allpasses | `coefficient = clamp(0.50 + 0.18 * Modulation, 0.32, 0.68)` | Change diffusion strength while the independent motion paths remain audible. |

Size deliberately does not connect to the eight stage-Allpass delay sockets.
Those sockets are exclusively owned by the LFO paths, avoiding an undisclosed
control sum or multiply. Motion A is a `0.11 Hz` sine at phase zero and moves
odd stages 1/3/5/7. Motion B is a `0.073 Hz` triangle at phase `0.25` cycles and
moves even stages 2/4/6/8. Each mapping is bipolar `+/-1.25 ms` and clamps to
the stage's base delay plus or minus that amount. The different rates,
waveforms, phases, and stage sets make the paths independent and inspectable.

## Runtime boundary

M9.3 extends the reusable audio runtime in two places. Ordinary control
mappings can now drive Gain and Low-pass cutoff in both acyclic and delayed
feedback schedules. They use the existing prepared mapping arrays, 1 kHz
control evaluation, and linear interpolation; no allocation, lock, logging, or
graph compilation occurs in `process`. Delay and Allpass modulation retain the
existing fractional-delay implementation.

Low-pass cutoff changes call the processor's bounded, smoothed coefficient
target. Gain values use the already-interpolated mapping vector directly. The
complete graph has 58 nodes, 94 cables, 15 participating control nodes, and 35
parameter mappings, below the project limits of 256 nodes, 512 cables, 64
control nodes, and 128 mappings.

## Safety and acceptance evidence

Deterministic native tests exercise all 32 combinations of the five Macro
endpoints. Each runtime receives silence, a unit impulse, and full-scale
bounded deterministic noise. Both channels must remain finite and pass the
existing numerical safety guard without latching; explicit guard reset is
also checked. A separate 4,000-block run continuously sweeps all five Macros
with bounded noise and verifies that every runtime-only update is accepted and
finite without topology recompilation.

Size endpoint renders require later causal onset at `+1` than `-1` and retain
`Inverse time-to-peak > Forward time-to-peak` at both sizes. Feedback `+1`
must add at least 1 dB of full three-second wet energy over `-1`. Damping `+1`
must reduce both wet energy and the post-one-second squared first-difference
high-frequency proxy by at least 20 percent relative to `-1`. Modulation endpoint renders must have
finite energy and measurably different stereo samples.

The safety latch, emergency mute, Undo, and explicit recovery code paths are
shared project infrastructure rather than factory-specific alternatives. The
full verification suite reruns their native and browser regressions alongside
the extreme instrument tests. M9.3 changes no editor component or style, so no
new UI capture is required by the project's evidence policy.
