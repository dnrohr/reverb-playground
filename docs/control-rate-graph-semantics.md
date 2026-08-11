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

Patch schema v2 adds this required object to every parameter:

```json
"modulation": {
  "portId": "delay-mod",
  "amount": 2.0,
  "polarity": "bipolar",
  "clampMinimum": 0.1,
  "clampMaximum": 100.0
}
```

Writers always emit schema v2. Readers accept v1 and migrate supported primitive parameters deterministically by exposing their parameter socket and applying the documented default mapping. V2 reads reject unknown mapping fields, a socket that does not match the parameter, a non-finite amount, invalid polarity, or clamps outside the parameter's allowed range. Round-trip tests require every mapping field to survive exactly. Schema v1 remains in the repository as the historical readable contract; schema v2 is the current writable contract.

M5.1 prepares and validates mapping semantics; M5.2 supplies user-creatable LFO/control-source nodes, and M5.3 binds mapped Delay and Allpass targets to their specialized interpolation paths.

## Reviewed UI evidence

- [`01-mapped-allpass-inspector.png`](../artifacts/ui/m5-1-control-rate-semantics/01-mapped-allpass-inspector.png) shows the real native editor at 125% Windows scaling with the library, graph, and complete inspector contained in the window. The selected Allpass exposes its typed parameter socket, formula, polarity, amount, clamps, and interpolation policy.
- [`control-mapping-inspection.mp4`](../artifacts/ui/m5-1-control-rate-semantics/control-mapping-inspection.mp4) shows the native mapping inspector while bounded live energy telemetry remains active.

The Windows build applies a narrow JUCE 8.0.13 WebView2 bounds patch during dependency population. JUCE otherwise multiplies controller bounds that are already in the required coordinate space, creating a child 125% wider than its component on a 125%-scaled display. The patch is guarded by an exact source match so a future JUCE update fails configuration instead of silently applying an obsolete modification.
