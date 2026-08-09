# Unified graph history and clipboard

M3.6 replaces separate structural and parameter stacks with one bounded document history. Node creation/deletion, cable creation/deletion/replacement, automatic Sum insertion, node movement, parameter commits, paste, and reference reset all store the complete graph state before and after one user-level transaction.

## Undo and redo

The editor exposes one **Undo** and **Redo** pair in the canvas toolbar and mirrors it in the selected-block inspector. `Ctrl/Cmd+Z`, `Ctrl/Cmd+Shift+Z`, and `Ctrl/Cmd+Y` traverse the same stack. Undoing or redoing a state republishes the restored parameters for runtime-bound reference nodes; draft topology remains outside the audible runtime until the later general-runtime binding.

Intermediate slider values remain audible and visible during a drag, but pointer release commits one graph transaction. Inserting an explicit Sum into an occupied input stores the new node and all three rewired cables as one transaction. Node movement commits on drag release, rather than recording every pointer event.

History holds at most **100 transactions and 8 MiB of serialized snapshots**, whichever limit is reached first. Oldest transactions fall away when either limit is exceeded. Redo is cleared by any new committed edit. Selection, focus, measured dimensions, and other React Flow presentation details are excluded from document identity and do not create history entries.

## Identity and clean state

Two deterministic FNV-1a hashes serve different purposes:

- the semantic hash covers sorted node identities, types, ports, parameter values/units, and connection endpoints, but not layout;
- the document hash adds node coordinates and drives the clean marker.

The toolbar displays **Saved** when the current document hash matches the clean marker and **Unsaved** otherwise. A successful save moves the marker to the current hash without clearing undo or redo and without mutating graph semantics. Undoing away from the saved state becomes dirty; redoing exactly back to it becomes clean. A successful load starts a new clean baseline with empty history because it replaces the document rather than editing it.

## Subgraph clipboard

**Copy** or `Ctrl/Cmd+C` captures selected non-I/O blocks and only cables whose two endpoints are both selected. Stereo Input and Stereo Output are omitted so paste cannot violate the exactly-one-I/O invariant. **Paste** or `Ctrl/Cmd+V`:

- assigns collision-free node and cable IDs;
- preserves module type, ports, parameter values, units, and internal connections;
- converts copied runtime-bound blocks into editable draft copies;
- offsets each successive paste so copies remain discoverable;
- records the whole pasted subgraph as one undo transaction.

The editor also writes a version-labelled JSON representation to the browser clipboard when permission is available, while its session-local graph clipboard remains authoritative for deterministic in-editor paste. External clipboard import is intentionally deferred until a separately versioned and validated subgraph schema exists.

## Verification and evidence

Web tests traverse semantic hashes through mixed layout, parameter, connection, and node edits; prove the 100-entry bound and saved marker; confirm one-step Sum insertion; verify save serialization is non-mutating; and verify copied parameters/internal cables receive fresh IDs on paste.

- Screenshot: [`artifacts/ui/m3-6-unified-history/clipboard-paste.png`](../artifacts/ui/m3-6-unified-history/clipboard-paste.png)
- Copy/paste/undo/redo video: [`artifacts/ui/m3-6-unified-history/copy-paste-undo-redo.mp4`](../artifacts/ui/m3-6-unified-history/copy-paste-undo-redo.mp4)
