# Normalized Gravity weighting

M9.2 turns the eight depth taps from the Gravity Diffusion design into an
audible, inspectable instrument. One `Gravity` Macro fans out through eight
ordinary linear Curve Mappers. Each mapper drives the exposed `gain-mod`
socket of one tap Gain; there is no factory-only weighting processor or hidden
destination table.

## Reproducible equation

For Gravity `g` clamped to `[-1, +1]`, tap `i` uses

`w_i(g) = b_i + s_i * g`.

The exact constants, from earliest to deepest tap, are:

| Tap | Base `b_i` | Slope `s_i` | `g=-1` | `g=0` | `g=+1` |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.09 | +0.09 | 0.00 | 0.09 | 0.18 |
| 2 | 0.09 | +0.09 | 0.00 | 0.09 | 0.18 |
| 3 | 0.12 | +0.06 | 0.06 | 0.12 | 0.18 |
| 4 | 0.12 | +0.06 | 0.06 | 0.12 | 0.18 |
| 5 | 0.14 | -0.06 | 0.20 | 0.14 | 0.08 |
| 6 | 0.14 | -0.06 | 0.20 | 0.14 | 0.08 |
| 7 | 0.15 | -0.09 | 0.24 | 0.15 | 0.06 |
| 8 | 0.15 | -0.09 | 0.24 | 0.15 | 0.06 |

The bases sum to one and the slopes sum to zero, so `sum(w_i)=1` for every
Gravity value. Odd taps feed left and even taps feed right; paired constants
also keep each channel's weight sum exactly `0.5`. Every weight stays in
`0...0.24`, inside the Gain mapping clamp of `0...0.25`. This constant-L1 and
per-channel normalization is reproducible with the table alone.

The native graph builder in `GravityDiffusionGraph` is the executable source
of these constants. It emits 43 audio nodes plus one Macro and eight Curve
Mappers. Gain modulation is handled by the general parameter-mapping runtime
with the existing 1 kHz control evaluation and linear interpolation, rather
than by special Gravity code.

## Measured behavior

Reference measurements use a unit left impulse, 48 kHz, a three-second render,
the specified centered 20 ms energy smoother, and a common 700 ms comparison
horizon from causal onset. The horizon includes the first arrival from all
eight stages while separating the first and last quarters around the inverse
buildup. Results are deterministic native test evidence:

| Gravity | Time to peak | Early/late ratio | Integrated energy |
|---:|---:|---:|---:|
| -1.0 | 493.312 ms | -5.679 dB | -15.995 dB |
| -0.5 | 325.708 ms | -5.351 dB | -17.265 dB |
| 0.0 | 165.604 ms | +0.879 dB | -17.987 dB |
| +0.5 | 62.417 ms | +6.613 dB | -17.983 dB |
| +1.0 | 62.417 ms | +12.649 dB | -17.255 dB |

Early/late ratio is strictly monotonic at these five states. Time to peak is
non-increasing, with Inverse later than Bloom and Bloom later than Forward.
The raw integrated-energy spread is 1.992 dB; the regression ceiling is 2.1 dB
across the sweep. That is the measured-energy normalization tolerance for this
first untuned implementation. M9.4 may tighten it without changing the sign or
ordering contract.

The first active frame is after frame zero at 44.1, 48, and 96 kHz. Negative
Gravity therefore creates a causal rising response by weighting later arrivals;
it never reverses samples and emits no pre-input wet signal. Rapid endpoint
automation is tested on every 64-sample boundary for 2,000 blocks. Outputs must
remain finite and the largest adjacent-sample step must stay below `0.10` for a
`0.05` constant test input. Reset reproduces the center impulse bit-for-bit,
and JSON save/reload preserves the complete expanded graph.
