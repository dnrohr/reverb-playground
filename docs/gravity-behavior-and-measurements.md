# Gravity behavior and measurements

M8.1 defines the public contract for **Gravity**, a single normalized control
that changes where a reverb network concentrates energy over time. Gravity is
an envelope-shape coordinate, not a hidden reverb algorithm and not a synonym
for decay time.

## Control contract

| Property | Contract |
|---|---|
| Automation range | Normalized, unitless, inclusive `-1.0...+1.0` |
| Sign convention | Negative is **INVERSE**; positive is **FORWARD** |
| Center | `0.0`, labeled **BLOOM**, with a reliable center detent |
| Default and reset | Exactly `0.0` |
| Display | Signed value with two decimal places; endpoints read `INVERSE` and `FORWARD` |
| Host mapping | `-1.0` maps to host-normalized `0.0`, `0.0` to `0.5`, and `+1.0` to `1.0` |

At negative values, progressively deeper stages of the visible diffusion
network receive progressively greater weight, moving the response's energy
peak later. At positive values, earlier stages receive progressively greater
weight, concentrating energy near the onset. At zero, stage weights form the
factory's clustered, slowly blooming reference distribution. The mapping must
be continuous across zero and must not switch topology or processing mode.

Gravity does not directly set network delay lengths, feedback gain, filter
cutoffs, modulation rate, or modulation depth. Those remain the distinct
**Size**, **Feedback/Decay**, **Damping**, and **Modulation** controls. A factory
patch may map Gravity to multiple visible stage gains or diffusion parameters,
but every destination and curve remains inspectable.

## What inverse means here

Negative Gravity produces a **causal inverse-decay shape**: a new response whose
smoothed energy generally grows toward a late peak after the input arrives. It
does not reorder samples. Specifically, it is not:

- sample reversal, where a finite signal or impulse response is read backward;
- reverse-grain processing, where overlapping buffered grains are read backward;
- offline pre-reverb, where a reversed wet render is placed before the dry event;
- undisclosed lookahead or a wet signal that precedes its causal input.

For every Gravity value, the first possible wet output frame must be no earlier
than the first causal arrival permitted by the visible graph. A negative state
must never emit wet energy before its input arrives. The distinction follows the
project-wide vocabulary in [Reverse, inverse, gated, and Bloom architecture
requirements](reverse-and-gated-architecture-requirements.md).

## Deterministic measurement contract

Measurements operate on a finite stereo impulse response `L[n], R[n]` rendered
at a declared sample rate `Fs`. Unless a fixture states otherwise, use a unit
impulse at frame zero, 100% wet output, a 48 kHz sample rate, and a render long
enough to satisfy the existing tail/noise-floor refusal rule. Define mono energy
per frame as:

`e[n] = 0.5 * (L[n]^2 + R[n]^2)`.

Define smoothed energy `E[n]` as the arithmetic mean of `e` over a centered
20 ms rectangular window. At either buffer edge, the window is clipped to
available frames and divided by its actual frame count. Ties choose the earliest
frame. These rules make every metric below deterministic across implementations.

| Metric | Deterministic definition |
|---|---|
| `timeToPeakMs` | `1000 * n_peak / Fs`, where `n_peak` is the earliest frame containing the maximum `E[n]`. |
| `earlyLateEnergyRatioDb` | `10 * log10((sum early e + eps) / (sum late e + eps))`, where early is `[onset, onset + 0.25T)` and late is `[onset + 0.75T, onset + T)`; `T` is the declared comparison horizon and `eps = 1e-20`. |
| `peakLevelDbfs` | `20 * log10(max_n(max(abs(L[n]), abs(R[n]))))`; silence is reported as null, not negative infinity. |
| `integratedEnergyDb` | `10 * log10(sum_n e[n] + eps)` over the declared comparison horizon. It is relative energy for identical unit-impulse conditions, not loudness. |
| `rt60Seconds` | The existing Schroeder/T30 estimate from [Response measurements](response-measurements.md), including all refusal rules. |
| `decaySlopeDbPerSecond` | Least-squares slope over the same valid `-5...-35 dB` Schroeder region used for RT60; null whenever RT60 is refused. |
| `postPeakEnergyFraction` | `sum_(n > n_peak) e[n] / sum_n e[n]`; null for silence. This distinguishes an abrupt terminal rise from a bloom that continues into a tail. |

`onset` uses the existing active-threshold policy. The comparison horizon `T`
must be identical for every state in one reference set and recorded with the
results. Metrics must be computed from unsmoothed `e[n]` except for locating the
smoothed peak. If a response is truncated or does not span the required decay
range, RT60 and slope are null rather than guessed.

Changing weights can change output level, so shape comparisons report both raw
and energy-normalized results. Normalization uses one scalar per rendered state
to match the center state's integrated energy; it is analysis-only and never
part of the audible graph. Peak level and integrated energy before normalization
remain mandatory safety/tuning results.

## Reference targets

The first Gravity Diffusion patch must supply versioned measurements for these
three fixed targets. They are behavioral acceptance regions, not claims about a
particular hidden implementation.

| Target | Gravity | Required measured relationship |
|---|---:|---|
| Inverse rise | `-1.0` | Latest `timeToPeakMs`; negative `earlyLateEnergyRatioDb`; causal onset; nonzero post-peak tail. |
| Clustered/Bloom center | `0.0` | Peak later than Forward and earlier than Inverse; nonzero post-peak tail; the factory default/reset state. |
| Forward decay | `+1.0` | Earliest `timeToPeakMs`; positive `earlyLateEnergyRatioDb`; a valid decaying slope when the render supports it. |

Across the set, raw peak level must remain below the project safety ceiling and
all samples and metrics must be finite. Integrated energy is reported and tuned
within the M9 fixture tolerance so the control does not masquerade primarily as
a volume knob. M9 tuning will set numeric time-to-peak and energy tolerances
after the eight-stage topology exists; it may tighten but not reverse the
ordering above.

## Inspiration and originality boundary

Eventide publicly describes Blackhole's Gravity control as moving between
inverse-style settings on the left and forward decay on the right, and describes
it as Blackhole's equivalent of decay time. That behavior inspired the bipolar
gesture and endpoint language: [Eventide H90 Blackhole documentation](https://cdn.eventideaudio.com/manuals/h90/1.1.2/content/algorithms/reverb.html)
and [Eventide Blackhole product page](https://www.eventideaudio.com/plug-ins/blackhole/).

Those public descriptions do not disclose a topology and are not evidence for
one. Reverb Playground's planned eight-stage diffusion graph, normalized stage
weighting, mappings, measurements, and UI are original project work. The graph
will remain expanded into public primitives rather than claiming to reproduce
Eventide's algorithm.
