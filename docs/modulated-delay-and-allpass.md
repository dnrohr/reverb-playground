# Modulated Delay and Allpass

M5.3 binds visible control graphs to prepared Delay time, Allpass time, and Allpass coefficient targets. It does not yet publish arbitrary edited topology into the live plugin; M5.4 owns that swap. The native graph compiler can nevertheless prepare and render the complete visible construction deterministically, including delayed feedback.

## Signal path

Control nodes run at the bounded 1 kHz rate established in M5.1. Each mapped parameter target is ramped linearly over the next control quantum at audio rate. Delay and Allpass consume those per-sample values without allocating, locking, logging, or resizing storage.

Delay time uses a circular buffer with a fractional read position and two-point linear interpolation. Integer-sample times therefore reproduce the static delay exactly. Fractional times interpolate the two adjacent older samples. The prepared line includes one guard sample beyond the largest connected mapping clamp, so both reads remain inside the allocated ring at the minimum and maximum settings.

Allpass time uses the same fractional linear read. Its coefficient is evaluated per sample and hard-limited to `[-0.95, +0.95]` before entering the recurrence. A constant mapped time and coefficient reproduce the equivalent static Allpass within floating-point tolerance.

## Rate and audible behavior

User LFOs remain limited to 100 Hz, below the 500 Hz Nyquist limit of the 1 kHz control graph. Linear audio-rate ramps remove control-tick steps, but the delay interpolator is not an anti-aliased audio-rate resampler. Moving a delay read point intentionally produces Doppler shift, pitch movement, and—at aggressive depth or rate—audible sidebands. The inspector labels that behavior rather than presenting extreme settings as transparent modulation.

For classic moving diffusion, start conservatively:

- LFO: roughly `0.1` to `2` Hz, sine or triangle.
- Scale / Offset: reduce the normalized excursion before branching.
- Allpass delay amount: roughly `0.1` to `3` milliseconds around a positive base time.
- Allpass coefficient amount: small enough that the complete mapped range remains comfortably inside `[-0.95, +0.95]`.

One mapped control output may branch to both Allpass parameter sockets or to several different diffuser blocks. Separate Scale / Offset blocks make decorrelated depth, polarity, and center values explicit.

## Safety and memory

The compiler derives Delay storage from the connected delay mapping's maximum clamp, plus the interpolation guard sample. Allpass retains its prepared maximum-time allocation. The existing 64-line and 64 MiB limits apply to the resulting allocation before runtime construction. Invalid plans never replace the last valid runtime.

Tests cover:

- static-equivalent constant control;
- fractional interpolation at minimum and maximum time;
- full-depth sweeps and extreme coefficient inputs;
- deterministic modulation across host block partitions;
- a visible LFO -> Scale / Offset -> Allpass construction with one control output branching to time and coefficient;
- modulated Delay inside a feedback loop.

## Milestone boundary

The standalone editor visualizes the same construction and accurately explains its interpolation and audible consequences. The current fixed Barr live runtime remains unchanged; selecting and publishing an arbitrary edited graph at an audio block boundary is M5.4.

## Evidence

- Native inspector showing the Delay mapping, fractional-tap policy, Doppler/pitch disclosure, 100 Hz source limit, and adjacent bounded coefficient socket: [`01-allpass-modulation-policy.png`](../artifacts/ui/m5-3-modulated-delay-allpass/01-allpass-modulation-policy.png).
- No new video is required for M5.3: the only new visible behavior is static policy text. M5.2 already records the control animation; deterministic native renders prove the new audio-rate path.
