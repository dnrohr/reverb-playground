# Density inspector

Status: M19.2 user interface contract.

The impulse-response viewer now has a **Density Inspector** toggle. Opening it computes a bounded analysis from the response samples already captured by the measurement workflow. Closing it removes the view; no density analysis, polling, telemetry, or audio-thread work runs while it is closed.

## Reading the display

The plot uses redundant line shapes so it remains readable without color:

- solid: normalized temporal echo density;
- dashed: strongest 1...30 ms recurrence;
- dotted: spectral flatness.

Up to three high-recurrence windows receive explicit `REPEAT <lag> ms` markers. Early, middle, and late cards show density, active peaks per second, crest factor, recurrence and lag, spectral flatness, and stereo correlation. These are separate dimensions, not a hidden quality score.

The panel explains the main interpretation boundary directly: density and crest describe the temporal field; recurrence exposes periodicity; spectral flatness describes coloration; stereo correlation describes relationship between channels. A spectrally flat impulse can still be temporally sparse, and a dense tail can still ring.

## Runtime boundary

Analysis starts only after the user opens the panel and uses a decimated, bounded view of each 40 ms window. The authoritative offline baselines remain the native M19.1 analyzer; this browser-side view prioritizes interactive inspection and retains the same metric meanings. Neither implementation changes captured or rendered audio.

The response view remains scrollable inside Analyze. The context dock widens on desktop without covering the canvas; at narrow widths it uses the existing single-dock overlay, and the early/middle/late cards stack vertically so every section remains reachable.

## Evidence

- [`density-overview.png`](../artifacts/ui/m19-2-density-inspector/density-overview.png): full-width response and density curves.
- [`density-compact-640x400.png`](../artifacts/ui/m19-2-density-inspector/density-compact-640x400.png): bounded minimum-size overlay.
- [`density-toggle-workflow.mp4`](../artifacts/ui/m19-2-density-inspector/density-toggle-workflow.mp4): response-only state followed by the opened inspector.

The browser evidence used a deterministic response fixture only to exercise presentation. That fixture was removed before the production asset rebuild; automated analysis tests and native impulse capture remain authoritative.
