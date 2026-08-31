# Primary user journey qualification

M25.5 qualifies the focused workspace as an internal candidate for the later
M7.5 non-implementer sessions. It does not substitute implementer testing for
those three external sessions.

## Method

The three journeys ran in the rebuilt Release standalone at 48 kHz on Windows
with 150% display scaling. A real WAV drove audition and export. The web-only
reference viewport was also measured at 1200 by 720 logical pixels. Commands
count user decisions—pointer drags count once, while passive status changes and
operating-system file-dialog navigation do not.

The checked result is stored in
[`m25-5-primary-user-journeys.json`](../artifacts/measurements/m25-5-primary-user-journeys.json).
Regressions are listed separately from preference feedback.

## Results

| Journey | Commands | Result |
|---|---:|---|
| Musician | 10 | Selected Gravity Diffusion, loaded and played a real file, adjusted Gravity to `+0.35`, switched A/B, and exported WAV without a graph-structure edit. |
| Sound designer | 10 | Added Delay and Pitch Shift, connected them, tuned Delay to `42 ms`, saved, deleted, and restored the edit with Undo while remaining in Inspector. |
| Learner | 8 | Loaded Split-Feedback Shimmer, traced the shifted loop, observed Energy, captured a current response, opened density analysis, and read the architecture explanation without covering the graph. |

At 1200 by 720, the canvas occupies 55.3% of the complete web surface in
Balanced, 86.4% in Create Focus, and 62.2% in Learn & Inspect. The graph viewport
inside that canvas occupies 51.6%, 80.6%, and 58.0% respectively. The center is
therefore visually dominant in all three intentional arrangements, with the
largest trade made only when the user explicitly asks for the learning dock.

## Regressions fixed during qualification

1. Native Tab traversal originally skipped the embedded editor and cycled back
   to the audition strip. The WebView is now an explicitly named keyboard-focus
   target. A repeated Release traversal proceeds from native audition and safety
   controls into File, Edit, View, Help, the factory selector, Energy, and Save.
2. At narrow logical widths and 150% scaling, the compact header correctly hid
   the inline A/B switch but offered no alternate command. View now contains
   keyboard-reachable **Compare A / Barr** and **Compare B / current design**
   radio commands while the wide inline switch remains unchanged.

Preference feedback is not promoted to a defect: a future performance-macro
strip could save musicians one graph selection, but the present macro workflow
is direct, visible, and does not mutate structure.

## Evidence

- [`musician-journey.png`](../artifacts/ui/m25-5-primary-journeys/musician-journey.png)
  shows the adjusted Gravity macro, loaded waveform, completed export, and View
  menu A/B fallback.
- [`musician-journey.mp4`](../artifacts/ui/m25-5-primary-journeys/musician-journey.mp4)
  shows file selection, playback, comparison, and deterministic export.
- [`sound-designer-journey.png`](../artifacts/ui/m25-5-primary-journeys/sound-designer-journey.png)
  shows the restored connected blocks and `42 ms` Delay in Inspector.
- [`sound-designer-journey.mp4`](../artifacts/ui/m25-5-primary-journeys/sound-designer-journey.mp4)
  shows creation, connection, tuning, save, delete, and Undo recovery.
- [`learner-analysis.png`](../artifacts/ui/m25-5-primary-journeys/learner-analysis.png)
  shows density buildup, recurrence, and early/middle/late summaries beside the
  highlighted complete graph.
- [`learner-journey.png`](../artifacts/ui/m25-5-primary-journeys/learner-journey.png)
  shows shifted-loop teaching and documented architecture while retaining graph
  context.
- [`learner-journey.mp4`](../artifacts/ui/m25-5-primary-journeys/learner-journey.mp4)
  shows feedback selection, response capture, Analyze, and Learn transitions.

## Candidate boundary

Internal regressions are closed and all three journeys pass. M7.5 still requires
three anonymous non-implementer sessions, including its keyboard-only segment;
this milestone only makes the reorganized workspace the candidate for them.
