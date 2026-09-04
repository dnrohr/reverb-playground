# Reusable subpatches

> M29 now places each reusable instance as a compact parent that opens into the
> same authoritative primitives described below. See
> [Hierarchical schematics](hierarchical-schematics.md) for navigation, proxy
> ports, copy/paste, persistence, nesting, and migration behavior.

M26.3 introduces reusable construction without making a new hidden DSP block.
The first definition, **Diffuse Delay v1**, places one All-pass followed by one
Delay. Every instance opens into those ordinary primitives;
the graph compiler, Energy telemetry, latency analysis, feedback validation,
diagnostics, automation, and rendering receive the same document they would
receive if the two blocks and cable had been created manually.

## Ownership and identity

A definition is an immutable, application-owned recipe identified by a
namespaced ID and positive version. An instance has its own stable ID and stores
the definition ID, pinned version, display name, complete primitive member IDs,
and explicit named mono audio/control port bindings. Primitive node and cable
IDs remain the editable, saved, automated, and executable authority.

This is **linked provenance with pinned copy semantics**:

- placing an instance copies the selected definition version into ordinary
  blocks and records where it came from;
- editing a member changes only that instance and never changes the definition
  or another instance;
- a newer registered definition is reported as an available update, but the
  saved instance remains pinned until a future explicit, undoable upgrade
  operation is accepted;
- detaching removes only provenance and the teal instance outline. It is one
  undoable history action and does not alter blocks, cables, parameters, audio,
  latency, or safety.

No parameters are promoted in v1. Users edit the visible All-pass and Delay
parameters directly. A future exposed parameter must use a stable definition
path, name its primitive destination and mapping, and remain an ordinary saved
parameter after deterministic expansion. It may not introduce hidden
normalization or a second automation identity.

## Ports, feedback, and compilation

The Diffuse Delay declares `in` as the All-pass `in` socket and `out` as the
Delay `out` socket. These bindings are descriptive and validated against the
real primitive port signal type and direction. Users connect to those visible
primitive sockets; there are no proxy cables or implicit fan-in.

Feedback may cross an instance boundary because the boundary is metadata over
the actual directed graph. Zero-delay-cycle rejection, delayed-loop legality,
loop highlighting, memory budgets, failure recovery, and Emergency Mute are
therefore unchanged. Native equivalence tests compile and render the same graph
with and without subpatch provenance and require identical schedules, memory,
latency, diagnostics counts, and samples.

Definitions may contain primitives only. A definition cannot instantiate
itself or another definition in M26.3, so recursion is rejected structurally
rather than discovered during audio compilation. If user-authored definitions
are added later, registration must first prove an acyclic dependency graph and
report the complete recursion path.

## Persistence, clipboard, and recovery

Instances are optional `layout.subpatches` entries in schema v2. This keeps old
schema-v2 patches readable while the closed schema validates IDs, versions,
members, and every explicit port. The native graph document preserves the same
metadata through plugin host state. Factory provenance uses the definition ID
and version; the expanded primitives remain sufficient to reproduce the sound.

Copying all instance members and pasting them creates fresh member, cable,
binding, and instance identities. A partial copy is ordinary detached graph
material; it cannot claim to be a complete instance. Undo/redo snapshots include
the instance metadata, and definition placement or detachment is one action.

If a registered definition is missing or a version is no longer available, the
editor reports that state but retains the saved primitives. The patch remains
audible, editable, exportable, and recoverable. Nothing is regenerated or
silently substituted. Unknown future fields remain rejected by the closed
schema; older documents without `layout.subpatches` load unchanged.

## Initial workflow

1. Choose **Diffuse Delay** under **Subpatches** in the module palette.
2. Connect its visible All-pass input and Delay output like ordinary mono ports.
3. Select the compact parent to inspect definition, version, instance identity,
   explicit port mappings, and recovery/update status; double-click it to open
   the two authoritative blocks.
4. Edit the ordinary parameters independently, copy/paste the parent, or choose
   **Detach to ordinary blocks**.

## Evidence

- `artifacts/ui/m26-3-reusable-subpatches/diffuse-delay-instance.png`
- `artifacts/ui/m26-3-reusable-subpatches/subpatch-inspector.png`
- `artifacts/ui/m26-3-reusable-subpatches/subpatch-create-copy-detach.mp4`
- `artifacts/ui/m26-3-reusable-subpatches/subpatch-640x400.png`
