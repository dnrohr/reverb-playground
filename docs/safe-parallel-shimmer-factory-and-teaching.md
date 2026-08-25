# Safe Parallel Shimmer factory patch and teaching view

M11.2 ships the M11.1 architecture as the sixth complete factory design. The
checked schema-v2 document is generated directly from
`makeSafeParallelShimmerGraph`; it contains 28 ordinary visible blocks, 32 mono
audio cables, all layout positions, and no factory-only processor or state.

## Factory and editing workflow

Choose **Safe Parallel Shimmer** from the Factory Patch menu. The editor fits
the complete tank and two post-tank paths into the schematic. Saved display
names identify Reverb Tank, Reverb Decay, Normal / aligned, Octave / +12 st,
Shimmer Damping, Shimmer Level, Parallel Recombine, Wet Balance, and the unequal
stereo extraction stages without changing their public module types.

The A/B header remembers Safe Parallel Shimmer as B while A remains the Barr
reference. Factory selection resets history to a clean graph. Editing Shimmer
Level creates one ordinary graph-history transaction; Undo and Redo restore the
exact parameter. Save emits the complete schema-v2 graph, and loading that file
returns it as an ordinary custom patch. Native host state retains all 28 nodes,
32 cables, 28 positions, names, modulation mappings, and edited values before
and after audio preparation.

The factory document is reproduced with:

```powershell
.\scripts\generate_safe_parallel_shimmer_factory.ps1 -Configuration Release
```

`scripts/generate_factory_patches.mjs --check` locks its SHA-256 alongside the
catalog, so a native-builder change cannot silently leave stale shipped JSON.

## What the teaching view claims

The inspector labels the design **Parallel Shimmer / One Pitch Pass** and shows:

- **Normal:** tank -> 360.01 ms alignment -> level.
- **Octave +12:** subtractive high-pass -> Pitch Shift -> damping -> diffusion.
- Both branches recombine after the tank, before Wet Balance and unequal stereo
  extraction.

Selecting a normal or shimmer block highlights the corresponding row. The copy
explicitly contrasts this topology with classic feedback shimmer: the octave
path cannot return through Pitch Shift, so it produces one stable +12-semitone
halo rather than cumulative +24/+36 octaves. This is a structural and measured
claim about this project-authored graph, not a claim about a proprietary
product's implementation.

## Spectral and safety evidence

The deterministic 48 kHz fixture drives both inputs with a 330 Hz sine for four
seconds. Hann-windowed band searches inspect 0.6-second windows beginning at
1.2 and 3.0 seconds. The checked result is
[`safe-parallel-shimmer-v1.json`](../artifacts/measurements/safe-parallel-shimmer-v1.json).

| Window | 660 Hz halo vs 330 Hz | 1320 Hz vs halo | 2640 Hz vs halo |
|---|---:|---:|---:|
| Early, 1.2 s | -24.46 dB | -50.91 dB | -65.07 dB |
| Late, 3.0 s | -24.24 dB | -55.18 dB | -69.77 dB |

The halo remains present in both windows. The next two octave bands remain at
least 50 dB below it, which is much stronger than the -13 dB no-staircase gate.
This does not claim an alias-free shifter; M10.4's folded-alias limitation still
applies, and the visible pre/post filters remain part of this factory.

The checked factory compiles and round-trips exactly. Impulse and bounded-noise
renders remain finite and below full scale at 44.1, 48, and 96 kHz; unequal
11.9/19.7 ms output Allpasses remain measurably stereo-different.

## UI evidence

Reviewed evidence under `artifacts/ui/m11-2-safe-parallel-shimmer/` includes:

- `01-complete-parallel-topology.png`: the fitted graph with tank, aligned
  normal branch, octave branch, recombination, and teaching panel.
- `02-shimmer-level-and-teaching.png`: selected Shimmer Level control and the
  explicit classic-feedback contrast.
- `safe-parallel-shimmer-workflow.mp4`: factory selection, Shimmer Level edit,
  response measurement, schema-v2 save, and saved-file reload.

The workflow recording used a deterministic local response fixture only to make
the measurement-view transition reproducible in the browser. That temporary
transport is absent from the committed production bundle. Native audio,
spectral, and persistence tests remain authoritative; UI evidence proves the
shipped presentation and user journey.
