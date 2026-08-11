# Topology-change crossfades

M5.5 connects the schema-v2 editor document to the native graph host and makes completed audible edits safe to audition. The browser sends only the semantic graph when nodes, cables, or audible values change; layout, selection, and viewport movement do not request compilation. Native parsing is synchronous and bounded to the message thread. Validation, feedback scheduling, delay allocation, and runtime construction remain on the M5.4 compiler worker.

## Transition policy

A valid prepared revision becomes active at an audio-block boundary. When a previous runtime exists, both runtimes process the same input for a fixed **10 milliseconds** and their stereo outputs are mixed with a linear old-to-new ramp. The first sample is almost entirely old output and the final sample is entirely new output. The abandoned runtime is then placed in the worker-owned retirement ring, so its tail cannot persist indefinitely.

Only the old and new runtimes execute during a transition. Both output scratch channels are allocated with the new runtime before publication. The audio callback performs no allocation, deletion, locking, waiting, logging, or topology work. One newer prepared revision may wait while a transition is active; still-newer requests replace it. The next transition begins only after the current one finishes. This bounds callback CPU to two graph renders and prevents rapid edits from growing a queue.

Compilation failure never starts a transition and never removes the preceding audible graph. The diagnostics panel retains requested, pending, active, and failed revisions, the current crossfade sample position, the last completed old/new revision pair, superseded work, reclaimed runtimes, and the prepared delay-memory allocation.

## Native editor binding

The editor debounces semantic changes for 35 milliseconds, serializes the same closed schema-v2 document used by Save Patch, and calls the native `publishGraph` bridge. The header reports compiling, crossfading, or active identity. A successful request is acknowledged immediately; its later compile result remains observable through diagnostics. Invalid native parsing is returned directly, while asynchronous compiler errors appear against their failed revision.

Constructed-graph processing retains master audition gain, manual mute, impulse triggering, finite/runaway guards, live CPU/clipping accounting, and deterministic measurement reset/capture. Capture input muting and the 0.1-peak measurement impulse happen before the graph; capture samples remain pre-gain. Known Barr energy lanes gracefully fall to zero when the constructed runtime cannot yet supply per-node telemetry; general per-node instrumentation remains separate future work.

## Windows scaling correction

WebView2 sizes its native child in physical pixels, while JUCE supplies logical component bounds. At Windows display scales above 100%, that mismatch previously left a white area to the right and below the schematic. The editor now compensates at the WebView boundary using the active display scale. The reviewed 125%-scale evidence shows the schematic occupying the complete area below the intentional 88-pixel native control strip.

## Verification and evidence

Native tests cover the exact ten-sample transition at a 1 kHz fixture rate, newest-wins coalescing during an active transition, finite legal-feedback edits, deterministic reset, off-thread reclamation, and a 1,000-edit concurrent stress run. Web tests cover semantic fingerprinting and strict bridge/diagnostic contracts.

- [`01-live-crossfade-diagnostics.png`](../artifacts/ui/m5-5-topology-crossfade/01-live-crossfade-diagnostics.png) shows the 125%-scaled native standalone filled correctly after a live Left Tap edit, with active revision and retained last-crossfade/reclamation diagnostics.
- [`live-topology-edit.mp4`](../artifacts/ui/m5-5-topology-crossfade/live-topology-edit.mp4) shows the native editor online at 48 kHz, impulse audition around a continuous Left Tap delay edit, revision publication, and the completed transition diagnostic. The capture is visual-only; deterministic native render tests are the authoritative audio evidence.
