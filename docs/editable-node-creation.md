# Editable node creation and deletion

M3.1 turns the module library into the first structural editing surface. Clicking a library item creates a stable, schema-backed block near the viewport center. The primitives are Stereo Input, Stereo Output, Gain / Invert, Sum (`+`), Delay, Allpass, and Low-pass.

IDs use a readable type-plus-number form such as `delay-1` and survive editing, undo, save, and reload. Each block copies ports, parameter ranges, units, and safe initial values from one module registry. Delay values use milliseconds. Gain spans `-1..1`, making inversion explicit without a separate subtraction primitive.

The patch must contain exactly one Stereo Input and one Stereo Output. The editor rejects duplicate I/O creation and deletion of either required block with an actionable message. The loader independently enforces the same invariant before replacing visible state.

Deleting a selection computes the resulting nodes and cables together, so incident cables disappear in the same transaction. **Undo Structure** restores the exact preceding node/cable snapshot; redo reapplies it. Parameter undo remains a separate inspector history for this milestone.

Reference blocks remain bound to the current native Barr DSP. Newly created blocks are visibly labeled as draft in the inspector: their parameters and topology save correctly, but they are not compiled into the audible runtime yet. Typed cable editing and compilation are M3.2 and M3.3 work.

Verification includes web tests for every primitive and safe default, collision-free IDs, atomic incident-cable deletion, exact structural undo, required I/O, and created-node persistence.

- [All primitive appearances](../artifacts/ui/m3-1-editable-node-creation/all-primitives.jpg)
- [Create/delete/undo interaction](../artifacts/ui/m3-1-editable-node-creation/create-delete-undo.mp4)
