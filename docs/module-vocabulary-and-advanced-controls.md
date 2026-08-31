# Module vocabulary and Advanced controls

M27.3 reduces first-glance inspector density without changing the executable
graph. Saved module types, parameter IDs, ports, values, and factory graphs are
unchanged.

| Block | Saved type | Signal contract | Common controls | Advanced controls |
| --- | --- | --- | --- | --- |
| Stereo Input | `stereo-input` | stereo boundary to two mono audio outputs | none | none |
| Stereo Output | `stereo-output` | two mono audio inputs to stereo boundary | none | none |
| Gain | `gain` | one mono audio input/output | Gain | modulation mapping |
| Sum (+) | `sum` | two mono audio inputs, one mono audio output | none | none |
| Delay | `delay` | one mono audio input/output | Delay | modulation mapping |
| Allpass | `allpass` | one mono audio input/output | Delay, Coefficient | modulation mappings |
| Low-pass | `lowpass` | one mono audio input/output | Cutoff | modulation mapping |
| Pitch Shift | `pitch-shift` | one mono audio input/output | Semitones | Grain, Overlap, Direction, Phase, modulation mappings |
| Macro | `macro` | one branchable control output | Value | Default value, Center detent |
| LFO | `lfo` | one branchable control output | Frequency | Phase, Waveform, Run mode, modulation mappings |
| Curve Mapper | `control-map` | one control input/output | Scale, Offset | Polarity, curve fields, clamps, modulation mappings |
| Envelope Follower | `envelope-follower` | mono audio input to control output | Attack, Release | none |
| Hold Gate | `hold-gate` | mono audio and control inputs to mono audio output | Threshold | Attack, Hold, Release |

## Naming and behavior

The palette name is now **Gain**. Negative values still invert polarity; there
is no separate invert switch or saved mode. A factory or user-defined block
name remains visible because descriptive instance names are useful teaching
labels.

Every selected block now exposes its signal/channel contract, port counts, and
audible role. Parameters show units and behavior badges:

- **Modulated / Smoothed** means a visible control socket can move the value
  through the runtime smoothing path.
- **Smoothed runtime** identifies a performance value such as Macro Value.
- **Base only / Crossfaded rebuild** means the setting is compiled off-thread
  and published through the existing click-safe graph crossfade.

## Disclosure contract

Advanced uses a native `details`/`summary` disclosure. Its open state is local
presentation state: it is absent from patch serialization and undo history and
cannot publish or alter audio. Common controls never move into the disclosure.
The disclosure remains keyboard- and screen-reader-operable and the inspector
scrolls rather than widening the application at compact sizes.

Reviewed evidence:

- [Pitch Shift Advanced inspector](../artifacts/ui/m27-3-inspector-vocabulary/pitch-advanced.jpg)
- [Gain vocabulary and role](../artifacts/ui/m27-3-inspector-vocabulary/gain-inspector.jpg)
- [Advanced inspector at 640 by 400](../artifacts/ui/m27-3-inspector-vocabulary/advanced-640x400.jpg)
- [Disclosure workflow](../artifacts/ui/m27-3-inspector-vocabulary/advanced-disclosure-workflow.mp4)
