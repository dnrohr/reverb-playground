# Interaction and state correctness

M30 gives parameter display, temporary audition, and assisted tuning one explicit
state contract. The editable graph remains the saved authority. Runtime-only
layers are visible, named, reversible, and excluded from patch/host persistence
and ordinary WAV export.

## Parameter state layers

Every Inspect parameter card shows its saved base value immediately. When
applicable, the same card also reports the calculated live-modulated value, an
audition-preview value, and a pending-topology badge. These values are not folded
into one ambiguous number. Control preview runs at the existing 30 Hz UI cadence;
native parameter acknowledgements are revision-gated, so a late acknowledgement
cannot overwrite a newer pointer or numeric edit and acknowledgements never emit
another native edit.

The deterministic source matrix covers pointer and numeric edits, control
modulation, Matrix edits, assisted preview, factory load, patch/host restore,
automation/native snapshots, undo/redo, A/B promotion, and topology publication.
Opening Analyze or changing selection is not part of the refresh path.

## Temporary audition

Mute, Isolate, and Bypass are toggle buttons. Pressing the active operation again,
**Clear audition**, or `Escape` restores the exact edited graph and existing
Wet/Dry gains. Selecting another operation switches atomically. Clicking empty
canvas only deselects; it does not change audio. Graph replacement and semantic
edits clear an incompatible preview through the existing publication boundary.

The overlay remains session-only: it is absent from graph history, dirty identity,
clipboard, patch JSON, host state, factory data, and processed-file export.
Measurement continues to disclose when its impulse is capturing an active preview.

## Cumulative assisted tuning

Each Four-Line suggestion lists its exact `node.parameter` set, expected audible
effect, and what it preserves before preview. Suggestions with disjoint parameter
sets are compatible and preview from the current edited graph, so applying a
second compatible suggestion retains the first. An applied suggestion is marked
as a replacement and cannot be silently compounded. The UI uses **Apply tuning**
and **Discard preview**; every application creates a separate `Apply tuning: <id>`
undo transaction.

Discard republishes the byte-equivalent pre-preview graph. Apply turns the preview
into ordinary saved graph state, after which save/reopen and nested editing use the
same values as any direct edit.

## Qualification evidence

The checked record is `artifacts/ui/m30-interaction-state/qualification.json`.
Screenshots show layered parameter state and cumulative tuning on desktop and
compact layouts. `interaction-state-workflow.mp4` covers cross-view parameter
state, active-button toggle, operation switching, and Escape restoration.
