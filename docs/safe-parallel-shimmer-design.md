# Safe Parallel Shimmer topology design

M11.1 defines a stable octave halo assembled entirely from ordinary public
blocks. It is deliberately not classic feedback shimmer: the pitch-shifted
signal never returns to the tank, so one sample can cross the Pitch Shift block
at most once. M11.2 will package and teach this graph; this task establishes the
audible architecture and its testable budgets.

## Signal path

The stereo input is reduced to one deterministic mono cable as
`0.5 * left + 0.5 * right`, then passes through 4.7 and 8.9 ms input Allpasses.
The tank is a visible delayed feedback loop:

`Sum -> Allpass 13.7 ms -> Delay 149 ms -> Allpass 23.9 ms -> Low-pass 6.5 kHz -> Gain 0.55 -> Delay 61 ms -> Sum`

The damped tank output splits after the loop:

- **Normal branch:** Delay 360.01 ms, then Gain 0.50.
- **Octave branch:** a public-block high-pass approximation
  `x + (-1 * Low-pass(x, 250 Hz))`, Pitch Shift +12 semitones with a 60 ms
  forward grain and 0.5 overlap, Low-pass 6 kHz, Allpasses 17.3 and 29.1 ms,
  then Gain 0.22.
- **Recombination:** Sum, then a separate Wet Balance Gain of 0.75.
- **Stereo extraction:** unequal 11.9 ms left and 19.7 ms right Allpasses feed
  the two Stereo Output sockets. Every cable between blocks remains mono.

The low-frequency subtraction before shifting and the post-shift low-pass are
intentional responses to M10.4's measured folded aliasing. They reduce unsuitable
energy but do not make the current linear-interpolation shifter alias-free.

## Responsibility boundaries

`reverb-decay`, `shimmer-level`, `shimmer-damping`, and `wet-balance` are four
different visible nodes. Reverb Decay changes only the tank return. Shimmer
Level changes only the post-shift branch. Shimmer Damping changes only the
post-shift cutoff. Wet Balance changes the complete recombined wet signal. The
builder bounds their construction-time values to 0...0.72, 0...0.30,
800...12,000 Hz, and 0...0.80 respectively.

## Structural non-recirculation proof

The only strongly connected component contains `tank-entry`, the two tank
Allpasses, tank Delay, tank Low-pass, Reverb Decay, and Feedback Delay. There is
no directed route from `shimmer-pitch` or `shimmer-level` back to `tank-entry`.
Consequently the topology can create one +12-semitone parallel image, but it
cannot create +24, +36, or later octaves by repeated traversal. M12 intentionally
introduces a separately bounded shifted return for that different behavior.

## Latency, memory, and loudness budgets

Pitch Shift reports `ceil(600 ms * sampleRate) + 2` samples. The normal branch's
360.01 ms Delay differs by no more than two samples at 44.1, 48, 96, and 192 kHz;
this avoids presenting the unshifted branch roughly 600 ms before its halo.

Compilation at the maximum supported 192 kHz must remain below 4 MiB of prepared
delay storage, well inside the project-wide 64 MiB limit. The graph has twelve
planned delay-bearing processors, including Pitch Shift. No allocation occurs
during audio processing.

The input sum is normalized by two 0.5 gains. Nominal post-tank branch gains are
0.50 normal and 0.22 shimmer; the final wet gain is 0.75. The allowed maxima are
0.50, 0.30, and 0.80. These are conservative operating coefficients rather than
a mathematical peak guarantee for correlated feedback signals, so the existing
runtime numerical guard remains authoritative. Two-second impulse-plus-bounded-
noise renders must remain finite, below full scale, non-silent, and stereo-
different at 44.1, 48, and 96 kHz.

## Milestone boundary

M11.1 adds the reusable native graph builder and deterministic architecture
tests. It changes no editor surface, factory menu, or packaged binary behavior;
therefore no screenshot or video is required. M11.2 owns factory admission,
visible teaching labels, spectral octave/no-staircase measurements, host-state
coverage, and current UI evidence.
