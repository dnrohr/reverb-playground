# Collapsible graph groups

M26.2 adds named visual groups for navigating dense primitive graphs. Select at
least two non-I/O blocks with Shift box selection, then choose **Edit → Group
Selection** or press `Ctrl+G`. Name the group, select any outlined member, and
use **Collapse Group** in the inspector. Selecting the violet boundary exposes
rename, expand, loop-reveal, and ungroup actions.

## Authority and audio identity

A group is layout metadata, not a module. The authoritative node and cable
arrays are never replaced by the boundary. Collapse is a render projection that
hides member blocks and internal cables, creates one temporary boundary, and
maps each crossing cable to its own typed input or output port. It performs no
mixing, normalization, parameter exposure, automation remapping, compilation,
or runtime publication. Semantic graph hashes therefore remain identical while
document hashes track group creation, naming, and independent collapse state.

Energy and loop decoration are calculated on the complete primitive graph
before projection. If a hidden member participates in the selected feedback
loop, the boundary carries the loop highlight. **Reveal Complete Loop** expands
the group so the full primitive path is visible without changing the selected
loop's executable meaning.

## Persistence, clipboard, and history

Schema-v2 `layout.groups` is optional. Each entry stores a stable group ID,
name, collapse state, and at least two member node IDs. Both browser and native
host-state readers preserve it; older v1/v2 documents without the field migrate
to an empty group list. Validation rejects unknown members, duplicate group
IDs, I/O members, and any node assigned to more than one group.

Create, rename, collapse, expand, paste, and ungroup are ordinary atomic
Undo/Redo operations. Copying every member reproduces the group under a fresh
group ID; copying only part of a group intentionally strips membership from the
partial copy. Loading or replacing the graph replaces its group layout at the
same atomic boundary as its primitive graph.

Nested groups are deliberately unsupported in M26.2. The editor rejects them
with an explicit message rather than persisting a partially defined hierarchy.

## Evidence

- [Collapsed group and inspector](../artifacts/ui/m26-2-collapsible-groups/collapsed-group-inspector.png)
- [Minimum 640×400 layout](../artifacts/ui/m26-2-collapsible-groups/collapsed-group-640x400.png)
- [Collapse and expand workflow](../artifacts/ui/m26-2-collapsible-groups/group-collapse-expand.mp4)
