# Reverse Cosmic Shimmer topology design

M13.2 combines existing public techniques into one project-authored graph:
causal-rise taps, a delayed damped tank, independently bounded normal and
octave returns, reverse-grain Pitch Shift, slow allpass motion, and unequal
stereo extraction. It contains no proprietary product identity, imported
impulse response, hidden processor, lookahead, random state, or factory-only
signal path.

This task creates the native graph builder and executable behavioral contract.
It does not add the factory-menu entry or teaching presentation; M13.3 owns
tuning, checked audio/measurement artifacts, UI evidence, persistence coverage,
and distribution validation.

## Signal flow

```text
stereo input -> 0.5 L + 0.5 R -> mono
  |-> 80 ms  * 0.12 --|
  |-> 240 ms * 0.25 --+-> rise diffusion -> tank entry
  `-> 520 ms * 0.50 --|

tank entry -> AP 19.7 ms -> Size delay 181 ms -> AP 31.9 ms -> Damping 4.6 kHz
  |-> Normal Feedback 0.42 -> 71 ms -------------------------------|
  `-> subtractive HP 360 Hz -> reverse Pitch Shift L/R             |
        | phase 0.000 -> LP 3.77 kHz -> 0.045 -> 89 ms ------------+-> tank entry
        ` phase 0.373 -> LP 3.50 kHz -> 0.045 -> 103 ms -----------|

damped tank -> Wet 0.62 -> AP 17.3 ms -----------------------> L sum
                         -> AP 26.9 -> AP 43.1 ms ------------> R sum
reverse voice L/R -> output-only 0.14 layers ----------------> L/R sums

LFO 0.083 Hz sine -> tank AP A + left extraction
LFO 0.061 Hz triangle, phase 0.317 -> tank AP B + right extraction stages
```

All cables are mono. Branching one audio output is explicit and permitted;
every Sum block still accepts only one cable per input. The complete default
document has 45 public blocks and 57 typed cables.

## Causal rise and harmonic timing

The three front-end delays are ordinary visible Delay blocks. Their increasing
weights move more injected energy toward 520 ms without reversing sample order.
The rise is followed by input diffusion and the tank's own Size delay before
ordinary wet output. Reverse-octave output must additionally wait for the Pitch
Shift block's fixed 600 ms history, so the octave layer cannot behave as an
immediate pitched echo or pre-input event.

A deterministic 48 kHz 400 Hz burst test compares three 300 ms windows. The
800 Hz octave component at 1.00 seconds must exceed its 0.30-second early value
by at least 4:1 and remain at least 1.5 times that early value at 1.65 seconds.
This establishes delayed, sustained harmonic evolution. M13.3 will replace
these architecture thresholds with checked spectra and audio fixtures.

The multirate impulse/noise fixture compares 0.20...0.45 seconds with
0.65...1.10 seconds. Late stereo energy must exceed early energy by at least
1.5:1 at 44.1, 48, and 96 kHz. This is a causal late-rise assertion, not a
claim that every source or control setting has one fixed perceptual peak.

## Feedback and spectral safety

All three feedback routes traverse explicit delays:

- the normal route crosses 71 ms;
- the phase-0 reverse voice crosses 89 ms;
- the phase-0.373 reverse voice crosses 103 ms.

The shared tank delay also remains in the strongly connected component. The
normal route receives the 4.6 kHz tank damping each circulation. Both shifted
routes receive that same damping, a 360 Hz subtractive high-pass before Pitch
Shift, and their own 0.82/0.76-scaled low-pass afterward. These simple one-pole
filters reduce low-frequency pitch clutter and repeatedly darken high-frequency
aliases; they do not make the linear-interpolation shifter alias-free.

Normal Feedback clamps at `0.50`. Total Shimmer Feedback clamps at `0.12` and
is divided equally between the two voices. Their absolute maximum sum is
therefore `0.62`. Qualified-rate maximum-control impulse/noise renders must
remain finite, non-silent, below full scale, and inside the prepared delay
memory budget. The existing numerical latch, emergency mute, last-valid graph,
crossfade, and explicit recovery contracts remain unchanged.

## Stereo motion and mono boundary

The two Pitch Shift blocks use deterministic phases `0.000` and `0.373`, 72 ms
grains, 68% overlap, and reverse direction. Their returns also use unequal
delays and damping. Normal wet extraction uses one moving Allpass on the left
and two different moving Allpasses on the right. Independent low-rate sine and
triangle LFOs avoid lockstep motion while remaining visible control sources.

The qualified-rate fixture requires absolute late stereo correlation below
`0.98`: related, but measurably non-identical. It also requires mono-summed
energy to remain between `0.20` and `1.80` of the normalized stereo-energy
reference, preventing decorrelation from being purchased with severe
cancellation. M13.3 will record exact correlation and mono-compatibility values
for tuned settings.

## Separately editable responsibilities

The architecture exposes responsibility through named ordinary nodes:

- **Rise / 80, 240, 520 ms** and their gains define rise shape.
- **Size** is the central tank Delay.
- **Normal feedback** controls unshifted persistence.
- **Shimmer feedback L/R** controls total octave recirculation as a bounded pair.
- **Damping** plus two named Shimmer damping filters control spectral loss.
- **Modulation L/R** are independent LFO blocks with inspectable frequency,
  waveform, and phase.
- **Wet level** and two output-only shimmer gains set presentation outside the
  feedback limits.

No one “Gravity” or “Blackhole” control secretly changes all of these. A future
instrument-style macro may map them visibly, but the underlying responsibilities
remain separately traceable and editable.
