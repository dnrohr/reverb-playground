# Macro control source

M8.3 adds Macro as a visible, user-named control source. It is deliberately a
normal graph block rather than a factory-only assignment system: its single
dashed control output branches through ordinary Curve Mapper blocks and then
into visible parameter sockets. The cables remain the complete routing truth.

## Exposed state

Each Macro stores a stable node ID, a user name of 1–64 characters, a current
normalized value in `-1..+1`, a default normalized value in `-1..+1`, and a
Boolean center-detent choice. It has exactly one output named `out` and no
hidden destination list. The editor snaps a continuous value to zero when the
detent is enabled and the requested value is within `0.02` of center.

The current value is runtime automation state. The default and detent are
structural settings. Reset clears the lock-free automation mailbox and restores
the prepared default deterministically.

## Runtime policy

UI or host-state value changes enter a fixed 64-slot lock-free mailbox keyed by
the Macro's stable node ID. At each audio-block boundary the active runtime,
including both sides of a topology crossfade, consumes the latest finite value.
There are no audio-thread strings, allocations, locks, or unbounded searches.

Macro values are sampled by the existing 1 kHz control graph and linearly ramp
to the requested value over exactly 20 control ticks: a fixed **20 ms** policy.
Mapped audio parameters then retain their existing one-control-quantum linear
interpolation. This makes fast gestures bounded and continuous without hiding
the two explicit stages of smoothing.

The audible topology fingerprint intentionally substitutes zero for the
Macro's current `value`. Value edits therefore call the runtime mailbox but do
not request topology compilation. Changing the default or center-detent remains
a document edit and republishes the prepared graph.

## Inspection

Selecting a Macro performs a bounded traversal of dashed control cables. It
follows each ordinary Curve Mapper from input to output, uses that mapper's
documented monotonic endpoint equations, and lists every reachable parameter as
`node ID / parameter ID` with its predicted minimum, maximum, and unit. The
Macro, reachable mapper/destination blocks, and traversed cables receive a
dashed outline or stroke as well as color, so the relationship does not depend
on color perception.

The range is a prediction from saved graph parameters, not measured audio. It
does not claim that the signal visits every point or that the resulting reverb
envelope has a particular shape.

## Persistence and limits

Schema v2 stores the name alongside the Macro's three parameters. Browser
save/load and native host state preserve exact values, node IDs, cable IDs,
positions, and viewport. Rename and parameter changes participate in unified
undo/redo. Copy/paste preserves the name and settings while assigning new,
collision-free block and cable IDs. Existing schema-v1 and schema-v2 documents
without Macro nodes migrate unchanged; the optional node-name field does not
alter their meaning.

A prepared graph accepts at most 64 control-participating nodes and 128 mapped
destinations. Hash-slot collisions between Macros fail compilation rather than
silently sharing automation. Signal-type errors, missing/oversized names,
non-finite or out-of-range values, invalid detent values, occupied parameter
sockets, and over-budget branching all fail before runtime publication.

## Evidence

- Screenshot: `artifacts/ui/m8-3-macro-control/macro-destinations.png`.
- Interaction video: `artifacts/ui/m8-3-macro-control/macro-automation.mp4`.
- Native and browser tests cover the fixed block contract, branching limit,
  invalid routes, rapid runtime-only automation, deterministic reset,
  reachability/range prediction, history, clipboard, save/load, and host-state
  restoration.
