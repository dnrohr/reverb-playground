# Stereo impulse and decay view

M4.3 turns each completed M4.2 capture into a navigable response inspector. The viewer opens automatically after native capture and keeps analysis on the message/UI side of the real-time boundary.

## What the view shows

- **Left waveform** occupies the upper lane and uses a solid cyan envelope.
- **Right waveform** occupies the lower lane and uses a dashed amber envelope.
- **Stereo energy decay** occupies the bottom lane as a white backward-integrated Schroeder curve.
- Dashed violet guides mark the -5 to -35 dB T30 regression band.
- Metrics report the visible time window, per-channel peaks, thresholded onset, and RT60 when defensible.

The two channels differ by vertical position, explicit `L`/`R` labels, and solid/dashed line style as well as color. The graph remains interpretable in monochrome and for common color-vision differences.

## Navigation

**Early / 16x** jumps to the first 1/16 of the capture, wide enough to show the Barr reference's first reflection cluster. **Zoom in** and **Zoom out** use powers of two up to 256x. **Full tail** restores the entire response. The pan slider moves the bounded visible window through the full capture; a mouse wheel over the chart changes zoom. Window start/end labels and the zoom readout make the current scale explicit.

Waveforms use min/max buckets rather than point sampling, so narrow peaks survive decimation at full-tail scale. Decay curves are independently decimated to the current window. Both operations are bounded by the chart's 450-column presentation budget even for the ten-second, 192 kHz capture limit.

## Decay and RT60 policy

Stereo energy is squared, summed, integrated backward, normalized to total captured energy, and expressed as decibels. RT60 is a T30-style estimate: linear regression from -5 through -35 dB, extrapolated to -60 dB. This matches the project's established native response-measurement policy and its synthetic 0.75-second exponential fixture.

The viewer withholds RT60 and explains why when:

- no response energy exists;
- final-10-percent RMS exceeds `1e-4` of response peak, indicating noise or a truncated tail;
- fewer than 20 samples occupy the fit range; or
- the fitted slope is degenerate, flat, or rising.

These are refusal rules, not error states. A longer/quieter capture may make an estimate possible, while a gated or reverse-style response may legitimately have no meaningful conventional RT60.

## Architecture teaching layer

Factory captures can add optional measured architecture annotations without changing the underlying waveform or decay. Causal Reverse Envelope marks its rising-energy span, late peak, and -40 dB crossing. Level-Gated Room marks gate/open, hold, release, and measured cutoff using the captured graph's visible timing controls; its explicit abrupt truncation withholds RT60 even when a pre-cutoff regression exists. Barr, custom, and silent captures remain unannotated. **Learn Off** removes these graphics and their explanatory copy. See [Visualization teaching overlays and A/B comparison](visualization-teaching-overlays.md).

## Evidence

Reviewed screenshots under [`artifacts/ui/m4-3-stereo-impulse-decay/`](../artifacts/ui/m4-3-stereo-impulse-decay/) cover a short response, the runtime Barr reference at full-tail/early/panned scales, and a long bloom-like response. The [`measurement and navigation video`](../artifacts/ui/m4-3-stereo-impulse-decay/stereo-response-measurement-and-navigation.mp4) demonstrates the same progression. Synthetic fixture screenshots exercise presentation shape; the Barr screenshots are produced from the runtime-bound reference measurement path.
