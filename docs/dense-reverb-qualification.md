# Dense-reverb qualification

M24.1 compares Barr Reference, Gravity Diffusion, Dense Figure Eight, and Four-Line Dense Room without hiding weak results. The automated pass is complete; listening review remains in progress.

## Fixtures and matching

Eight deterministic stereo comparison reels cover percussion, speech-like synthesis, piano, pads, and noise in room, hall, dark, and modulated settings. Every design receives the same source. Each 3.2-second render is integrated-stereo-RMS matched to -24 dBFS, subject to a 0.5 peak ceiling, then concatenated in this fixed order:

1. Barr Reference
2. Gravity Diffusion
3. Dense Figure Eight
4. Four-Line Dense Room

There is 200 ms of silence between designs. The [objective report](../artifacts/measurements/m24-dense-qualification/objective-report.json) records the exact build (`b970de54a813`), normalization, thresholds, and all 32 design/fixture cases. Objective values do not contain or infer subjective listening scores.

## Objective result

The two newer dense tanks clearly improve late echo density. Average late density is 0.99 for Figure Eight and 0.98 for Four-Line, versus 0.56 for Barr and 0.09 for the current Gravity settings. Both dense designs pass the repeat-like density/recurrence gate in all eight cases and all four designs pass the smearing and mono-energy gates.

Coloration now uses maximum positive narrowband ripple after removing the local spectral trend from the strongest energetic late-response window. Raw spectral flatness remains diagnostic, but it is not a pass gate: it confounded intentional damping with resonant coloration and could select a nearly silent late window. Figure Eight and Four-Line remain below the 12 dB ripple limit in every case. Barr exceeds it in the two hall cases. Barr and Gravity still fail the minimum-density gate in all eight cases; those failures remain visible rather than being averaged into one score.

The searched 35.2 / 45.9 / 72.7 / 110.3 ms Four-Line alternative reduced average recurrence from 0.291 to 0.282 and average ripple from 9.30 to 8.99 dB, but lowered the established middle-density comparison below Dense Figure Eight. It therefore remains the optional **Smoother** preview instead of silently replacing the qualified factory tuning.

## Listening worksheet

Record the review in the
[M24 comparative listening session](m24-listening-session-template.md) so every
fixture, mono check, confidence level, and retuning decision is explicit.

Listen at a fixed monitor level; do not change gain between the four sections of one reel. For each section, record short notes separately for:

- repeat-like taps or flutter;
- metallic ringing or stable pitches;
- transient smearing and loss of articulation;
- audible pitch motion or chorus;
- width and mono collapse;
- preference and confidence.

The reels are in [`artifacts/measurements/m24-dense-qualification/`](../artifacts/measurements/m24-dense-qualification/). Start with `percussion-room`, `piano-hall`, `noise-dark`, and `pad-modulated`; use the remaining reels to confirm or contradict the first impression. Do not mark M24.1 complete until listening notes exist; objective measurements are not a substitute for audibility or preference.
