# Runtime graph binding

M2.2 makes the native fixed Barr runtime the authoritative source for the visible patch. TypeScript no longer contains a second list of reference nodes, connections, or parameter values.

## Contract

The JUCE resource provider serves `runtime-snapshot.json` from the processor. Contract version 1 contains:

- engine identity and active sample rate;
- every runtime node's stable ID, type, label, role, mono ports, parameters, values, and units;
- every runtime connection and its exact endpoint ports;
- editor positions for the current presentation;
- processors deliberately outside the reference patch.

The web editor rejects an unsupported contract version, unexpected engine, duplicate node identity, unsupported node type or role, invalid parameter/unit, invalid signal type, or connection to an unknown node before rendering. A binding failure is shown as an explicit error surface instead of falling back to a stale graph.

## Single source of truth

`BarrReferenceRuntime` owns the immutable definitions. `BarrReference::prepare` reads its cutoff, delay, and coefficient values from those definitions. `makeBarrReferenceGraph` copies the same nodes, ports, parameters, and connections into the semantic graph. The runtime-snapshot writer serializes those definitions for the web editor.

Native validation compares semantic graph identity and values with the runtime definitions. Debug builds assert that validation succeeds before a snapshot is served. Tests deliberately rename a node, change a parameter, and remove a connection to prove drift is detected.

## Visible and outside-patch processing

The live-input path is exactly:

```text
Stereo Input -> Mono Sum (L + R, normalized by 0.5) -> Input Low-pass
  -> Diffuser 1 -> Diffuser 2 -> Tank 1 -> Tank 2
  -> explicit left/right branch -> Left Tap / Right Tap -> Stereo Output
```

There is no implicit polarity inversion, channel conversion, summing stage, or delay. The negative Tank 2 allpass coefficient is a visible parameter, not a hidden polarity operation. Every delay is a visible allpass `delay` parameter measured in milliseconds.

The audition impulse is a test stimulus at the normalized sum boundary. Master audition gain and per-channel numerical safety guards run after **Stereo Output**, are visible in the native audition surface, and are declared in `outsidePatch`; they do not change the reference topology.

## Update behavior

The snapshot is loaded when the editor opens. Selecting a node reads the values, ranges, steps, and units supplied by the native snapshot. Continuous editor changes return through the stable node/parameter bridge and update the prepared runtime as described in [Continuous parameter editing](continuous-parameter-editing.md).
