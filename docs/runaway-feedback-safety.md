# Runaway-feedback safety and recovery

M5.6 turns numerical containment into a complete constructed-graph workflow. Safety remains outside the user patch: no hidden limiter, delay, or gain changes the visible topology.

## Detection and mute

Each audible channel has an allocation-free output guard. NaN or positive/negative infinity mutes the complete stereo block immediately. A finite sample above an absolute `16.0` hard ceiling also mutes immediately. In addition, output remaining above absolute `4.0` for 50 continuous milliseconds is classified as sustained runaway and mutes deterministically; any sample at or below that lower threshold resets the consecutive-sample count. Sample-rate preparation converts the duration to a fixed sample count, so host block boundaries do not change the decision.

The first violation latches silence and records its kind, channel, sample index, and immutable active graph revision. Processing stays muted while graph edits and Undo remain available. The guard uses constant storage and performs one bounded pass over each output block.

## Likely-loop guidance

When constructed-graph safety mutes, the editor searches explicit feedback cycles through Delay blocks using the existing 100,000-transition inspection budget. It ranks candidates heuristically by the product of visible absolute Gain/Allpass coefficients divided by nominal loop delay, marks the highest-ranked loop in red on the schematic, and lists its path in diagnostics. This is deliberately labeled **heuristic**: modulation, filtering, phase, and interacting loops mean it is not a mathematical stability proof. If no explicit delayed loop can be identified, diagnostics say so rather than inventing one.

## Explicit recovery

The safety panel opens automatically and retains **Undo Last Edit**. **Recover Audio** is the only action that clears the numerical latch. On the audio thread that action first resets every active/crossfading constructed runtime, clearing stored delay and feedback energy, then resets both guards. Recovery therefore emits silence until new live input or an explicit audition impulse excites the graph; it never releases an abandoned unstable tail automatically.

The resizable editor clamps its preferred initial size to the scaled primary desktop work area, keeping both actions reachable rather than allowing a 1280-by-800 logical window to extend below a smaller 125%-scaled screen.

The recommended workflow is: inspect the marked loop, lower a risky value or Undo the edit, then recover. Manual emergency mute remains independent.

## Verification

Native tests cover NaN, infinity, the immediate hard ceiling, the exact continuous 50-millisecond detector, mute persistence through a safe graph edit, and explicit state-clearing recovery to silence. Browser tests cover bounded likely-loop ranking and danger decoration; the unified history suite continues to prove structural and parameter Undo.

Reviewed native evidence at 125% Windows scaling:

- [Latched runaway and likely-loop guidance](../artifacts/ui/m5-6-runaway-feedback/01-runaway-loop-and-recovery.png)
- [Recovered safe reference with Undo retained](../artifacts/ui/m5-6-runaway-feedback/03-recovered-silence.png)
- [Mute, safe reset, Undo availability, and explicit recovery video](../artifacts/ui/m5-6-runaway-feedback/runaway-mute-edit-undo-recovery.mp4)
