# Runtime resource and safety diagnostics

M4.5 exposes the fixed Barr runtime's resource cost and numerical-safety state without handing live DSP objects to the editor. The diagnostics panel separates four evidence bases in its labels and serialized contract:

- **Estimate / static** is a topology-derived scalar-operation count per sample and per second. It is a comparative workload estimate, not a measured CPU percentage.
- **Live / measured** is the exponentially smoothed ratio of audio-callback processing time to the current block deadline, plus the highest observed ratio and processed-block count.
- **Memory / prepared** is the exact allocation owned by the six prepared Allpass delay lines, not a TypeScript reconstruction or whole-process memory estimate.
- **Clipping / measured** counts output samples above unity and the blocks containing them.

## Snapshot boundary

The audio callback writes only always-lock-free scalar atomics. Live timing uses a monotonic counter around the bounded callback. Safety-event fields use an atomic sequence lock so kind, channel, sample index, and graph revision are copied coherently. The processor creates JSON on the message thread from that value snapshot; the browser never receives a pointer, span, mutable buffer, or reference to audio-thread state.

The fixed operation estimate is 48 scalar operations per sample for the current ten-node Barr path. It intentionally excludes browser, host, driver, SIMD, cache, and platform effects. The measured percentage is the authoritative observation of this process on the current device and block size.

## Revision-bound safety events

Runtime revision 1 is the prepared reference. Each accepted parameter value change advances the native revision. If either output contains NaN/infinity or exceeds the runaway ceiling, the guard zeros both output channels, latches safety mute, and records the exact active revision. Later parameter edits and Undo can advance the active revision, but the event's revision remains unchanged.

Manual emergency mute and numerical safety mute are reported independently. The diagnostics panel opens automatically for a safety latch and keeps graph editing, parameter controls, and **Undo Last Edit** available. The native master audition gain also remains editable. Recovery is deliberately two-step:

1. Undo or reduce the risky gain/parameter while muted.
2. Choose **Recover Audio** (or the native **Reset Safety** control) to clear DSP state and the latch explicitly.

The last event remains visible after recovery as an audit fact; the recovery counter advances. It is not silently reassigned to the newer graph revision.

The embedded WebView receives the editor's full remaining logical bounds. JUCE converts those logical bounds to WebView2 controller pixels using the active monitor scale. This keeps the schematic flush with every edge of the available editor area at 125% and other non-default scales.

## Verification

Native tests drive NaN, positive infinity, and runaway finite input through the complete `LiveReferenceHarness` path. They require silence, the correct violation type, coherent channel/sample/revision identity, editable revisions while latched, and explicit recovery before audio resumes. Separate tests cover prepared memory, estimated-versus-live fields, measured clipping counters, and finite timing. Web tests reject inconsistent mute state, non-finite values, and incoherent event exposure.
