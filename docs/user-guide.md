# Reverb Playground user guide

Reverb Playground is a visual instrument for listening to, constructing, and
understanding algorithmic reverbs. Every solid cable carries one mono audio
signal. Dashed cables carry control signals. The visible blocks and cables are
the saved and executable design; compact groups are navigation views, not
hidden DSP.

## First sound: load and audition a factory

1. Open the standalone application. It starts with **Barr Reference** selected.
2. In the top application bar, open the **Patch** list and choose a factory.
3. Press **Trigger Impulse** to hear the reverb alone. The impulse is generated
   inside the application and does not use live input.
4. Set **Dry Gain** to `0` and raise **Wet Gain** cautiously if you want to
   isolate the reverb. Restore Dry Gain when auditioning a recording.
5. Turn **Energy** on from **View** to see live activity. Reduced-motion mode
   deliberately keeps Energy dormant.

Expected result: the graph remains visible while the impulse travels through
the active paths. If output is silent, check Wet Gain, Emergency Mute, the
audio device, and the diagnostics panel in that order.

## Trace a signal path

1. Select a solid audio cable.
2. In **Inspect**, choose **Trace to Source** or **Trace to Output**.
3. If the cable participates in feedback, choose **Focus Complete Loop**.
4. Select blocks along the highlighted path to read their signal contract,
   audible purpose, control direction, and latency or safety notes.
5. Choose **Clear Trace** when finished.

Expected result: presentation highlighting changes, but the graph, audio,
history, viewport, and saved patch do not.

## Build a first reverb

Start from **File → Reset Patch**. Create this safe feed-forward chain:

```text
Stereo Input L ─┐
                Sum (+) → Allpass → Allpass → Delay → Stereo Output L
Stereo Input R ─┘                              └────→ Stereo Output R
```

1. Add blocks by clicking the module palette. Connect output sockets to input
   sockets; audio cables are solid and mono.
2. Use **Sum (+)** when two signals must share one input. There is no hidden
   normalization, so reduce level with **Gain** when necessary.
3. Increase allpass Delay to spread transient energy. Increase its Coefficient
   for stronger diffusion, remaining inside the displayed safe range.
4. Increase Delay to make spacing more obvious. Shorter, unequal times usually
   sound less like one discrete repeat.
5. Save with **Ctrl+S**. Invalid edits are refused and the last valid runtime
   remains audible.

Feedback is not required for this first exercise. When adding feedback, ensure
every cycle contains causal delay and conservative gain. A zero-delay cycle is
rejected before publication.

## Edit and compare variants

1. Open **A/B** and capture slot A before editing.
2. Change one audible idea at a time, then capture slot B.
3. Audition A and B. Choose **Matched** only when the level-matching evidence is
   available; matching attenuates and never boosts.
4. Use **Promote Active** to make an auditioned snapshot the editable graph, or
   **Revert to Edited Graph** to leave the saved design unchanged.

Snapshots are session-only. Promotion is a normal undoable graph edit.

## Diagnose temporarily

Select an audio block or cable and use **Mute**, **Isolate**, or **Bypass** in
the inspector. Temporary audition does not modify the saved patch or exported
WAV. Bypass is refused when its explicit temporary rewiring would create a
zero-delay cycle. Press an active Mute, Isolate, or Bypass button again to toggle
it off. **Clear Audition** and `Escape` are global exits; clicking empty canvas
only changes selection and does not change audio.

## Use assisted tuning

Supported graphs show **Tune** above the canvas. A suggestion lists its exact
parameters, audible intent, preserved settings, and compatibility before it
publishes an audition-only runtime. Compatible suggestions accumulate from the
current edited graph. An overlapping suggestion is labeled as a conflict or
replacement instead of silently overwriting earlier work. **Apply tuning** commits
each suggestion as its own undoable edit. **Discard preview** restores the exact
edited graph. Suggestions are design aids, not claims of a uniquely correct reverb.

Parameter cards keep saved and heard state explicit. **Saved** is the base value
written to the patch. **Live** is the current modulated value. **Preview** belongs
to temporary tuning or A/B audition, and **Pending topology** means a compiled
revision has not yet become active. These labels update without opening Analyze.

## Inspect groups and compounds

Visual groups organize ordinary blocks. Collapsing a group changes only
presentation; expanding it restores every authoritative primitive and cable.
The Four-Line Dense Room's Matrix Mixer summarizes the visible 4×4 network.
Its compact parent has four mono inputs and four mono outputs. Double-click it
or choose **Open schematic** in Inspect to edit its 16 Gain coefficients and 12
Sum blocks on a dedicated live canvas. Use **Back to patch** or `Alt+Left` to
return; there is no Apply step, and undo/redo spans both levels. Reusable
subpatches use the same interaction while retaining their versioned provenance.
See [Hierarchical schematics](hierarchical-schematics.md) for boundary,
migration, copy/paste, and deletion details.

## Play and export an audio file

1. In the standalone audio drawer, choose **Audio File**, then **Load**.
2. Select a waveform region if desired. **Loop** repeats the selected region.
3. Set Wet Gain and Dry Gain independently, then press **Play**.
4. Choose **Export WAV**. A selected loop exports that source interval once,
   followed by its bounded reverb tail; it does not repeat forever.

Offline export is not slowed by live callback load. It renders as fast as the
machine permits while producing correctly timed audio. The VST3 instead uses
the DAW's stereo input, transport, automation, and export workflow.

## Feedback safety and recovery

- **Emergency Mute** silences output immediately without rewriting the patch.
- A non-finite or sustained runaway output latches safety mute. Correct the
  graph, then use **Reset Safety**.
- A failed compilation leaves the last valid runtime active and reports the
  failed revision under **Analyze**.
- Undo the last risky change if its cause is unclear. Do not raise output gain
  merely because a safety-muted graph is silent.

## Crash reports

Automatic crash upload is not present in the current alpha. If the application
crashes, report the Windows version, standalone or DAW/host name, audio device,
sample rate, block size, factory or patch, exact last action, and the version
plus 12-character commit shown under **Help**. Attach the smallest patch that
reproduces the problem if it is safe to share. Local minidumps and plain-text
crash summaries are planned for M32.

## Workspace arrangements

- **Balanced** keeps the module palette and context dock available.
- **Create Focus** prioritizes canvas space for construction.
- **Learn & Inspect** widens contextual inspection and hides the palette.

These are presentation-only arrangements. They never enable documentation,
change audio, enter patch history, or alter saved state. Open **Help** for the
offline article library from any arrangement.

## More help

- [Keith Barr reverb architectures](keith-barr-reverb-architectures.md)
- [Module and visualization reference](module-and-visualization-reference.md)
- [Schematic editor interactions](schematic-editor-interactions.md)
- [Audio workflow guide](audio-workflow-guide.md)
- [Topology research](large-modulated-and-shimmer-reverb-topologies.md)
