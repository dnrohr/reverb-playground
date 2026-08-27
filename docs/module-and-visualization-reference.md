# Module and visualization reference

This is the user-facing contract for every module and visualization shipped in
the 0.1.0 alpha. Every cable carries one signal: solid/circular connections are
mono audio; dashed/diamond connections are normalized control. Stereo exists
only through explicitly paired mono boundary sockets.

Parameter ranges below are inclusive. A connected control socket applies the
inspector's visible amount and polarity to the base value, then clamps the
result to the displayed minimum and maximum. Control evaluation runs at 1 kHz
with linear interpolation between control frames.

## Modules

### Stereo Input

<!-- module: stereo-input -->

Stereo boundary source with mono audio outputs `out-l` and `out-r`. It has no
parameters. Branch either output freely; joining channels requires an explicit
**Sum (+)** block.

### Stereo Output

<!-- module: stereo-output -->

Stereo boundary sink with mono audio inputs `in-l` and `in-r`. It has no
parameters. Each input accepts one cable.

### Gain / Invert

<!-- module: gain -->

Mono audio input/output with a `gain-mod` control socket. **Gain** is linear,
`-1.000..+1.000`, step `0.001`, default `+1.000`; negative values invert
polarity and zero mutes this path. Default modulation depth is `0.5`.

### Sum (+)

<!-- module: sum -->

Adds mono audio inputs `in-a` and `in-b` into one mono output with no hidden
normalization and no parameters. Use **Gain / Invert** before or after it when
level or subtraction is required. Connecting a second source to an occupied
audio input can insert this block automatically after confirmation.

### Delay

<!-- module: delay -->

Mono variable delay with audio input/output and `delay-mod` control socket.
**Delay** is `0.10..10000.00 ms`, step `0.01 ms`, default `10.00 ms`; default
modulation depth is `10 ms`. The prepared sample-rate-specific memory plan must
fit the project delay budget. Its fractional linear tap makes continuous time
changes click-resistant but intentionally pitch/Doppler active.

### Allpass

<!-- module: allpass -->

Mono Schroeder allpass with audio input/output plus `delay-mod` and
`coefficient-mod` control sockets. **Delay** is `0.10..100.00 ms`, step
`0.01 ms`, default `10.00 ms`, with `2 ms` default modulation depth.
**Coefficient** is unitless `-0.950..+0.950`, step `0.001`, default `+0.500`,
with `0.25` default modulation depth. The coefficient clamp is a hard
stability boundary; delay changes use the same pitch-active fractional tap as
Delay.

### Low-pass

<!-- module: lowpass -->

Mono one-pole low-pass with audio input/output and `cutoff-mod` control socket.
**Cutoff** is `20..20000 Hz`, step `1 Hz`, default `7000 Hz`; default modulation
depth is `5000 Hz`. At runtime the effective cutoff is additionally kept below
Nyquist, so unusual low sample rates remain finite.

### Pitch Shift

<!-- module: pitch-shift -->

Mono dual-grain pitch processor with audio input/output plus `semitones-mod`,
`grain-mod`, and `overlap-mod` control sockets. **Semitones** is
`-12.00..+12.00 st`, step `0.01 st`, default `+12 st`; `+7 st` selects a perfect fifth; **Grain** is
`20.0..120.0 ms`, step `0.1 ms`, default `60 ms`; **Overlap** is normalized
`0.10..1.00`, step `0.01`, default `0.50`. **Direction** selects forward grains
or reverse playback inside each causal grain. **Phase** is
`0.000..0.999 cycles`, step `0.001`, default `0`; it deterministically offsets
the two read heads so paired mono blocks can avoid coincident grain boundaries.
Direction and Phase have no modulation socket.
This is ratio-based musical pitch shift, not fixed-Hz frequency shift, Delay
modulation, whole-response reversal, or pre-input audio. The initial quality is
dual grain with linear interpolation and fixed sample-rate-derived latency.

### LFO

<!-- module: lfo -->

