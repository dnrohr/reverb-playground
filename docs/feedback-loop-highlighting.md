# Feedback-loop highlighting

M4.1 makes directed feedback visible without compiling, editing, or executing the graph. Selecting a block finds every bounded simple directed loop containing that block. Selecting a cable restricts results to loops containing that exact cable. Acyclic selections receive an explicit **Feedback loops: none** result rather than a misleading partial path.

## Analysis and bounds

Inspection first computes deterministic strongly connected components over the semantic node/cable graph. Search is then restricted to the selected component and begins at the selected block's outgoing cables or the selected cable itself. Depth-first traversal never revisits a block inside one candidate, so every result is a simple directed cycle.

Outgoing cables and final results are ordered by stable cable ID. The UI accepts at most **64 loop results and 100,000 examined transitions** per selection. If either bound is reached, the inspector says that additional loops were omitted. This prevents pathological dense patches from monopolizing the UI thread while preserving exact results for ordinary reverb graphs. The automated maximum fixture uses the supported 256 blocks and 512 cables.

Inspection returns presentation copies with CSS classes; it never modifies nodes, cables, parameters, history, saved state, or native runtime state.

## Canvas presentation

The active loop uses solid amber blocks and cables. Blocks/cables belonging only to another matching loop use violet styling and dashed cables. **Previous** and **Next** cycle the active result, making shared-edge and nested loops visually separable while keeping all alternates visible.

The selected shared cable in the verification fixture belongs to two legal delay-containing cycles:

- a 30 ms path through Gain/Invert at `-0.6`, Low-pass at `6,400 Hz`, and Delay;
- a 12.5 ms path through the short Delay.

## Inspector facts

For each active loop the inspector lists:

- constituent block IDs in directed traversal order, closing back on the first block;
- nominal total delay in milliseconds, summed from Delay and Allpass `delay` parameters;
- Gain `gain`, Sum `gain` when present, and Allpass `coefficient` elements, including negative polarity;
- Low-pass cutoff elements.

These are static topology/parameter facts, not live measurements or stability guarantees. “Nominal” is deliberate: later modulation and runtime interpolation can change instantaneous delay, and filters inside a loop affect frequency-dependent loop gain.

## Verification and evidence

Tests cover nested alternatives, two cycles sharing the selected edge, selection outside a cycle, active/alternate decoration without graph mutation, delay/gain/filter summaries, and bounded maximum-size behavior.

- Reproducible fixture: [`artifacts/ui/m4-1-feedback-loop-highlighting/shared-edge-loop-fixture.rvp.json`](../artifacts/ui/m4-1-feedback-loop-highlighting/shared-edge-loop-fixture.rvp.json)
- Long-loop screenshot: [`artifacts/ui/m4-1-feedback-loop-highlighting/01-long-loop.png`](../artifacts/ui/m4-1-feedback-loop-highlighting/01-long-loop.png)
- Short-loop screenshot: [`artifacts/ui/m4-1-feedback-loop-highlighting/02-short-loop.png`](../artifacts/ui/m4-1-feedback-loop-highlighting/02-short-loop.png)
- Selection/cycling video: [`artifacts/ui/m4-1-feedback-loop-highlighting/loop-selection-and-cycling.mp4`](../artifacts/ui/m4-1-feedback-loop-highlighting/loop-selection-and-cycling.mp4)
