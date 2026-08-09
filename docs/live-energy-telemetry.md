# Live energy telemetry

M4.4 connects measured DSP activity to the schematic without exposing audio-thread storage to the web editor. It instruments the ten runtime-bound Barr nodes and derives each cable's presentation level from its source node.

## Real-time contract

The prepared `EnergyTelemetry` object owns ten fixed atomic RMS lanes and fixed audio-thread scratch arrays. When enabled, each Barr processing block measures stereo input/output and the output of Sum, Low-pass, and the six Allpass stages. It peak-holds block RMS values until a 30 Hz publication boundary, then publishes one coherent snapshot through an atomic sequence lock.

The audio callback does not allocate, lock, serialize, invoke UI code, or wait for a reader. The message thread performs a bounded snapshot retry and creates JSON only from copied atomic values. Polling late, skipping generations, or never polling cannot influence DSP state or samples; native tests compare polled and ignored renders sample-for-sample.

The disabled audio path performs one relaxed atomic flag read per block and skips every RMS scan and publication. The web editor also removes its polling timer and clears decorations. Tests verify that the disabled path inspects and publishes zero sample values.

## Visual mapping

Linear RMS is mapped over a `-72..0 dBFS` display range. The UI applies a 42 millisecond attack and 260 millisecond release independently from audio processing. A 100 millisecond generation timeout releases a stale glow toward zero if the audio device stops; dropped intermediate generations simply mean the next coherent measurement replaces them.

Each block has a five-segment activity meter as a non-colour intensity cue. Increasing cable energy also increases stroke width. Cyan glow is deliberately subordinate to selected-loop amber styling, so live behavior does not erase topology inspection.

## Accessibility and control

**Energy On/Off** is an explicit header control. Turning it off stops both native measurement and UI polling. If the operating system requests reduced motion, energy animation starts disabled, the toggle reports **Energy Reduced**, and the ordinary global reduced-motion CSS removes residual transitions. Graph editing, audio, measurement, and saved state are unaffected; telemetry is presentation-only and is not serialized.

## Scope note

The current runtime is the fixed Barr development reference. Its Allpass stages contain local recirculating state, so an impulse remains visible as it diffuses and decays even though the visible M2 reference graph intentionally omits an outer feedback cable. General compiled-graph telemetry can reuse the same fixed snapshot contract when editable graph publication replaces the fixed harness.