Control generator with one control output and modulation sockets for all four
parameters. **Frequency** is `0.01..100.00 Hz`, step `0.01 Hz`, default `1 Hz`.
**Phase** is `0.000..0.999 cycles`, step `0.001`, default `0`. **Waveform** is
Sine (`0`) or Triangle (`1`). **Run mode** is Free Run (`0`) or Restart on
Transport (`1`). Defaults for modulation amount are respectively `1 Hz`,
`0.25 cycles`, `1`, and `1`; discrete selectors change only at their defined
integer choices.

### Macro

<!-- module: macro -->

User-named normalized control source with exactly one branchable `out` control
socket. **Value** is `-1.000..+1.000`, step `0.001`, default `0`; **Default
value** uses the same range and step and defaults to `0`; **Center detent** is
Off (`0`) or On (`1`, default). With detent enabled, editor gestures within
`0.02` of zero snap to center. Runtime value changes use a fixed 20 ms control
ramp and do not compile topology. Selecting the block highlights and lists
reachable mapped parameters and predicted ranges; these are graph-derived
predictions, not measured audio.

A factory-designated `gravity` presentation adds a prominent bipolar surface:
Inverse at `-1`, Bloom at `0`, and Forward at `+1`, with an exact keyboard
field. Its envelope graphic is a labeled design prediction rather than measured
audio. Expand / Focus frames the visible Macro, mappers, and destinations.
These controls and inspections remain available when Learn is Off.

### Curve Mapper

<!-- module: control-map -->

Transforms normalized control and exposes one control output. **Curve family**
selects Linear (`0`, default), Power (`1`), or Exponential (`2`). **Curve
amount** is `-8..+8`, step `0.01`, default `0`; **Exponent** is `0.1..8`, step
`0.01`, default `1`. **Scale** is
linear `-4.00..+4.00`, step `0.01`, default `+1.00`. **Offset** is unitless
`-1.00..+1.00`, step `0.01`, default `0`. **Polarity** selects Unipolar 0..1
(`0`) or Bipolar -1..+1 (`1`, default). All three have control sockets; default
modulation amounts are `1`, `0.5`, and `1`. **Clamp min/max** are each
`-1..+1`, default `-1/+1`, and min must remain below max. The inspector previews
the bounded output range before connection. Linear mode exactly preserves the
released Scale / Offset behavior; older schema-v2 nodes migrate to neutral
Linear curve fields when loaded.

### Envelope Follower

<!-- module: envelope-follower -->

Converts mono audio magnitude to normalized `0..1` control. **Attack** is
`0.1..500.0 ms`, step `0.1 ms`, default `5 ms`. **Release** is `1..5000 ms`,
step `1 ms`, default `100 ms`. These timing parameters are base-only and have
no modulation sockets in this release.

### Hold Gate

<!-- module: hold-gate -->

Passes or attenuates its mono audio input under a separate normalized `gate`
control input. **Threshold** is unitless `0.00..1.00`, step `0.01`, default
`0.50`. **Attack** is `0.1..100.0 ms`, step `0.1 ms`, default `2 ms`;
**Hold** is `1..2000 ms`, step `1 ms`, default `250 ms`; **Release** is
`0.1..1000.0 ms`, step `0.1 ms`, default `20 ms`. All are base-only in this
release. The detector can connect directly from Envelope Follower or through
one Scale / Offset block.

## Visualizations and inspectors

### Schematic canvas

<!-- visualization: schematic-canvas -->

The graph is the executable design. Solid audio and dashed control cables,
separate port shapes, labels, and explicit stereo sockets expose signal type
without depending on color. Zoom is bounded by React Flow; **Fit** frames the
whole patch. The minimap is navigational, not an audio measurement.

### Feedback-loop highlighting

<!-- visualization: feedback-loops -->

Selecting a participating node or cable highlights one complete directed loop
in amber and alternate loops in violet/dashed style. Previous/Next cycles the
bounded result set. The inspector totals nominal delay and lists gain,
polarity, and filters on the path. This topology-derived heuristic is not a
proof of stability.

### Live energy

<!-- visualization: live-energy -->

