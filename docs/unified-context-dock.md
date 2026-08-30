# Unified context dock

M25.3 replaces the separate Inspector, response overlay, diagnostics overlay,
and research dialog with one tabbed context dock. Nothing in the dock changes
patch serialization or the real-time DSP contract.

## Routing

| User intent | Destination | Evidence language |
|---|---|---|
| Select a block, cable, or feedback loop | Inspector | Edited and saved patch values; live/runtime boundaries remain explicit |
| Inspect a matrix | Analyze | Edited coefficients and predicted orthogonality/energy |
| Complete an impulse measurement | Analyze | Measured waveform, onset, peak and density; estimated RT60 |
| Open diagnostics or latch safety | Analyze | Measured CPU/clipping, estimated work, prepared memory, compiled latency, immutable safety revision |
| Request contextual teaching or architecture research | Learn | Documented architecture, explanatory reconstruction notes, and listening guidance |

The relevant tab opens the existing context dock instead of creating a second
panel over the graph. Analyze widens the desktop dock up to 420 logical pixels
while preserving at least 490 logical pixels for the canvas. At narrow widths,
the existing single-dock overlay rule remains in force.

## State and revision behavior

Inspector, Analyze, and Learn remain mounted while inactive. Each owns its own
scroll container, so switching tabs retains selection and meaningful scroll
position. Context-tab state is presentation-only and is absent from patch JSON
and host state.

An impulse capture stores the active graph revision alongside the captured
samples. The Analyze header labels it Current, Stale, or Unknown against the
latest active revision. A stale capture remains available as historical
evidence but is never presented as current. Entering Analyze clears cached
diagnostics until a coherent new native snapshot arrives.

Energy telemetry and its native sample scanning still stop when Energy is off
or reduced motion disables it. Detailed runtime diagnostics polling runs only
while Analyze is visible or visible Energy needs the active revision; it is
dormant when both forms of analysis are hidden.

## Accessibility

The context selector uses the WAI-ARIA tab pattern: a named tablist, named tabs,
associated tabpanels, one active tab stop, and Left/Right/Home/End keyboard
navigation. Selected tabs use text, `aria-selected`, and a filled shape rather
than color alone. All panels remain scrollable at supported sizes, focus rings
remain visible, and reduced motion continues to suppress transitions.

## Evidence

- [`inspector-tab.png`](../artifacts/ui/m25-3-unified-context-dock/inspector-tab.png)
  shows saved/edited patch context without an overlay.
- [`analyze-tab.png`](../artifacts/ui/m25-3-unified-context-dock/analyze-tab.png)
  shows the widened current-revision Energy and measured-response surface.
- [`measured-response-tab.png`](../artifacts/ui/m25-3-unified-context-dock/measured-response-tab.png)
  shows a completed impulse capture routed directly into Analyze with its graph
  revision and evidence labels.
- [`learn-tab.png`](../artifacts/ui/m25-3-unified-context-dock/learn-tab.png)
  shows documented architecture and listening guidance in the same dock.
- [`context-tabs-workflow.mp4`](../artifacts/ui/m25-3-unified-context-dock/context-tabs-workflow.mp4)
  demonstrates keyboard-accessible tab changes while the graph and selection
  remain in place.
