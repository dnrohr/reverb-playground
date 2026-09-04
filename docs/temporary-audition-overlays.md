# Temporary audition overlays

M27.1 adds diagnostic mute, isolate, and block bypass without turning a listening
question into a saved graph edit. The selected block or cable receives an amber
treatment and the canvas always offers **Clear Audition** while an overlay is
active. Pressing the active operation again or pressing `Escape` is equivalent.
Switching to another operation is atomic, while clicking empty canvas only
changes selection and deliberately leaves the audible preview active.

## State boundary

An overlay is editor-session state. It is excluded from patch JSON, plugin host
state, history, semantic/layout dirty identity, clipboard, factories, and WAV
export. Any graph replacement, factory selection, reset, undo/redo, deletion,
or other committed graph transformation clears it. Emergency Mute and the
numerical-safety latch remain downstream and authoritative.

The editor builds a temporary ordinary graph and publishes it through the new
native `previewGraph` path. Preview publication uses the existing off-thread
compiler and bounded topology crossfade but never stores the document or emits
a host-state change. WAV export continues to read the saved host document, so
it cannot accidentally render a mute/isolate/bypass preview. A native test
round-trips host state after a different preview graph and requires the saved
document to remain byte-equivalent.

Impulse capture intentionally measures the active audible preview. While an
overlay is active, the measurement bar says exactly which overlay is included
and changes its action to **Capture with overlay**. Clearing the overlay returns
capture to the authoritative graph.

## Operations

- **Mute cable** omits one selected mono audio connection from the preview.
- **Mute output** omits a selected block's outgoing audio cables while retaining
  unrelated control routing.
- **Isolate branch** retains the selected cable plus only its upstream and
  downstream path segments. Parallel paths that bypass the selected cable are
  excluded.
- **Isolate paths** retains only upstream/downstream segments that pass through
  the selected block.
- **Bypass block** is available only for a non-I/O block with exactly one
  connected mono audio input and at least one audio output. The preview removes
  the block and all incident cables, then creates visibly temporary direct
  connections from that input source to the former output destinations.

Control cables and required Stereo I/O are refused. Ambiguous multi-input
bypass is refused rather than choosing a source. Before publication, bypass
checks the resulting delay-free graph; if removing a Delay or other block would
create an algebraic cycle, the overlay is refused with that reason. Accepted
bypass explicitly warns that compiled latency may change. Group/subpatch
presentation metadata is detached only inside the preview if its member is
removed; the authoritative saved graph remains intact.

## Evidence

- `artifacts/ui/m27-1-audition-overlays/cable-mute.png`
- `artifacts/ui/m27-1-audition-overlays/block-bypass.png`
- `artifacts/ui/m27-1-audition-overlays/audition-overlay-workflow.mp4`
- `artifacts/ui/m27-1-audition-overlays/audition-640x400.png`
