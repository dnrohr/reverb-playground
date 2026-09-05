# Predictable layout and construction

M31 makes layout edits geometric, presentation-only, and reversible. None of
these commands changes a node, port, cable endpoint, parameter, automation
identity, compile schedule, or rendered sample.

## Geometry contract

- **Align left** uses the left edge of each selected bounding box. **Align top**
  uses its top edge.
- **Distribute horizontally/vertically** sorts deterministically by graph-space
  edge and stable ID, fixes the two outer bounding-box edges, and makes the free
  gaps between mixed-size blocks equal. If the fixed span cannot contain the
  blocks without overlap, the command refuses and reports the missing span.
- Stereo I/O is locked out of layout commands. A collapsed group must be
  expanded before its members can be arranged. Nested commands operate on the
  live primitives at that level; a collapsed compound is treated as one parent
  bounding box.
- Every successful command is one document-history edit. Exact coordinates and
  orientation save in schema v2 and restore independently of display scale.

## Bounded group arrangement

**Arrange Group (Bounded)** evaluates every possible column count using measured
block dimensions, 36 graph-pixel free gaps, and a stable top-left anchor. It
chooses the non-colliding result closest to a square aspect ratio, with stable-ID
ordering as its tie breaker. Protected blocks outside the group are never moved.
If no candidate clears them by 24 graph pixels, the command refuses instead of
partially moving or overlapping the group. Repeating a successful arrangement
is idempotent.

## Horizontal presentation flip

**Flip Horizontally** moves input sockets to the right and output sockets to the
left. Text, parameter values, socket shapes, keyboard order, cable arrows, and
semantic source/target direction are not mirrored. The block prints `R→L VIEW`
and its accessible name states that signal direction is unchanged. Orientation
round-trips through patch and host state, clipboard, groups, compound parents,
and undo/redo; old files default to left-to-right.

## Direct creation and context

Palette buttons remain ordinary keyboard and screen-reader buttons: click,
Enter, and touch-capable activation create with the same defaults and stable-ID
allocator as before. A pointer drag adds a labeled preview and converts the
drop's screen coordinates through the active React Flow viewport, so its center
lands at the pointer under pan, zoom, and nested navigation. Cancelled, external,
unknown, or singleton-I/O-invalid drops create no graph or history entry.

Dropping inside a nested schematic adds the created primitives to that live
hierarchy without creating a hidden runtime boundary. Ungroup, collapse,
expand, delete, detach, Back, Undo, and Redo clear projected selections before
choosing a stable surviving primitive, parent, or canvas. The context dock is
returned to Inspect, focus follows the new context, and a status announcement
describes the presentation-only result.
