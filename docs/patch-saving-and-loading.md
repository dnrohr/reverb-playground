# Saving and loading patches

M2.4 adds explicit **Save Patch** and **Load Patch** actions to the Barr reference editor. A saved `barr-reference.rvp.json` is a schema-v1 patch document, so the diagram, parameter values, and editor layout share the same versioned representation already used by the native graph library.

## What is saved

- `schemaVersion: 1` and `engineVersion: "0.1"`;
- every semantic node, mono port, parameter value/unit, and connection;
- every node's schematic coordinates;
- viewport pan and zoom.

Runtime presentation metadata—labels, teaching roles, slider ranges/steps, and native descriptor identity—is not duplicated in the file. Loading reconstructs known reference nodes from the native descriptor and user-created nodes from the versioned module registry.

## Atomic load behavior

The browser reads the selected file into temporary memory and completes JSON, schema, reference-identity, range, layout, and viewport validation before calling any React setter or native parameter function. Only a fully valid temporary document replaces the visible copy and publishes its parameter values. An invalid file leaves the current nodes, cables, values, and viewport untouched and displays an actionable `Patch load rejected: …` diagnostic.

After a successful load, selection and undo/redo history are cleared because the loaded document becomes the new editing baseline. Every loaded DSP parameter is then sent through the existing lock-free parameter bridge; file I/O and parsing remain on the UI thread.

## Schema and future fields

Schema v1 is closed: unknown fields are rejected at every defined object boundary. An unknown top-level field therefore produces a diagnostic such as `document contains unknown field 'futureField' (schema v1 rejects future fields)`. Unsupported schema or engine versions are also rejected. Forward compatibility will use explicit, tested migrations rather than silently discarding data from a newer author.

## Editable-graph boundary

M3.1 extends schema-v1 persistence to supported created and deleted nodes while requiring exactly one stereo input and output. Ports, parameter units/ranges, endpoints, IDs, layout, and viewport are validated before replacement. User-created topology remains a saved draft until graph compilation is implemented; only known Barr-reference identities publish parameter values to the current DSP runtime.

## Verification

Web tests prove deterministic round trips, exact numeric values, exact node positions and viewport values, invalid JSON/version/range rejection, and the unknown-field policy. UI evidence under `artifacts/ui/m2-4-patch-persistence` demonstrates successful save/load restoration and an invalid-load diagnostic with the previously valid patch still visible.
