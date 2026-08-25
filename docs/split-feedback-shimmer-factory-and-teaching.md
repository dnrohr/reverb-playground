# Split-Feedback Shimmer factory patch and teaching view

M12.3 publishes the M12.1 architecture and M12.2 qualification as the seventh
complete factory design. The checked schema-v2 document is exported directly
from `makeSplitFeedbackShimmerGraph`; it contains 25 ordinary visible blocks,
29 mono audio cables, saved names and layout, and no hidden shimmer processor.

## Factory and persistence workflow

Choose **Split-Feedback Shimmer** from the Factory Patch menu. The fitted graph
shows the shared tank and its independently bounded return paths. **Normal
Feedback** controls ordinary decay through a 67 ms return. **Shifted Feedback**
controls harmonic ascent through the visible subtractive high-pass, +12
semitone Pitch Shift, damping, and 83 ms return. Both paths recombine before the
shared 149 ms tank delay.

The A/B header remembers this design as **FB SHIMMER** while A remains the Barr
reference. Factory selection creates a clean graph. Continuous parameter edits
use ordinary graph history and the fixed 10 ms runtime crossfade. Undo, Redo,
schema-v2 Save/Load, and standalone/VST3 host restoration retain all 25 nodes,
29 cables, positions, names, modulation mappings, and edited values.

The factory document is reproduced with:

```powershell
.\scripts\generate_split_feedback_shimmer_factory.ps1 -Configuration Release
```

`scripts/generate_factory_patches.mjs --check` locks the exported document to
SHA-256 `4aea9287200d5c585d0e0a982e3bc7114b2bbc8b43e1fabd7bfafd66dbddd9ff`, so a
native-builder change cannot silently leave stale shipped JSON.

## Loop inspection and teaching

The teaching panel exposes three topology-derived focus choices:

- **Normal:** shared tank -> Normal Feedback -> 67 ms return -> recombine.
- **Shifted:** shared tank -> subtractive high-pass -> Pitch +12 -> damping ->
  Shifted Feedback -> 83 ms return -> recombine.
- **Shared tank:** recombine -> diffusion -> 149 ms delay -> damping -> split.

Selecting a focus keeps the complete chosen circulation bright and leaves the
other related return visible but subdued. Selecting an actual return cable also
uses the generic directed-cycle inspector. It finds one normal cycle and two
shifted cycles because the subtractive high-pass is deliberately expressed as
two visible graph paths, `x` and an inverted low-passed copy. Both enumerated
shifted cycles describe the same musical return and are labeled **SHIFTED
RETURN**. Selecting the shared tank finds all three directed graph cycles.

The circulation ladder relates the visible graph to the checked 400 Hz probe:

| Visible circulation | Measured region |
|---|---:|
| Input | 400 Hz |
| First shifted pass | 800 Hz / +12 semitones |
| Second shifted pass | 1600 Hz / +24 semitones |

The late +24 component grows 40.48 dB from the 1.0-second window to the
2.5-second window and is 48.50 dB stronger relative to +12 than the parallel
reference. These measurements establish repeated harmonic circulation in this
project-authored topology. They do not reconstruct or identify any proprietary
shimmer algorithm, and they do not imply that higher passes are audible.

## Safety and quality boundary

The return controls retain the M12 construction limits: Normal Feedback
0...0.58, Shifted Feedback 0...0.14, and a combined visible ceiling of 0.72.
Every cycle crosses explicit shared and return Delays. Invalid algebraic edits
leave the last valid runtime audible; the existing numerical guard, emergency
mute, state reset, and recovery workflow remain authoritative.

The checked qualification also records the current shifter limits: the
strongest grain sideband is -64.20 dB relative to the first octave, a 7 kHz
probe folds energy near 20 kHz at -32.17 dB, closing damping from 9 to 2 kHz
reduces late +24 by 2.63 dB, and late stereo correlation is 0.729. Visible
filtering reduces buildup but does not make the dual-grain linear-interpolation
Pitch Shift alias-free. Full methods and exact values are in
[`split-feedback-shimmer-validation.md`](split-feedback-shimmer-validation.md)
and
[`split-feedback-shimmer-v1.json`](../artifacts/measurements/split-feedback-shimmer-v1.json).

## UI evidence

Reviewed evidence under `artifacts/ui/m12-3-split-feedback-shimmer/` includes:

- `01-complete-split-loops.png`: fitted complete topology with the shifted
  circulation isolated and the teaching ladder visible.
- `02-shared-region-and-inspector.png`: shared delayed region focus alongside
  the response inspector.
- `split-feedback-shimmer-workflow.mp4`: factory selection, loop focus changes,
  continuous feedback editing, impulse/spectral inspection, save, and reload.

The workflow recording uses a deterministic local response fixture only to
make the measurement-view transition reproducible in the browser. That
temporary transport is absent from the committed production bundle. Native
audio, spectral, graph, and persistence tests remain authoritative; UI evidence
proves the shipped presentation and user journey.
