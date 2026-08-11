# Visualization teaching overlays and A/B comparison

M6.4 adds an optional explanatory layer to the existing stereo impulse/decay viewer. The waveform and Schroeder decay remain measured evidence; teaching regions are annotations over that evidence and never alter audio, capture samples, zoom, RT60, or the saved patch.

## Measured landmarks

The browser repeats the M6.3 envelope contract using non-overlapping 10 ms windows of summed left/right squared energy. It finds:

- onset: first window above `peak * 1e-8`;
- peak: centre of the maximum-energy window;
- cutoff: first post-peak window at or below `peak * 1e-4` (-40 dB).

Regions are clipped to the current zoom/pan window. Markers outside that viewport disappear rather than being pinned to a misleading edge.

For **Causal Reverse Envelope**, the viewer shades onset through the measured late peak as **Rising Energy**, then marks **Late Peak** and the -40 dB crossing. Its explanation states that visible weighted delays construct a causal swell: sample order is not reversed and wet sound cannot precede the trigger.

For **Level-Gated Room**, the viewer labels **Gate Open**, **Hold**, and **Release** using the captured graph's visible Envelope Follower release and Hold Gate hold/release millisecond values. **Cutoff** remains a measured audio landmark rather than an assumed parameter boundary. The explanation identifies level-triggered retrigger behavior, so the overlay is not confused with a fixed time window. Even if a -5 to -35 dB regression can be calculated before truncation, the viewer labels RT60 **Not Meaningful** and explains that the abrupt gate invalidates an exponential extrapolation.

Barr/reference, custom, and silent captures receive no architecture-specific overlay. The generic stereo waveform, decay, metrics, and RT60/refusal behavior remain available.

## Learn control

The existing **Learn On/Off** preference now controls both inspector teaching cards and response architecture overlays. It persists in local editor preferences. Turning it off removes region graphics and explanatory prose while retaining the unannotated capture. This gives experienced users an evidence-only view and keeps all explanations available offline in this documentation.

## A/B workflow

The header exposes two explicit audition targets:

- **A / Barr** loads the native Barr reference.
- **B / Reverse Env** or **B / Gated** loads the last selected teaching design.

Selecting either teaching design from the Factory Patch menu updates B; returning to A does not forget it. Both buttons use ordinary schema loading, off-thread compilation, and the existing 10 ms topology crossfade. There is no second hidden DSP engine and no level-matched crossfade between stored recordings: A/B means rapid live graph replacement under the same input, device, and master audition gain.

The active button uses filled styling plus `aria-pressed`, so state is not communicated by colour alone. Factory selection, graph revision status, and graph title provide redundant textual confirmation.

## Evidence boundary

Overlay timing explains the factory architecture but is not a claim about historical MIDIVerb program internals. M6.3 offline measurements and native multirate tests remain authoritative for the factory audio contracts. The response viewer is authoritative for the particular live capture displayed.

Reviewed native evidence at 125% Windows scaling:

- [`01-reverse-rise-peak-overlay.png`](../artifacts/ui/m6-4-teaching-overlays/01-reverse-rise-peak-overlay.png) shows the measured rise, late peak, -40 dB marker, causal explanation, and meaningful gradual-tail estimate.
- [`02-gated-hold-cutoff-overlay.png`](../artifacts/ui/m6-4-teaching-overlays/02-gated-hold-cutoff-overlay.png) shows gate/open, hold, release, measured cutoff, active B state, and RT60 marked **Not Meaningful**.
- [`03-learn-off-raw-response.png`](../artifacts/ui/m6-4-teaching-overlays/03-learn-off-raw-response.png) shows the same gated capture without teaching graphics or prose while retaining waveform, decay, metrics, and the architecture-based RT60 refusal.
- [`ab-response-overlays.mp4`](../artifacts/ui/m6-4-teaching-overlays/ab-response-overlays.mp4) demonstrates the live Barr A to Gated B publication, capture, and explanatory response overlay. The separate Learn Off screenshot is authoritative for disabled presentation.
