# Control-rate graph semantics

M5.1 defines how a visible control cable changes an audio parameter before LFO and mapping blocks arrive in M5.2. Every modulatable parameter owns one typed control-input socket and four saved mapping properties: amount, polarity, clamp minimum, and clamp maximum. The parameter's ordinary value remains its base value.

The effective value at a control tick is:

`clamp(base + amount × normalizedControl, clampMinimum, clampMaximum)`

- **Bipolar** clamps the incoming control value to -1 through +1.
- **Unipolar** clamps it to 0 through 1.
- A disconnected socket evaluates to the base value; exposing a socket does not change existing sound.
- NaN or infinity at a mapping boundary is treated as zero control. The existing output guards remain the final audio-safety boundary.

The inspector shows the base value and the complete formula inputs together. Control sockets and cables are dashed and use a distinct violet treatment, so their identity does not depend on color alone. Audio/control endpoint mismatches remain hard validation errors, and a control socket accepts one cable. Unlike an occupied audio input, an occupied control socket never offers automatic insertion of an audio Sum block.

## Rate and interpolation

Control graphs tick at a nominal 1 kHz. The prepared quantum is `ceil(sampleRate / 1000)`, so 48 kHz uses 48 samples and 44.1 kHz uses 45 samples. Each newly mapped parameter target is linearly interpolated across the following quantum before audio processing consumes it. A constant control therefore reaches and holds the same effective value as the equivalent static parameter, without a sample-edge step.

The compiler records the prepared sample rate, maximum audio block size, quantum, maximum ticks per block, and maximum mapping evaluations per block. It accepts at most 64 control-participating nodes and 128 connected parameter mappings. For a prepared block size `B`, work is bounded by `ceil(B / quantum) × connectedMappings`; an over-limit plan is invalid before publication. Compilation and allocation remain control-thread work.

## Patch contract

Patch schema v2 adds this object to every parameter that exposes a modulation socket:

```json
"modulation": {
  "portId": "delay-mod",
  "amount": 2.0,
  "polarity": "bipolar",
  "clampMinimum": 0.1,
  "clampMaximum": 100.0
}
```

Writers always emit schema v2. Readers accept v1 and migrate supported modulatable primitive parameters deterministically by exposing their parameter socket and applying the documented default mapping. A parameter without a control socket omits `modulation`; this is used by the reverb-specific Envelope Follower and Hold Gate base controls. V2 reads reject missing mappings on modulatable parameters, mappings on base-only parameters, unknown mapping fields, a socket that does not match the parameter, a non-finite amount, invalid polarity, or clamps outside the parameter's allowed range. Round-trip tests require every present mapping field to survive exactly. Schema v1 remains in the repository as the historical readable contract; schema v2 is the current writable contract.

M5.1 prepares and validates mapping semantics; M5.2 supplies the documented user-creatable LFO and Scale / Offset nodes. M5.3 binds mapped Delay and Allpass targets to fractional linear delay taps and a bounded per-sample Allpass coefficient. See [Modulated Delay and Allpass](modulated-delay-and-allpass.md).

M8.3 adds a user-creatable [Macro control source](macro-control-source.md). Its
runtime value is sampled by this same 1 kHz graph after a fixed 20 ms Macro
ramp; it then reaches audio parameters through the interpolation described
above. Macro value gestures are runtime-only and do not request graph
compilation.

## Reviewed UI evidence

- [`01-mapped-allpass-inspector.png`](../artifacts/ui/m5-1-control-rate-semantics/01-mapped-allpass-inspector.png) shows the real native editor at 125% Windows scaling with the library, graph, and complete inspector contained in the window. The selected Allpass exposes its typed parameter socket, formula, polarity, amount, clamps, and interpolation policy.
- [`control-mapping-inspection.mp4`](../artifacts/ui/m5-1-control-rate-semantics/control-mapping-inspection.mp4) shows the native mapping inspector while bounded live energy telemetry remains active.

The Windows build verifies JUCE 8.0.13's WebView2 physical-pixel bounds conversion during dependency population. WebView2 controller bounds must include the active monitor scale: without that conversion, a 1920-by-1200 display at 125% receives only a 1536-by-960 child and exposes white space on the right and bottom. The check restores the known conversion in an existing patched dependency tree and fails on an unknown JUCE implementation.
