# LFO and control-mapping blocks

M5.2 added two user-creatable control blocks to the schematic: **LFO** and the
original **Scale / Offset** mapper. M8.2 evolves that same persisted
`control-map` node into the backward-compatible [Curve Mapper](curve-mapper.md).
They use the 1 kHz bounded control plan defined in M5.1. Control cables remain
mono, may branch from one output to many parameter sockets, and are visually
dashed; control blocks add a dashed outline, double left rule,
waveform/mapping label, signed numeric preview, and moving position marker so
signal identity does not depend on violet alone.

## LFO contract

The LFO produces a normalized bipolar value in `[-1, +1]`.

- **Frequency:** 0.01 through 100 Hz. The prepared control generator clamps against the control-rate Nyquist boundary as a final safety measure.
- **Phase:** cycles in `[0, 0.999]`; `0.25` is a quarter cycle.
- **Waveform:** sine or triangle. Triangle starts at `-1`, reaches `+1` after half a cycle, and returns to `-1` at the wrap.
- **Free run:** a transport/reset notification preserves the current oscillator phase.
- **Restart on transport:** the same notification restores the saved phase offset.
- An explicit restart always restores the saved phase, which makes deterministic audition and tests possible in either mode.

Native tests exercise frequency, phase, both waveforms, phase wrapping, restart, and free-run behavior. Generation uses fixed scalar state and performs no allocation, locking, logging, or I/O.

## Scale / Offset contract

The mapping block computes:

`clamp(normalizedInput × scale + offset, -1, +1)`

Its input mode is explicit:

- **Bipolar** first clamps input to `[-1, +1]`.
- **Unipolar** first clamps input to `[0, +1]`.

Scale ranges from `-4` to `+4`, so negative values invert the control. Offset ranges from `-1` to `+1`. The inspector evaluates both input endpoints and shows the resulting minimum/maximum before any output cable is connected. This makes polarity, inversion, and clamping consequences inspectable before patching.

## Preview and milestone boundary

The schematic evaluates a bounded 30 Hz visual preview for draft control nodes and animates the already-dashed control cable. Reduced-motion mode freezes the preview and suppresses dash animation. The preview is explicitly a control-graph visualization; it does not pretend the draft topology is active audio DSP.

Native control-rate compilation records LFO and mapper definitions, their source identities, modes, and predicted ranges. M5.3 consumes those prepared targets inside modulated Delay and Allpass implementations. M5.4 later publishes edited topology to the live audio runtime.

## Evidence

- Creation, mapping, one-to-many branching, non-colour control styling, and the pre-connection range inspector: [`01-lfo-mapper-branching.png`](../artifacts/ui/m5-2-lfo-control-mapping/01-lfo-mapper-branching.png).
- Live signed preview markers and animated dashed control cables: [`live-control-preview.mp4`](../artifacts/ui/m5-2-lfo-control-mapping/live-control-preview.mp4).
