# Response measurements

M1.4 measures a rendered stereo impulse response automatically and writes the canonical Barr reference result to [`artifacts/measurements/barr-reference-v1.json`](../artifacts/measurements/barr-reference-v1.json). The artifact identifies measurement format version 1, engine version 0.1, patch ID, render length, and sample rate.

Generate it with the same headless renderer used by golden tests:

```powershell
build/windows-msvc/src/render/Debug/reverb_render_cli.exe `
  --input impulse --sample-rate 48000 --duration-ms 2000 `
  --output build/barr-reference-ir.wav `
  --measurements artifacts/measurements/barr-reference-v1.json
```

## Metric definitions

| Metric | Kind | Definition |
|---|---|---|
| `onsetFrame` | Exact under threshold policy | First frame where either channel's absolute sample exceeds `activeThreshold`. Null for silence. |
| `lastActiveFrame` | Exact under threshold policy | Last frame exceeding the same threshold. Null for silence. |
| `impulseLengthFrames` | Exact under threshold policy | Inclusive distance from onset through last active frame. This is a thresholded observed length, not proof that an IIR has mathematically ended. |
| `peakLeft`, `peakRight` | Exact for rendered samples | Maximum absolute floating-point sample in each channel. |
| `stereoDifferenceRms` | Exact for rendered samples | RMS of `left - right` across the complete render. Zero means identical channels; larger values indicate more channel difference, not perceptual width. |
| `decayCurve` | Derived | Backward Schroeder integration of summed stereo energy, normalized to frame-zero energy and expressed in dB. It is decimated to at most roughly 257 points for a compact artifact. Null dB represents zero remaining energy. |
| `rt60Seconds` | Estimated | Linear regression over the Schroeder curve from -5 through -35 dB, extrapolated to -60 dB. |

## RT60 refusal rules

RT60 is null instead of guessed when any of these conditions holds:

- the render has no energy;
- fewer than 20 samples occupy the -5 to -35 dB fit range;
- the regression has a degenerate or non-decaying slope;
- RMS energy in the final 10 percent exceeds `1e-4` of peak level, indicating that the render is too short or its tail/noise floor is too high.

The tail criterion is intentionally conservative. It avoids presenting a confident number when a constant noise floor or truncated decay can make Schroeder integration appear to fall near the buffer boundary. Synthetic tests cover a known exponential response, insufficient decay range, excessive tail noise, and silence.

## Current reference result

At 48 kHz over two seconds, engine 0.1 reports onset at frame 0, thresholded length 37,785 frames, distinct left/right peaks near 0.0712, nonzero stereo-difference RMS, and estimated RT60 near 0.356 seconds. These numbers describe the current floating-point development reference documented in [Barr reference implementation](barr-reference-implementation.md); they are not measurements of original MIDIVerb hardware.
