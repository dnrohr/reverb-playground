# Dense-reverb qualification

M24.1 compares Barr Reference, Gravity Diffusion, Dense Figure Eight, and Four-Line Dense Room without hiding weak results. The automated pass is complete; listening review and any resulting retune remain in progress.

## Fixtures and matching

Eight deterministic stereo comparison reels cover percussion, speech-like synthesis, piano, pads, and noise in room, hall, dark, and modulated settings. Every design receives the same source. Each 3.2-second render is integrated-stereo-RMS matched to -24 dBFS, subject to a 0.5 peak ceiling, then concatenated in this fixed order:

1. Barr Reference
2. Gravity Diffusion
3. Dense Figure Eight
4. Four-Line Dense Room

There is 200 ms of silence between designs. The [objective report](../artifacts/measurements/m24-dense-qualification/objective-report.json) records the exact build (`39e82b8077eb`), normalization, thresholds, and all 32 design/fixture cases. Objective values do not contain or infer subjective listening scores.

## Objective result

The two newer dense tanks clearly improve late echo density. Average late density is 0.99 for Figure Eight and 0.97 for Four-Line, versus 0.56 for Barr and 0.09 for the current Gravity settings. Both dense designs pass the repeat-like density/recurrence gate in all eight cases and all four designs pass the smearing and mono-energy gates.

The run also exposes a real caution: Four-Line fails the spectral-flatness coloration threshold in six cases, and Figure Eight fails it in the two dark cases. This is not converted into an overall passing score. Barr and Gravity pass coloration, but their impulse tails fail the minimum-density gate in six and eight cases respectively. These failures remain checked into the report and are the input to the next tuning pass.

## Listening worksheet

Listen at a fixed monitor level; do not change gain between the four sections of one reel. For each section, record short notes separately for:

- repeat-like taps or flutter;
- metallic ringing or stable pitches;
- transient smearing and loss of articulation;
- audible pitch motion or chorus;
- width and mono collapse;
- preference and confidence.

The reels are in [`artifacts/measurements/m24-dense-qualification/`](../artifacts/measurements/m24-dense-qualification/). Start with `percussion-room`, `piano-hall`, `noise-dark`, and `pad-modulated`; use the remaining reels to confirm or contradict the first impression. Do not mark M24.1 complete until notes exist and the six Four-Line coloration warnings have either driven a retune or been accepted with an audible rationale.
