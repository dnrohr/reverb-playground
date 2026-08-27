# Compiled control-ramp processing

Milestone 18.2 keeps the fixed 1 kHz control graph and its sample-accurate
linear ramps, but changes how prepared audio processors consume those ramps.

## Prepared execution

Control evaluation now fills bounded ramp segments between tick boundaries.
Each prepared modulation owns its maximum-block-size view; no storage is
allocated during processing. Static graphs still skip modulation generation
entirely.

Block-wise regions pass those views once to explicit kernels:

- Gain multiplies the prepared gain view directly.
- Delay consumes the prepared fractional-delay view.
- Allpass consumes optional delay and coefficient views, with constants for
  unmodulated parameters.
- Low-pass consumes the cutoff view without setter-plus-one-sample span calls.
- Pitch Shift consumes optional semitone, grain, and overlap views while
  retaining its internal 20 ms click-safe transitions.

Causal feedback regions cannot be reordered into whole blocks. They now use
explicit single-sample Delay, Low-pass, Allpass, and Pitch Shift kernels rather
than composing public setters with one-sample spans. Envelope Follower and Hold
Gate remain causal sample processors by design.

For a block-wise modulated node the prepared runtime dispatch count changes
from one setter/process pair per sample to one kernel call per block. At a
256-frame block that removes 255 runtime dispatches per parameterized processor;
the processor still performs the necessary sample arithmetic internally.

## Equivalence and measurement

Native equivalence tests compare the new block kernels with the former
per-sample path for swept Low-pass, Allpass, and Pitch Shift inputs. Existing
tests cover control-tick interpolation, block-partition determinism, macro
smoothing, LFO run modes, Curve Mapper behavior, automation, feedback, and host
state.

The checked [baseline matrix](../artifacts/measurements/performance-matrix-m18-2-baseline.json)
uses exact M18.1 commit `f62ae8ec4fedb50c9feb376c152430017e5bceef`.
The checked [optimized matrix](../artifacts/measurements/performance-matrix-m18-2.json)
was run immediately afterward on the same workstation with 1,000 normal
callbacks per case. At the representative 256-frame block size, normal p95 load
fell 3.8%/4.7% for Gravity Diffusion at 48/96 kHz, 4.9% for Safe Parallel
Shimmer at 96 kHz, and 3.5% for Reverse Cosmic Shimmer at 96 kHz. Other
individual cases moved within roughly +/-4%; this small dispatch optimization
is not presented as a universal CPU reduction. Every one of the 75 normal and
crossfade cases remains finite and inside the published callback budgets.

No editor, schema, saved-value, latency, control-rate, or quality-policy change
is introduced here, so no new UI evidence is required.
