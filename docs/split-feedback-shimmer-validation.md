# Split-Feedback Shimmer validation

M12.2 qualifies the M12.1 graph as feedback shimmer rather than a parallel
octave layer. The deterministic Release generator writes
`artifacts/measurements/split-feedback-shimmer-v1.json`; the native test suite
regenerates the report in memory and requires exact JSON equality with that
checked artifact.

## Reproduction

Build `reverb_split_feedback_shimmer_validation_cli` in Release, then run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/generate_split_feedback_shimmer_validation.ps1 -Configuration Release
```

The report uses one channel for spectral identity so unequal stereo extraction
phase cannot cancel a valid component. Stereo behavior is measured separately
from the two physical outputs.

## Cumulative octave ascent

A continuous 400 Hz probe runs through the default forward-grain graph with
Shifted Feedback at 0.13. Hann-windowed 0.5 second regions begin at 1.0 and 2.5
seconds. Target searches are restricted to ±0.8% and the checked musical
tolerance is ±15 cents.

| Measurement | Early | Late |
|---|---:|---:|
| +12 target | 799.84 Hz / -0.35 cents | 800.00 Hz / 0.00 cents |
| +12 level | -58.92 dBFS | -55.54 dBFS |
| +24 target | 1600.32 Hz / +0.35 cents | 1600.00 Hz / 0.00 cents |
| +24 level | -116.39 dBFS | -75.91 dBFS |

The +24 component grows by 40.48 dB between the windows. In the late window it
is 20.56 dB below +12; the Safe Parallel Shimmer reference places the comparable
+24 region 68.90 dB below +12. The 48.50 dB contrast is the checked distinction
between cumulative recirculation and a one-pass parallel halo. No +36 or higher
stage is required for task acceptance.

## Independent feedback responsibilities

Changing Shifted Feedback from 0.04 to 0.13 increases the late +24 component by
20.48 dB. Separately, with Shifted Feedback held at zero, changing Normal
Feedback from 0.18 to 0.56 increases 1.5...3.0 second impulse-tail energy by
95.76 dB. The large tail ratio reflects the near-extinction of the deliberately
short low-feedback reference; it is not a claimed RT60 ratio.

## Quality disclosure

These figures describe the current dual-grain linear-interpolation quality, not
an alias-free production shifter:

- Strongest measured ±16.67 Hz forward-grain sideband: -64.20 dB relative to
  the 800 Hz target.
- A 7 kHz probe produces a folded 20 kHz component 32.17 dB below the first
  14 kHz octave component, even with the visible post-shift filter opened to
  9 kHz.
- Closing post-shift damping from 9 kHz to 2 kHz reduces late +24 energy by
  2.63 dB for the 400 Hz probe.
- Late left/right normalized correlation is 0.729: related, non-identical, and
  not a mono-compatibility guarantee for every input.

Filtering reduces unsuitable buildup but cannot remove every grain sideband or
folded component. M12.3 tuning must preserve this disclosure.

## Continuous-edit qualification

At 44.1, 48, and 96 kHz, the harness first renders an audible prepared state,
then performs 18 alternating edits. Every edit simultaneously sweeps Normal
Feedback, Shifted Feedback, Pitch (+7/+12), post-shift damping (1.6/8 kHz), and
Size (95/230 ms). Each legal graph is prepared off-thread and enters through the
existing fixed 10 ms two-runtime crossfade.

All 54 edits compile, all 54 crossfades complete, and every sample is finite.
The largest output peak is 0.01774 and the worst adjacent-sample step is 0.00151,
below the checked 0.01 click-safety bound. The test deliberately allows each
new causal runtime enough time to become audible; an all-zero run is rejected.

## UI boundary

M12.2 changes qualification code, the native graph builder, tests, and
documentation. It adds no factory entry or editor behavior. UI unchanged; no
screenshot or video is required. M12.3 owns the visible factory and interaction
evidence.
