# Large-graph routing and layout

M26.4 adds layout tools for dense patches without adding executable objects or
changing cable semantics. Shift-select ordinary blocks and use the Edit menu to
align their left or top edges, distribute three or more blocks horizontally or
vertically, or arrange the members of a selected visual group on a stable grid.
Each command is one Undo/Redo operation.

## Cable waypoints

Select a cable and open Inspect. **Add Waypoint** inserts a point into the
longest current segment; its exact graph-space X/Y coordinates remain editable.
Up to 32 points are saved in order. Clearing them restores the ordinary smooth
route. Waypoints belong to `layout.cables`, are offset with a copied subgraph,
and do not participate in the semantic graph hash, compilation, latency,
feedback analysis, Energy, automation, or audio publication.

## Paired routing portals

A selected long cable can become one named portal pair. The source endpoint
ends at a visible `NAME →` marker and the destination begins at a visible
`→ NAME` marker. Both markers are projections of one real cable ID, so they
cannot be mismatched, cross documents, introduce fan-in, or bypass connection
validation. Selecting either rendered endpoint selects that cable, reveals its
complete route, and exposes source, destination, signal type, and direction in
the inspector. A portal never hides feedback evidence: loop decoration is
computed from the authoritative edge before presentation.

## Trace and focus

The cable inspector can trace the complete directed graph toward sources or
toward outputs. The cyan focus is temporary presentation state and clears from
the canvas; it is not saved. **Focus Complete Loop** uses the existing bounded
feedback-loop result and frames every primitive in the selected loop. These
actions neither edit nor republish the graph.

## Persistence and limits

Schema v2 optionally stores one `layout.cables` entry per routed semantic
connection. Native and browser readers reject unknown or duplicate cable IDs,
non-finite coordinates, more than 32 waypoints, empty metadata, and portal
names outside 1–32 characters. Documents without cable layout retain their
previous deterministic bytes.

## Evidence

- `artifacts/ui/m26-4-large-graph-routing/four-line-waypoint-layout.png`
- `artifacts/ui/m26-4-large-graph-routing/reverse-cosmic-portals.png`
- `artifacts/ui/m26-4-large-graph-routing/routing-focus-workflow.mp4`
- `artifacts/ui/m26-4-large-graph-routing/routing-640x400.png`