Five block segments and cable width show measured RMS mapped over
`-72..0 dBFS`, with `42 ms` visual attack, `260 ms` visual release, and a
`100 ms` stale-generation release. The 30 Hz native publication is
presentation-only. **Energy Off** stops polling and native scans; reduced-motion
preference starts and locks it off.

### Stereo impulse and decay viewer

<!-- visualization: impulse-decay -->

The upper solid lane is left amplitude, the middle dashed lane is right
amplitude, and the lower white lane is backward-integrated stereo energy in
decibels. Capture length is `100..10000 ms`; the UI offers 500, 2000, 5000,
and 10000 ms. Silence threshold is clamped to `-120..-24 dBFS`; the UI offers
-60, -80, -100, and -120 dBFS. The stimulus is `0.1` peak. Waveform display is
bounded to 450 min/max buckets; zoom reaches 256x. RT60 is a T30 regression over
-5 through -35 dB and is withheld when energy, tail, sample count, or slope is
not defensible.

### Perceptual density inspector

<!-- visualization: density-inspector -->

The optional capture-driven panel plots normalized echo density as a solid
line, recurrence as a dashed line, and spectral flatness as a dotted line.
Prominent recurrence windows carry their millisecond lag as text. Separate
early/middle/late cards retain active peak rate, crest, recurrence, flatness,
and stereo correlation. Opening the panel triggers bounded response analysis;
while closed it adds no audio-thread, polling, or background analysis work.

### Architecture teaching overlays and A/B

<!-- visualization: teaching-overlays -->

For the reverse-envelope factory, measured onset-to-peak and -40 dB landmarks
label rising energy and the late peak. For the gated factory, gate/open, hold,
release, and cutoff regions derive from captured samples and visible gate
timings. **Learn Off** removes overlays and explanatory prose but does not
change audio. **A / Barr** and **B / Reverse Env** or **B / Gated** load normal
visible factory graphs through the same runtime transition as other edits.

### Control previews

<!-- visualization: control-previews -->

LFO and Scale / Offset blocks show signed control motion and control cables use
dashed animation. The inspector's range preview predicts the normalized mapped
minimum/maximum from polarity, scale, and offset. These are control-rate facts,
not audio waveforms; reduced motion removes animation without changing values.

### Pitch-grain inspector

<!-- visualization: pitch-grains -->

Two markers explain the saved dual-read-head phase relationship, direction,
grain length, and overlap. The panel reports the exact configured latency and
quality but labels marker motion as illustrative design state: it is neither a
waveform nor measured, sample-accurate read-head telemetry. Reduced-motion mode
holds both markers in a static separated state without removing any labels.

### Diagnostics

<!-- visualization: diagnostics -->

The panel labels operation count as an active prepared-plan estimate, callback
load and clipping as aggregate live measurements, and total runtime memory as
exact prepared bytes. It also exposes node/cable/feedback counts, the current
block-wise or sample-wise execution domain, expensive processor families,
validation/scheduling/preparation time, request-to-active delay, and stale
compiled work that was superseded. Its
compiled-latency card shows active samples/milliseconds, both output paths, and
the node identity and sample difference of every uncompensated parallel join;
these are prepared graph facts rather than callback timing. Graph
publication revisions distinguish compiling, active, failed, crossfading, and
last transition. A safety event records immutable graph revision, channel, and
sample identity. Recovery remains disabled until safety mute is active and
clears stored feedback state explicitly.

## Related contracts

- [Patch format](patch-format.md) defines persistence, units, ports, and schema
  compatibility.
- [Real-time and safety contract](real-time-safety-contract-v1.md) defines
  resource limits and failure behavior.
- [Schematic editor interactions](schematic-editor-interactions.md) lists
  pointer and keyboard controls.
- [Control-rate graph semantics](control-rate-graph-semantics.md) defines the
  exact modulation formula and scheduling policy.
- [Graph latency and host reporting](graph-latency-and-host-reporting.md)
  defines compiled path rules, the no-hidden-compensation policy, and JUCE host updates.
- [Graph runtime diagnostics](graph-runtime-diagnostics.md) defines prepared-plan
  workload estimates, compile/publication measurements, and real-time boundaries.
