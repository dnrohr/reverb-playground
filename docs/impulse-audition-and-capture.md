# Impulse audition and capture

M4.2 adds a controlled measurement path for the runtime-bound Barr reference graph. The editor's **Measure / Impulse Response** strip exposes the maximum capture length, silence stop threshold, and **Mute live input** policy. **Capture impulse** resets the unmodulated runtime to its current parameter targets, injects a `0.1`-peak test impulse at the documented normalized-sum boundary, and records the raw stereo wet response before master audition gain.

## Safety and bounds

- Test impulse level is fixed at `0.1` peak and clamped by the native engine to at most `0.25`.
- Maximum length is selectable from 500, 2,000, 5,000, or 10,000 milliseconds. The native boundary clamps arbitrary callers to 100-10,000 milliseconds.
- Stop threshold is selectable from -60, -80, -100, or -120 dBFS and clamped to -24 through -120 dBFS.
- Threshold stopping requires the response to cross the threshold first, followed by 100 continuous milliseconds below it. Predelay silence therefore cannot terminate a capture before the response arrives.
- **Mute live input** suppresses both hardware input channels during the measurement without changing the saved graph. Emergency mute can keep the audible output silent while the internal measurement proceeds.
- Capture data is taken before master audition gain, so changing that safety/listening control does not change analysis samples.

## Real-time boundary

`ImpulseCapture::prepare` allocates three fixed stereo slots outside processing, bounded at ten seconds and 192 kHz. During processing, the audio thread only resets prepared DSP state, writes samples into the current slot, updates counters, and publishes a completed slot index with atomics. It performs no allocation, locking, filesystem work, JSON encoding, or UI callback.

The message thread copies only a completed immutable slot and serializes capture format version 1 for the web editor. The payload contains sample rate, frame count, applied bounds, stop reason, impulse level, live-input policy, and equal-length finite left/right arrays. This contract is the input for M4.3's waveform and decay visualization.

## Determinism

Starting a measurement clears delay/filter state and settles all smoothed parameters to their current targets. Repeating a capture without modulation or parameter changes therefore produces sample-identical left and right arrays, independent of prior live input and master audition gain. Tests exercise repeated captures with different input buffers and audition gains.

## Evidence

Reviewed UI evidence is stored in `artifacts/ui/m4-2-impulse-audition-capture/`. The screenshot shows the visible measurement bounds and completed result. The video shows a capture progressing to its bounded result while live input is muted.
