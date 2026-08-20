# Reverse Cosmic Shimmer factory and validation

Reverse Cosmic Shimmer is the published M13 reference instrument: a visible
three-tap causal rise feeding dark normal and paired reverse-octave feedback,
independent slow allpass motion, and unequal stereo extraction. The factory
contains 45 editable public blocks and 57 mono cables. It is project-authored
behavioral synthesis, not a reconstruction of a proprietary algorithm or
preset.

## What “reverse” means

The response is causal. Its Pitch Shift blocks wait for their declared history
and play samples backward only inside each buffered grain. They do not reverse
the complete reverb recording and cannot create pre-input audio. The visible
80/240/520 ms weighted taps produce the rising envelope without reversing any
sample order. A true sample-order reverse reverb would require recording a
bounded segment or impulse response and reading that complete segment backward;
this factory deliberately does not claim that behavior.

## Checked fixture results

The deterministic generator renders a 4 s stereo impulse, three-note chord
burst, and bounded-noise burst at every qualified rate. The complete report is
[`reverse-cosmic-shimmer-v1.json`](../artifacts/measurements/reverse-cosmic-shimmer-v1.json).

| Rate | First wet | Peak | Late/early energy | Final/mid decay | Octave balance growth | L/R correlation | Mono energy |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 44.1 kHz | 261.00 ms | 721.43 ms | +6.33 dB | -37.57 dB | +23.49 dB | 0.171 | 0.586 |
| 48 kHz | 261.00 ms | 721.42 ms | +6.32 dB | -36.93 dB | +23.63 dB | 0.191 | 0.595 |
| 96 kHz | 261.00 ms | 721.49 ms | +6.29 dB | -34.43 dB | +24.28 dB | 0.476 | 0.738 |

“Octave balance growth” compares the +12-semitone chord-band level relative to
the source-frequency band in 0.30–0.60 s and 1.30–1.70 s windows. It describes
late harmonic evolution even while the overall tail decays. Correlation and
mono-energy measurements use the bounded-noise response after 0.8 s. All nine
renders are finite and below full scale.

Checked stereo fixtures:

- [44.1 kHz impulse](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-44k1-impulse.wav), [chord](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-44k1-chord.wav), and [noise](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-44k1-noise.wav)
- [48 kHz impulse](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-48k-impulse.wav), [chord](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-48k-chord.wav), and [noise](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-48k-noise.wav)
- [96 kHz impulse](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-96k-impulse.wav), [chord](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-96k-chord.wav), and [noise](../artifacts/audio/m13-3-reverse-cosmic-shimmer/reverse-cosmic-shimmer-96k-noise.wav)

Regenerate them after an intentional topology or DSP change:

```powershell
cmake --build --preset windows-release --target reverb_reverse_cosmic_shimmer_validation_cli
.\scripts\generate_reverse_cosmic_shimmer_validation.ps1
```

The native test regenerates the report in memory and requires exact JSON
equality, in addition to behavior thresholds. Fixture names and PCM16 hashes
in the report detect accidental audio drift.

## Editor workflow

The teaching card can focus the causal rise, paired reverse grains, dark return
network, or stereo motion. A persistent four-way comparison strip switches
among Barr Reference, Modulated Cosmic Reverse, Split-Feedback Shimmer, and
Reverse Cosmic Shimmer. The ordinary factory selector remains available, and
the existing A/B control remembers the last selected non-Barr design.

The normal and shifted feedback controls remain separate continuous edits.
Selecting a return exposes its complete feedback loop, nominal delay, gains,
and filters. Selecting either Pitch Shift exposes direction, grain, overlap,
phase, latency, and the causal boundary. The graph saves as schema v2 and
restores exactly in editor files and standalone/VST3 host state.

Reviewed evidence:

- [Fitted complete graph and reverse-grain focus](../artifacts/ui/m13-3-reverse-cosmic-shimmer/fitted-reverse-grains.jpg)
- [Reverse-pitch inspector and feedback path](../artifacts/ui/m13-3-reverse-cosmic-shimmer/reverse-pitch-inspector.jpg)
- [Stereo-motion focus and LFO inspector](../artifacts/ui/m13-3-reverse-cosmic-shimmer/modulation-inspector.jpg)
- [DPI-aware packaged standalone window inspection](../artifacts/ui/m13-3-reverse-cosmic-shimmer/standalone-window-fit-125-percent.png), showing the editor filling its 1200×720 content area with both sidebars reachable at 125% Windows scaling
- [Selection, modulation, continuous feedback edit, save, and comparison video](../artifacts/ui/m13-3-reverse-cosmic-shimmer/reverse-cosmic-workflow.mp4)
- [Complete interaction demonstration](../artifacts/ui/m13-3-reverse-cosmic-shimmer/reverse-cosmic-complete-demo.mp4), combining the current factory workflow with the product's measured-response, numerical safety recovery, and valid/invalid save-reload workflows

The UI recording is visual evidence. Deterministic native tests remain
authoritative for measurement, continuous runtime publication, numerical mute
and recovery, exact save/reload, host restoration, and audio behavior.
