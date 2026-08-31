# A/B comparison snapshots

M27.2 replaces the old Barr-versus-factory shortcut with two explicit session
snapshots. **Capture A** and **Capture B** copy the complete editable graph,
Macro values, processing quality, viewport, Wet/Dry audition gains, measured
compiled latency, label, and—when it belongs to that exact semantic graph—the
latest isolated impulse response. A and B are always named and visible; neither
is a hidden mutable buffer.

Snapshots are temporary comparison evidence. They are excluded from patch JSON,
plugin host state, factory identity, clipboard, edit history, and WAV export.
Switching calls the preview-only graph publisher, so compilation remains
off-thread and the existing bounded topology crossfade, feedback validator,
latency reporting, Emergency Mute, and numerical-safety latch remain in force.
The active slot, mode, measured adjustment, graph differences, gain differences,
and B-minus-A latency are disclosed in the panel.

## Raw and matched listening

**Raw** applies no compensation. Each snapshot retains its captured Wet/Dry
audition gains, and the comparison-only output multiplier is exactly unity.

**Matched** is available only when both snapshots include an impulse captured
from their exact semantic graph. The bounded probe computes stereo RMS and peak
from the finite capture. It refuses fewer than 128 frames, silence, any
non-finite sample, or a peak at or above unity. The quieter measured response is
the target; the louder response is attenuated and the quieter response is never
boosted. The panel reports both adjustments, target RMS, frame counts, and
capture generations.

The comparison multiplier and captured Wet/Dry overrides are smoothed over the
same 10 ms boundary as the normal audition gains. They exist after the graph and
outside persistent Wet/Dry state, so matching cannot alter a saved project or
processed WAV export. Native qualification verifies audible attenuation and
that serialization restores unity comparison gain.

## Revert and promote

**Revert to edited graph** exits comparison and republishes the untouched graph
that was open before auditioning A or B. Any ordinary graph edit also exits the
preview rather than editing a hidden slot.

**Promote active** copies the selected snapshot into the editor as one explicit
graph-history transaction, restores its viewport and processing quality, and
marks the result Custom. Undo returns to the previous edited graph. Promotion is
the only operation that changes exported graph semantics.

## Evidence

- `artifacts/ui/m27-2-ab-comparison/ab-snapshots.png`
- `artifacts/ui/m27-2-ab-comparison/ab-raw-refusal.png`
- `artifacts/ui/m27-2-ab-comparison/ab-snapshot-workflow.mp4`
- `artifacts/ui/m27-2-ab-comparison/ab-640x400.png`
