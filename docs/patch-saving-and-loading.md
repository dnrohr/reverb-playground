# Saving and loading patches

M2.4 adds explicit **Save Patch** and **Load Patch** actions to the Barr reference editor. A saved `barr-reference.rvp.json` is a schema-v1 patch document, so the diagram, parameter values, and editor layout share the same versioned representation already used by the native graph library.

## What is saved

- `schemaVersion: 2` and `engineVersion: "0.1"`;
- every semantic node, mono port, parameter value/unit, and connection;
- every node's schematic coordinates;
- viewport pan and zoom.

Runtime presentation metadata—labels, teaching roles, slider ranges/steps, and native descriptor identity—is not duplicated in the file. Loading reconstructs known reference nodes from the native descriptor and user-created nodes from the versioned module registry.

## Atomic load behavior

The browser reads the selected file into temporary memory and completes JSON, schema, reference-identity, range, layout, and viewport validation before calling any React setter or native parameter function. Only a fully valid temporary document replaces the visible copy and publishes its parameter values. An invalid file leaves the current nodes, cables, values, and viewport untouched and displays an actionable `Patch load rejected: …` diagnostic.

After a successful load, selection and undo/redo history are cleared because the loaded document becomes the new clean editing baseline. Every loaded DSP parameter is then sent through the existing lock-free parameter bridge; file I/O and parsing remain on the UI thread. Saving updates the clean-state marker without clearing history or changing the graph's semantic hash, so the user may still undo after saving.

## Schema and future fields

Schemas v1 and v2 are closed: unknown fields are rejected at every defined object boundary. Writers emit v2, including exact parameter modulation mappings. Readers accept v1 through a tested deterministic migration and reject unsupported schema or engine versions. Forward compatibility continues to use explicit migrations rather than silently discarding data from a newer author.

## Editable-graph boundary

M3.1 extends schema-v1 persistence to supported created and deleted nodes while requiring exactly one stereo input and output. M3.2 permits arbitrary valid mono branches while rejecting invalid endpoint direction/type, direct self-connections, and multiple cables on one ordinary input. Ports, parameter units/ranges, endpoints, IDs, layout, and viewport are validated before replacement. User-created topology remains a saved draft until graph compilation is implemented; only known Barr-reference identities publish parameter values to the current DSP runtime.

## Verification

Web tests prove deterministic round trips, exact numeric values, exact node positions and viewport values, invalid JSON/version/range rejection, and the unknown-field policy. UI evidence under `artifacts/ui/m2-4-patch-persistence` demonstrates successful save/load restoration and an invalid-load diagnostic with the previously valid patch still visible.
