# Typed connection editing

M3.2 makes mono audio cables editable. Drag from an output handle to an input handle to connect. A single output may branch to any number of inputs, while each ordinary input accepts one cable.

## Validation and occupied inputs

The editor resolves both endpoint ports before changing the graph. Missing ports, input-to-output reversal, and signal-type mismatch are rejected. The schema-v1 loader applies the same endpoint checks and also rejects files containing two cables on one input. Cycle legality is deliberately left to the M3.3/M3.4 graph compilers, where delay-containing feedback can be distinguished from algebraic loops.

Dropping a second cable on an occupied audio input does not silently replace or sum it. The editor first draws the proposed ordinary Sum and all three replacement cables in dashed amber. This is a presentation-only preview: it is not saved, published to the audio runtime, or entered into history. A dialog offers three explicit outcomes:

- **Confirm Insert +** creates the previewed Sum block, moves the old source to `in-a`, sends the new source to `in-b`, and connects the Sum output to the original target.
- **Replace Cable** removes the old cable and connects the new source.
- **Cancel** leaves the graph untouched.

Create, replace, automatic Sum insertion, and cable deletion are structural history transactions. **Undo Structure** restores the exact preceding nodes and edges; redo reapplies the transaction.

The proposal receives the same deterministic ID, position, and cable IDs as confirmation. `Enter` confirms the focused Insert action and `Escape` cancels from anywhere in the dialog. A control socket never offers audio Sum insertion; its focused default action is Replace Cable. Cancel never writes graph state, so the serialized document remains byte-identical.

## Interaction and runtime boundary

Audio cables are solid and carry one mono signal. Their visible stroke remains narrow, but each edge has a 24-unit interaction target, verified by selecting a cable at the editor's 40% minimum zoom. Control cables retain the dashed style and type boundary for later modulation modules.

These edits update and persist the draft semantic graph. Compiling arbitrary topology into an immutable audio runtime begins in M3.3.

## Evidence

- [Occupied-input resolution screenshot](../artifacts/ui/m3-2-typed-connections/02-occupied-offer.jpg)
- [Typed connection interaction video](../artifacts/ui/m3-2-typed-connections/typed-connection-editing.mp4)
- [Assisted Sum preview screenshot](../artifacts/ui/m26-1-assisted-sum/assisted-sum-preview.png)
- [Keyboard cancel and confirm workflow](../artifacts/ui/m26-1-assisted-sum/assisted-sum-keyboard-workflow.mp4)
