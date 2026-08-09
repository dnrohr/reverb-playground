# DSP primitives

The first engine vocabulary is deliberately small and mono. Stereo behavior is assembled explicitly in the graph from mono ports and branches.

| Primitive | Contract |
|---|---|
| `Gain` | Multiplies a span by a signed linear value; a negative value is the explicit polarity inversion operation. Runtime target edits use a 20 ms linear ramp. |
| `Sum` | Adds two mono spans into a caller-owned output span; unmatched output samples become silence. |
| `Delay` | Integer-sample delay prepared from milliseconds and sample rate, rounded to the nearest sample with a one-sample minimum. |
| `Allpass` | Schroeder first-order delay allpass, `y[n] = d[n] - g*x[n]`, with `d <- x[n] + g*y[n]`; supports the Barr-favored `g = 0.5` and general stable `abs(g) < 1`. Runtime coefficient edits smooth over 20 ms and delay edits crossfade fixed-buffer read taps over 20 ms. |
| `OnePoleLowPass` | One-pole smoothing filter with cutoff strictly between zero and Nyquist; runtime cutoff edits smooth its coefficients over 20 ms. |

## Lifetime and real-time behavior

`prepare` validates configuration and owns every allocation. It is a control-thread operation. Standalone primitives may own their buffers; compiled Delay and Allpass nodes instead receive slices of the runtime's single [prepared delay arena](delay-memory-planning.md). `reset` and `process` are `noexcept`, bounded by the supplied span, and perform no allocation, locking, I/O, or topology work. Delay and allpass return silence if processed before preparation. Reset clears all delay/filter state and makes subsequent silent input deterministically silent.

Delay also exposes split-phase `readSample` and `writeSample` operations for compiled feedback. Reading never advances state; writing stores the completed current input and advances exactly once. Block `process` uses the same pair internally, so feed-forward and feedback timing remain identical.

Times presented or serialized by the product remain milliseconds. Integer delay conversion is deliberately defined here so offline and real-time renders agree at every sample rate. The editable allpass preallocates its declared maximum delay and crossfades integer read taps; fractional and continuously modulated delay interpolation is later M5 scope.

## Test tolerances

- Delay impulses are checked at 44.1 and 96 kHz at the exact rounded sample.
- Allpass impulse energy and sampled response magnitudes from DC through `0.9 * Nyquist` are within `1e-5` of unity over 8192 samples for coefficients `0.5` and `-0.73`.
- Sum and negative polarity use exact signed reference vectors.
- Low-pass step response must be positive, monotonic, bounded below unity over the test window, and silent after reset.
