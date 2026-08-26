# Pitch Shift inner-loop optimization

Milestone 18.1 keeps the released `dual-grain-linear-v1` signal model and its
public -12 to +12 semitone range while reducing work inside each audio sample.
The implementation remains portable scalar C++ and performs no allocation,
locking, I/O, or mutable-table access while processing.

## Prepared work

The processor now prepares the pitch ratio, grain length, phase increment, and
window-oscillator rotation coefficients. A static block therefore performs no
per-sample `pow`, `sin`, `floor`, or general modulo calls. Positive bounded
delay positions use defined floating-to-integer truncation, ring indices and
write positions use bounded conditional wraps, and the two-head window advances
with a sine/cosine recurrence. Equal-power overlap gains use a bounded polynomial
on `[0, pi/2]`; endpoints and the existing output headroom remain unchanged.

Semitone edits still take 20 ms. Their ratio follows the same exponential
mapping using one prepared multiplier per edit. Grain length, overlap,
direction, and phase edits retain the existing two-state 20 ms crossfade.

## Release measurement

The checked [baseline](../artifacts/measurements/pitch-shift-m18-1-baseline.json)
was produced from exact commit `d0934e6e2d82c13d07b75fdc8d77fc480b29b285`
before the inner-loop change. The checked
[optimized report](../artifacts/measurements/pitch-shift-m18-1-optimized.json)
uses the same Release executable and workstation. One-second 256-frame steady
fixtures measured as follows; these are same-machine trends, not portable CPU
guarantees.

| Rate | Baseline forward / reverse | Optimized forward / reverse | Reduction range |
|---:|---:|---:|---:|
| 44.1 kHz | 2,084 / 1,930 us | 827 / 837 us | 56.6-60.3% |
| 48 kHz | 2,391 / 2,232 us | 951 / 921 us | 58.7-60.2% |
| 96 kHz | 4,539 / 4,431 us | 1,692 / 1,902 us | 57.1-62.7% |

The canonical report additionally separates steady state, a parameter edit on
every block, two voices, and a two-processor topology crossfade at every
qualified rate. At 96 kHz those cases measured 0.20%, 0.41%, 0.41%, and 0.42%
of one real-time core respectively.

## Qualification boundary

Existing tests remain authoritative for octave accuracy, causal latency,
storage canaries, bounded output, deterministic reset, reverse-grain stereo
decorrelation, transient-envelope distinction, alias disclosure, delayed
feedback containment, topology crossfade, and safety recovery. The refreshed
[canonical validation](../artifacts/measurements/pitch-shift-validation-v1.json)
records all four benchmark modes beside those quality measurements.

This task does not change the visible editor, saved graph schema, latency,
interpolation mode, or quality policy. It also does not optimize the runtime's
one-sample control dispatch; that is M18.2.
