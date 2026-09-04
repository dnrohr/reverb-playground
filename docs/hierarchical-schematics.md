# Hierarchical schematics

M29 makes dense structures compact without changing what the reverb executes.
The parent block is a saved navigation and routing view; the blocks and cables
inside it remain the only DSP, automation, latency, feedback, Energy, safety,
audition, export, and host-state authority.

## Three different organization tools

| Tool | Purpose | Saved authority | Audio effect |
|---|---|---|---|
| Visual group | Organize blocks on the current canvas | Group name, bounds, members, and collapsed presentation | None |
| Compound | Recognize a dense arrangement of existing primitives | Stable hierarchy ID, boundary map, parent position, and nested viewport | None beyond its primitives |
| Reusable subpatch | Place a version-pinned recipe as ordinary primitives | Definition provenance plus the same hierarchy presentation | None beyond its primitives |

They deliberately do not convert into one another. Ungroup removes layout
organization. Detach removes reusable-definition provenance. Deleting a
compound or subpatch parent instead removes its owned primitives and every
incident cable as one undoable action.

## Parent and boundary contract

Every hierarchy owns a stable ID, display name, member-node IDs, saved parent
position, independently saved nested viewport, and named ports. A port maps to
one or more explicit internal primitive sockets of the same signal type and
direction. The 4×4 Matrix Mixer therefore presents four mono inputs and four
mono outputs while retaining its 16 Gain inputs, 12 Sum blocks, and 20 crossing
cables underneath.

One Matrix input represents four explicit fan-out cables from the same external
source. Deleting or reconnecting that parent cable expands into one atomic edit
of those four cables; no hidden fan-out or summing appears in DSP. Output ports
remain one-to-one. Parent cables are ordinary solid audio cables and follow the
movable parent block, so collapse no longer leaves a dashed internal boundary
or reserves the internal layout's dimensions.

Validation runs before save, host-state publication, or compilation. It rejects
duplicate ownership, missing members, dangling or signal-mismatched sockets,
unmapped crossing cables, divergent sources behind a shared input, missing
parents, recursive parents, and parent links beyond M29's one opened nested
level. Error
messages name the `layout.hierarchies` path, hierarchy, port, node, or cable that
must be repaired.

## Live nested editing

Double-click a parent block or choose **Open schematic** in Inspect. M29
supports one opened nested level; compounds inside compounds are rejected until
recursive navigation has its own interaction and ownership qualification. The canvas
then shows the exact owned primitives plus stable Input and Output boundary
blocks. The breadcrumb names the current level; **Back to patch** and
`Alt+Left` return to the parent. Entering or leaving a level is navigation only:
there is no Apply, Cancel, reconstruction, or implicit history entry.

Parameter changes inside the nested canvas use the existing continuous edit
path. Topology changes use the same worker compilation and short runtime
crossfade as parent-level edits. Undo and redo remain one history across both
levels. The current selection is retained where meaningful, the parent regains
context on Back, and each nested level restores its own viewport.

The parent inspector derives Energy, feedback-loop membership, delay, polarity,
filter, warning, latency, memory, and safety context from its members. Opening
the nested schematic reveals the exact contributing primitive or cable.
Temporary audition and A/B state remain explicit overlays over the same
semantic graph; save/load, offline export, impulse capture, plugin automation,
and host restore never execute the projected parent or boundary blocks.

## Persistence and compatibility

Schema v2 stores hierarchy presentation in optional `layout.hierarchies` data.
The native graph document parses, validates, preserves, and writes that layout
alongside the existing semantic graph. Because it is layout-only, semantic
hashes and rendered samples are unchanged by collapse, rename, parent movement,
or nested viewport movement. Document hashes include those presentation edits
so save state and undo remain honest.

Older Matrix Mixer patches and reusable-subpatch instances have no hierarchy
entry. On load, the editor recognizes their complete primitive membership and
materializes the same deterministic IDs, names, port bindings, positions, and
viewports. Partial or structurally changed matrices remain ordinary primitives;
the migration never invents a DSP node or normalizes coefficients. A complete
copy receives fresh hierarchy, member, cable, boundary, and subpatch-instance
IDs. Partial copies intentionally detach from the hierarchy.

## Qualified Matrix workflow

1. Load **Four-Line Dense Room** and choose **Inspect Matrix**.
2. Move, rename, connect, copy, or delete the compact four-input/four-output
   parent as one coherent object.
3. Double-click it to inspect the 16 signed Gain coefficients and 12 explicit
   Sums behind the eight stable boundary blocks.
4. Edit a Gain, use temporary audition or Analyze, and undo normally; changes
   are immediately part of the live authoritative graph.
5. Choose **Back to patch**, reconnect a parent port if needed, and reopen the
   Matrix without losing the nested viewport or context.

Current desktop and compact screenshots plus the interaction walkthrough are
stored in
[`artifacts/ui/m29-hierarchical-compounds`](../artifacts/ui/m29-hierarchical-compounds/).
