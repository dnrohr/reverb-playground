# Patch format v1

Runtime acceptance also follows the versioned [real-time and safety contract](real-time-safety-contract-v1.md), including the rule that every feedback cycle contains an explicit stateful delay.

The authoritative machine-readable definition is [`schemas/patch-v1.schema.json`](../schemas/patch-v1.schema.json). The framework-light C++ representation and semantic validator live in `src/graph/`.

## Design goals

- Preserve stable identity for nodes, ports, parameters, and connections.
- Keep the DSP graph independent from editor position and viewport state.
- Represent stereo as two explicit mono audio ports rather than an implicit stereo cable.
- Distinguish audio and control/modulation connections before graph compilation.
- Store musical delay values in milliseconds without rounding to samples.
- Reject unknown schema versions rather than guessing.
- Produce deterministic JSON so equivalent saved state is easy to test and review.

## Top-level structure

```json
{
  "schemaVersion": 1,
  "engineVersion": "0.1",
  "semantic": {
    "nodes": [],
    "connections": []
  },
  "layout": {
    "nodes": [],
    "viewport": { "x": 0.0, "y": 0.0, "zoom": 1.0 }
  }
}
```

`semantic` determines sound and graph validity. `layout` determines presentation only. Moving a node or changing the viewport must not alter the compiled DSP graph.

## Nodes and ports

A node contains:

- a stable, non-empty `id`;
- a stable type identifier such as `allpass`, `sum`, or `stereo-input`;
- explicit ports;
- parameters with value and unit.

Every port declares:

- an ID unique within its node;
- `audio` or `control` signal type;
- `input` or `output` direction.

All audio cables are mono. A stereo input is therefore represented as two audio output ports:

```json
{
  "id": "input",
  "type": "stereo-input",
  "ports": [
    { "id": "out-l", "signal": "audio", "direction": "output" },
    { "id": "out-r", "signal": "audio", "direction": "output" }
  ],
  "parameters": []
}
```

The same rule gives a stereo output separate `in-l` and `in-r` ports. The Barr reference fixture visibly connects both inputs into a mono sum and branches one network back into two output ports.

## Connections

A connection has its own stable ID and references one source and one destination by node/port ID.

Semantic validation requires:

- both referenced ports exist;
- the source is an output;
- the destination is an input;
- source and destination signal types match;
- node, port, parameter, connection, and layout identities are unique in their scope.

Graph-cycle legality, occupied single-input policy, and node-specific parameter ranges are compiler/product validation layered above this base schema.

## Parameters and units

Version 1 permits these units:

- `coefficient`
- `decibels`
- `hertz`
- `linear`
- `milliseconds`
- `unitless`

Delay time defaults to milliseconds. Conversion to sample counts occurs when a runtime is prepared for a sample rate; saved state does not silently rewrite musical time as an integer sample count. Historical fixed-sample-rate behavior will require an explicit later schema/engine extension.

Parameters are base values. Modulation mapping is represented by typed control connections and dedicated mapping nodes when that feature enters the engine; it is not hidden inside the serialized numeric value.

## Layout

Layout contains zero or one position per known node and a viewport. Semantic validation rejects positions for unknown nodes, duplicate positions, and non-positive zoom.

A loader may calculate a default position for a semantic node omitted from layout. It may not invent or remove semantic connections based on visual proximity.

## Versioning and migration

- `schemaVersion` describes the serialized document shape and is currently exactly `1`.
- `engineVersion` records the engine family that authored the patch.
- The v1 parser rejects unsupported schema versions.
- A future incompatible shape increments `schemaVersion` and adds a tested migration into the current in-memory model.
- Migrations preserve stable IDs unless the old format did not contain them; generated replacement IDs must then be deterministic or explicitly recorded.
- Unknown fields are rejected by the JSON Schema. Forward compatibility is handled by versioned migrations, not silent field loss.
- Factory fixtures from every released schema remain in tests.

## Fixtures and verification

- Valid Barr-shaped fixture: `tests/fixtures/patches/valid/barr-minimal.json`
- Invalid signal-type fixture: `tests/fixtures/patches/invalid/audio-control-connection.json`

Native tests prove:

- explicit stereo mono ports;
- audio/control type rejection;
- stable semantic and layout IDs;
- exact in-memory round trips of millisecond values;
- deterministic repeated serialization;
- required JSON Schema metadata.
