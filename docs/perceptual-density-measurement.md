# Perceptual density measurement

Status: M19.1 measurement contract and baseline, version 1.

The density analyzer is an offline tool. It does no work in the audio callback. It analyzes equal-length stereo impulse responses in overlapping 40 ms windows with a 20 ms hop, then summarizes the first, middle, and final thirds. The checked baseline covers every released factory at 44.1, 48, and 96 kHz.

## Measurements and meaning

- **Echo density** is the fraction of mono samples above that window's RMS, normalized by the same probability for Gaussian noise and clamped to 0...1. Values near one resemble a noise-like temporal field; this is not a count of geometrical reflections.
- **Active peaks per second** counts local absolute maxima above half the window RMS. It makes isolated repeats explicit, but oscillatory or high-bandwidth material can also raise it.
- **Crest factor** is peak divided by RMS. Large values identify impulse-like windows; silence reports zero.
- **Kurtosis** is the normalized fourth moment. Gaussian noise approaches three; sparse outliers are much larger. It is undefined perceptually for silence, which reports zero.
- **Energy variation** is the coefficient of variation across eight equal subwindows. It reveals bursts and gaps, but also follows the intended decay envelope.
- **Recurrence** is the strongest absolute normalized autocorrelation from 1 to 30 ms. The report also records its lag. Tonal ringing and repeating delay structure both raise it, so it is evidence of periodicity rather than a diagnosis of its cause.
- **Spectral flatness** is the geometric-to-arithmetic mean of 64 deterministic DFT power bins. A single impulse is spectrally flat even though it is temporally sparse; this metric must therefore be read beside echo density and crest factor.
- **Stereo correlation** is normalized zero-lag correlation. It describes width/polarity, not density, and silent windows report zero.

No single composite score is published. Dense reverb tuning must improve temporal density and recurrence without hiding regressions in coloration, stereo behavior, or envelope shape.

## Determinism and limits

The analyzer uses fixed window boundaries, a fixed Hann DFT, and no random state. Controlled sparse and deterministic noise-like fixtures prove separation and rate stability at the three supported rates. Values are calculated from rendered floating-point output and may receive explicit tolerance if a future platform changes elementary math behavior.

The early/middle/late regions are equal thirds of the requested render, not perceptual phases inferred from onset or RT60. Silence is retained rather than discarded; this makes gated or short responses honest but means an average can fall when a region contains no tail. M19.2 will expose the window curve so a summary cannot conceal that timing.

## Reproduction

After building Release, regenerate the versioned report with:

```powershell
.\build\windows-msvc\src\render\Release\reverb_density_baseline_cli.exe --factory-directory factory-patches --output artifacts\measurements\factory-density-baseline-v1.json
```

The authoritative report is [`factory-density-baseline-v1.json`](../artifacts/measurements/factory-density-baseline-v1.json). It contains 24 entries: eight factory families at three sample rates. Each entry retains the full window curve and all three summaries so later figure-eight and FDN work can compare against the actual pre-M20 baseline.
