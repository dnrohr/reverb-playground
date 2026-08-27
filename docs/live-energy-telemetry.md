# Live energy telemetry

M4.4 connects measured DSP activity to the schematic without exposing audio-thread storage to the web editor. It instruments the ten runtime-bound Barr nodes and derives each cable's presentation level from its source node.

## Real-time contract

The fixed Barr harness owns ten fixed RMS lanes. Editable compiled graphs own a
bounded lane for each prepared audio operation, keyed to that graph's stable node
ID. When enabled, the audible active runtime peak-holds block RMS until a 30 Hz
publication boundary, then publishes one coherent snapshot with the active graph
revision. During topology crossfades only the incoming, newly audible revision is
published; the UI discards any frame whose revision no longer matches diagnostics.

The audio callback does not allocate, lock, serialize, invoke UI code, or wait for a reader. The message thread performs a bounded snapshot retry and creates JSON only from copied atomic values. Polling late, skipping generations, or never polling cannot influence DSP state or samples; native tests compare polled and ignored renders sample-for-sample.

The disabled audio path performs one relaxed atomic flag read per block and skips every RMS scan and publication. The web editor also removes its polling timer and clears decorations. Tests verify that the disabled path inspects and publishes zero sample values.

## Visual mapping

Linear RMS is mapped over a `-72..0 dBFS` display range. The UI applies a 42 millisecond attack and 260 millisecond release independently from audio processing. A 100 millisecond generation timeout releases a stale glow toward zero if the audio device stops; dropped intermediate generations simply mean the next coherent measurement replaces them.

Each block has a five-segment activity meter as a non-colour intensity cue. Increasing cable energy also increases stroke width. Cyan glow is deliberately subordinate to selected-loop amber styling, so live behavior does not erase topology inspection.

## Accessibility and control

**Energy On/Off** is an explicit header control. Turning it off stops UI polling
and takes a single disabled branch in the audio callback: no buffers are scanned,
no RMS values are calculated, and no telemetry atomics are published. If the
operating system requests reduced motion, energy animation starts disabled and
the toggle reports **Energy Reduced**. Telemetry is presentation-only and is not
serialized.

Reviewed Release evidence is in `artifacts/ui/m18-5-audition-truth/compiled-energy.mp4`.
It triggers the audible impulse through the active 58-block compiled graph and
shows node/cable energy advance through that revision.

## Scope note

The current runtime is the fixed Barr development reference. Its Allpass stages contain local recirculating state, so an impulse remains visible as it diffuses and decays even though the visible M2 reference graph intentionally omits an outer feedback cable. General compiled-graph telemetry can reuse the same fixed snapshot contract when editable graph publication replaces the fixed harness.
