# Visual Reverb Constructor / Inspector

Status: product definition v0.2
Date: 2026-08-08

## 1. Product definition

An open-source real-time audio instrument for constructing, hearing, and understanding algorithmic reverberators as small signal-flow schematics.

The experience is inspired by ZOIA-style patching but deliberately narrow. Users assemble reverbs from a curated vocabulary - allpass, delay, gain, sum, filter, modulation, input, and output - connect them visually, edit a few meaningful parameters, and immediately hear and inspect the resulting network.

The product should make structures such as Keith Barr's allpass loops understandable without requiring code or a general modular-synthesis environment.

The product begins as an **instrument that teaches through use**. It should feel rewarding before the user understands the theory, then progressively reveal why the patch sounds as it does. Over time it may also become a structured teaching environment and a serious architecture inspector, but neither secondary goal should make the first instrument feel academic.

The first development target is not a general graph engine. It is a visual, parameterized reconstruction of the BarrVerb/MIDIVerb-I signal path. The editor and DSP runtime become general only as that reconstruction is decomposed into reusable primitives.

### One-sentence promise

**Patch a reverb, hear it immediately, and see why it behaves that way.**

### Initial identity

- **Primary:** playable reverb-construction instrument.
- **Secondary:** embedded explanations and visual feedback that teach through interaction.
- **Long-term:** architecture inspection, comparison, reconstruction, and export.
- **Reference MVP:** the original MIDIVerb-I/BarrVerb family.
- **License:** open source; exact license to be selected after dependency and ROM-distribution review.

## 2. Product principles

1. **Reverb-first, not general-purpose modular DSP.** Every feature should help construct or understand a reverberator.
2. **The diagram is the program.** The visible graph must correspond predictably to the signal flow.
3. **Safe experimentation.** Feedback, clipping, invalid graphs, and live edits must not produce dangerous output.
4. **Hear and see together.** Construction and inspection are equally important.
5. **Few modules, deep behavior.** Prefer eight excellent primitives over a large module catalog.
6. **Reproducible patches.** Saved graphs include schema version, parameters, positions, and engine assumptions.
7. **Architectural honesty.** The UI may simplify presentation, but it must not conceal inserted delays or feedback behavior.
8. **Progressive disclosure.** A useful patch should be approachable at a glance, with mathematics and implementation details available on demand.

## 3. Intended users

- Musicians exploring distinctive algorithmic reverbs.
- DSP learners who understand signal-flow diagrams more readily than equations or code.
- Reverb designers prototyping compact topologies.
- Researchers inspecting historical structures and impulse responses.

The initial version is not intended to replace Max/MSP, Pure Data, Reaktor, ZOIA, or a general audio programming language.

## 4. Initial interaction concept

```text
+----------------+------------------------------------------------+------------------+
| MODULES        | PATCH CANVAS                                   | INSPECTOR        |
|                |                                                |                  |
| I/O            |  [Input]--->[LPF]--->[AP 37]--+                | Allpass 37       |
|  Input         |                                |                | Delay   13.7 ms  |
|  Output        |                  +-------------+                | Gain    0.50     |
|                |                  v                              | Mod     Off      |
| TIME           |  [ + ]<---[Delay 29ms]<---[AP 91]               |                  |
|  Delay         |    |                             ^              | [Bypass] [Solo]  |
|  Allpass       |    +-----------------------------+              |                  |
|                |                                                | Stability: safe  |
| MIX            |              L branch -->[L Out]                | Loop: 84.7 ms    |
|  Gain          |              R branch -->[R Out]                | Est. RT60: 2.4 s |
|  + / Sum       |                                                |                  |
|                |  ───── selected feedback loop highlight ─────  |                  |
| FILTER         |                                                |                  |
|  Low-pass      |                                                |                  |
| MODULATION     |                                                |                  |
|  LFO           |                                                |                  |
+----------------+------------------------------------------------+------------------+
| Input level | Output level | Freeze | Excite impulse | CPU | Undo | A/B | Presets    |
+--------------------------------------------------------------------------------------+
```

Primary gestures:

- Drag a module from the left library onto the canvas.
- Drag from an output port to an input port to connect.
- Select a node or cable to edit it in the right inspector.
- Double-click a compact value on a node for direct editing.
- Delete, duplicate, box-select, move, undo, and redo using standard schematic-editor conventions.
- Click a cable or node to highlight every feedback loop containing it.
- Trigger an impulse and see measured energy illuminate the graph.

## 5. MVP module vocabulary

### Required

| Module | Purpose | Initial parameters |
|---|---|---|
| Input | Stereo plugin input and optional mono sum | stereo/mono mode, gain |
| Output | Stereo wet output | channel, gain |
| Delay | Plain delay line | time in ms/samples, optional interpolation mode |
| Allpass | Delay allpass | delay, coefficient, optional modulation |
| Gain | Sign and scale | linear/dB gain, invert |
| Sum (`+`) | Mix signals | explicit inputs, per-input `+/-` polarity |
| Low-pass | Loop damping | cutoff or simple coefficient |
| LFO | Generate a modulation signal | waveform, rate, phase, depth |
| Modulation mapping | Scale/offset a control signal before a parameter socket | amount, bipolar/unipolar, offset |

### Strong candidates after MVP

- High-pass or DC blocker.
- Saturator/quantizer for historical character.
- Random/smoothed-random modulation source.
- Diffuser macro, visibly expandable into primitive allpasses.
- Stereo matrix/crossfeed.
- Comment/region frame for labeling diffuser, tank, and output taps.
- One-sample/unit delay for explicit cycle legality.

Avoid a generic oscillator, envelope, MIDI logic, sequencer, sampler, or arbitrary scripting module until the reverb workflow proves it needs one.

## 6. Signal, channel, and port model

Audio cables carry one mono signal. Stereo is represented by two explicit ports. A stereo `Input` convenience block exposes `L` and `R`; a `Mono Sum` or explicit `+` block can combine them. Stereo `Output` exposes `L` and `R` inputs.

Modulation is supported through a second typed signal: control. Modulation blocks produce control cables that connect only to parameter sockets. Audio and control cables must be visually distinct without relying on color alone. There is no event/MIDI cable type initially.

The original MIDIVerb/BarrVerb reference patch should visibly teach its channel structure:

```text
Left input  --+
             +--> mono sum --> input filtering --> one reverb tank --> left branch  --> Left output
Right input --+                                                   +--> right branch --> Right output
```

It does **not** contain two independent left/right reverb tanks. Stereo is created by taking different weighted taps from one internal network. Later factory patches may use separate stereo injection points or cross-coupled structures, but the channel routing must remain explicit.

Proposed rules:

- One output may feed many inputs.
- A normal signal input accepts one cable; use `Sum` for multiple sources.
- Connections are directed.
- Feedback is allowed only when every directed cycle contains state/delay.
- Zero-delay algebraic cycles are rejected and visually explained.
- Channel count is explicit and validated.
- Audio outputs may branch freely. A branch is the MVP representation of an internal tap; no dedicated `Tap` node is required.
- A gain or polarity change on one branch requires a visible `Gain` node.
- Control outputs may feed many compatible parameter sockets.
- Parameter modulation is `base value + mapped control`, followed by the parameter's documented clamping or wrapping rule.
- Engine-inserted safety processing, if any, is displayed in the inspector and export metadata.

Summing is always explicit. Connecting a second audio cable to an occupied single-input port is rejected, with a shortcut offering to insert a `+` block.

There is no separate subtraction module in the MVP. A `+` block exposes a polarity toggle on each input, and a `Gain` block can invert any branch. This keeps subtraction visible without multiplying the primitive vocabulary.

## 7. Graph lifecycle and real-time behavior

The editable UI graph must never be mutated directly by the audio thread.

Proposed lifecycle:

1. The user changes an editable graph on the UI thread.
2. A validator checks port types, cycles, limits, and parameter ranges.
3. A compiler finds strongly connected components, establishes an execution schedule, allocates delay memory, and creates immutable runtime state.
4. The new runtime graph is prepared off the audio thread.
5. The audio thread swaps to it at a block boundary.
6. When feasible, the old graph is briefly crossfaded into the new graph so tails do not click or disappear abruptly.
7. Old runtime state is reclaimed away from the audio thread.

Parameter edits take effect continuously while dragging and use lock-free/atomic transfer plus appropriate smoothing. Delay-time edits require an interpolation/crossfade policy so dragging does not create uncontrolled discontinuities; a user-selectable tape-style behavior can be added later.

Topology edits preview continuously in the UI, validate immediately, and become audible when a connection is completed or a node is dropped. The new runtime is prepared off the audio thread and swapped at a block boundary.

Long preservation of the abandoned reverb tail is not an MVP requirement. It can double runtime cost and make structural experimentation feel laggy. The engine instead applies a short click-suppression crossfade between old and new runtime output. Parameter edits remain continuous and do not replace the runtime.

## 8. Feedback safety

Feedback is essential, so safety cannot mean forbidding gain above an arbitrary number. Stability depends on the entire frequency-dependent loop, not an individual gain knob.

MVP safeguards:

- hard finite-value checks at every output boundary;
- a final emergency limiter or mute independent of the user graph;
- conservative default gains;
- a user-adjustable master audition level;
- automatic mute and diagnostic state after NaN/infinity or persistent runaway level;
- loop highlighting and a clearly labeled heuristic stability warning;
- optional “safe audition” mode that injects a quiet impulse before unmuting live input;
- undo available even while emergency-muted.

Do not promise a mathematically exact stability verdict for arbitrary nonlinear or modulated graphs. For the linear time-invariant MVP, stronger analysis may be possible, but the UI must distinguish proof from heuristic estimation.

## 9. Inspection and visualization

The first visualizations answer three different questions:

1. **Where is the feedback?** Highlight every directed loop containing the selected node or cable, with total loop delay and cumulative gain/filter elements listed. This is the most important schematic-level teaching aid.
2. **What did the network do to an impulse?** Show the stereo impulse response and an overlaid smoothed energy-decay curve. This exposes predelay, bloom, discrete echoes, decay time, reverse envelopes, and channel differences in one place.
3. **Where is energy moving now?** Illuminate nodes and cables from decimated RMS/peak telemetry. During an impulse audition this reads as energy propagating and recirculating without pretending that an animated particle is a literal audio sample.

These views connect topology, measured behavior, and live sound. A spectrogram is valuable but visually dense; echo-density metrics are educational but need careful definition; stereo correlation becomes more useful after stereo patching matures.

### MVP

- Live input/output meters.
- Selected-node input/output meter.
- Impulse excitation button.
- Stereo time-domain impulse response display with zoom.
- Smoothed energy-decay overlay and an explicitly labeled RT60 estimate when the decay supports one.
- Highlighted directed feedback loops with loop delay and elements.
- Energy glow on active nodes and cables, with a global disable for performance and accessibility.
- Total delay memory and CPU estimate.

### Later

- Frequency response and spectrogram.
- Energy-decay curve by frequency band.
- Echo-density estimate over time.
- Stereo correlation and inter-channel coherence.
- Freeze graph and inspect one sample/impulse moving through nodes.
- Compare two graph revisions or presets.
- Export impulse response, diagram, JSON graph, or generated DSP code.
- Historical/fixed-point execution modes.

Inspection should run from decimated/copied telemetry or offline analysis buffers. The UI must not synchronously query or block the real-time audio thread.

## 10. Patch format

Use a versioned, framework-neutral data model. JSON is suitable initially.

Conceptual structure:

```json
{
  "schemaVersion": 1,
  "engineVersion": "0.1",
  "sampleRatePolicy": "seconds",
  "nodes": [
    { "id": "ap1", "type": "allpass", "parameters": { "delayMs": 13.7, "gain": 0.5 } }
  ],
  "connections": [
    { "from": ["input", "out"], "to": ["ap1", "in"] }
  ],
  "layout": {
    "nodes": { "ap1": { "x": 420, "y": 180 } }
  }
}
```

Semantic graph data and visual layout should be stored separately in the same document. Stable UUIDs allow automation, undo, comparison, and migration.

Critical decision: delay values can be defined in seconds, samples at a reference rate, or both. Musical “same time at every sample rate” behavior favors seconds; historical machine emulation often requires fixed sample counts and a fixed internal rate.

## 11. Plugin state and automation

DAW hosts expect a mostly fixed parameter list, while a constructed graph creates parameters dynamically. This is a major product constraint.

Recommended MVP:

- Serialize the entire graph as plugin state/chunk.
- Expose a fixed bank of assignable macro parameters to the host, perhaps 8 or 16.
- Let users map graph parameters to those macros.
- Keep direct node edits internal to the plugin UI unless mapped.

Trying to create and remove host parameters whenever nodes change will behave inconsistently across plugin formats and hosts.

## 12. UI implementation candidates

### Option A: web UI in a plugin webview

Use React/TypeScript with React Flow for the editor, with C++ owning DSP and graph compilation.

Advantages:

- React Flow already supplies selection, dragging, zoom/pan, custom nodes, handles, edges, keyboard interaction, and accessibility basics.
- Fastest route to a polished editor and standalone/browser prototype.
- Excellent CSS/SVG control for educational overlays and animation.
- MIT licensed.

Costs:

- Embedding webviews across VST3/AU/LV2 hosts adds packaging, focus, scaling, GPU, accessibility, and platform-specific testing.
- UI-to-C++ messaging needs a carefully versioned bridge.
- A browser engine can dominate binary/runtime footprint relative to the DSP.

React Flow: <https://reactflow.dev/>

### Option B: native C++ UI

Use JUCE components or DPF/DGL and implement/adapt a node canvas. ImGui node-editor libraries are useful prototypes, but their tool-like look and accessibility may require substantial product work.

Advantages:

- One native process/runtime and direct C++ model integration.
- Conventional path for audio-plugin deployment.
- Easier to control realtime telemetry and graphics resource lifetime.

Costs:

- More work for polished cables, hit testing, selection, keyboard behavior, minimap, zoom/pan, text editing, undo, and accessibility.
- Immediate-mode ImGui styling may fight the desired instrument-like visual identity.

Candidates:

- `thedmd/imgui-node-editor`: <https://github.com/thedmd/imgui-node-editor>
- `Nelarius/imnodes`: <https://github.com/Nelarius/imnodes>
- JUCE `AudioProcessorGraph` as a reference/runtime helper: <https://docs.juce.com/develop/classjuce_1_1AudioProcessorGraph.html>

### Option C: standalone/web prototype first, native plugin second

Prototype the UX in React Flow against a shared JSON schema and a small native or WebAudio test engine. In parallel, implement the production C++ graph compiler/runtime. Decide on embedded webview versus native editor after testing hosts.

This is the recommended discovery path. It prevents the hardest deployment decision from blocking validation of the product itself.

### Other libraries

- Rete.js is powerful and supports dataflow/control-flow concepts, but some advanced plugins have noncommercial licensing. Its processing abstraction is more machinery than the proposed small audio graph needs: <https://retejs.org/docs/licensing/>.
- LiteGraph.js is MIT licensed, Canvas2D-based, lightweight, supports JSON and subgraphs, and is proven at larger graph sizes. Its built-in visual language is more developer-tool-like and may be harder to make feel refined than React Flow: <https://github.com/jagenjo/litegraph.js/>.
- Tracktion Engine is capable but far broader than required and carries GPL/commercial licensing considerations: <https://github.com/Tracktion/tracktion_engine>.

No UI library should execute the production audio graph. It should edit the shared graph model; C++ should validate and run it.

## 13. Likely technical architecture

```text
Editor UI
  -> editable graph + undo history
  -> validation/diagnostics
  -> serialized graph schema
  -> native graph compiler
       -> node schedule / strongly connected components
       -> delay-memory plan
       -> immutable runtime graph
  -> real-time audio engine
       -> meters/telemetry ring buffer
  -> inspector UI + offline analyzer
```

Likely C++ components:

- framework-neutral DSP primitives;
- graph schema and migration layer;
- graph validator/compiler;
- runtime memory arena;
- lock-free runtime swap mechanism;
- plugin wrapper (JUCE or DPF decision pending);
- test harness that renders graphs offline;
- UI bridge.

## 14. Major bottlenecks

### Cyclic graph scheduling

A normal DAG can be topologically sorted. Reverb tanks intentionally contain cycles. The compiler must identify cyclic components and define exactly where state advances. Ambiguous zero-delay loops must be rejected.

### Glitch-free topology editing

Changing a delay, removing a feedback path, or swapping the runtime can click, erase a tail, or momentarily destabilize the system. Crossfading whole runtimes is memory/CPU expensive but conceptually reliable.

### Delay-memory allocation

Many nodes can share a planned arena, as Barr's machine demonstrates. Dynamic allocation is forbidden on the audio thread. Maximum delay policy and graph recompilation behavior need early definition.

### Parameter automation versus dynamic graphs

Plugin-host parameter lists are not naturally dynamic. Fixed macros are the practical compromise.

### Sample-rate semantics

Milliseconds preserve time across hosts; sample counts preserve topology ratios and historical behavior only at a known rate. Modulation interpolation and graph state migration complicate rate changes.

### Inspection cost

Impulse responses and decay metrics can be computed offline, but live node-by-node visualization can flood the UI bridge. Telemetry needs fixed budgets and decimation.

### UI deployment

Web technologies make the editor easier; plugin webviews make distribution and host compatibility harder. Native UI reverses that tradeoff.

### Preserving comprehensibility

Macros, implicit summing, hidden safety delays, automatic channel conversion, and invisible compensation can make the diagram stop being truthful. This is a product-design bottleneck as much as a software one.

## 15. Product decisions

| Question | v0.2 decision |
|---|---|
| Product identity | Instrument that teaches through use; later teaching and research modes. |
| Reference MVP | BarrVerb/MIDIVerb-I reconstruction decomposed into primitives. |
| Input/output | Stereo host I/O represented as explicit mono ports and cables. |
| Stereo architecture | One tank may produce stereo through different branches/taps; dual tanks are not assumed. |
| Cable types | Mono audio cables plus visually distinct control/modulation cables. No events/MIDI initially. |
| Modulation | Explicit LFO and mapping blocks feeding parameter sockets. |
| Summing/subtraction | Explicit `+` block with per-input polarity; no separate subtraction block. |
| Taps | Ordinary cable branches; branch-specific processing uses visible nodes. |
| Delay units | Milliseconds by default; samples available in an advanced/historical mode later. |
| Parameter editing | Continuous and smoothed. |
| Topology editing | Audible on completed edit, with off-thread compile and short click-suppression crossfade. |
| Tail preservation | No long orphan-tail preservation in MVP. |
| First visualizations | Loop highlighting, stereo impulse/decay view, and measured energy glow. |
| License | Open source, subject to dependency and ROM review. |
| Reverse reverb | Planned as a constructible architecture/preset family after the foundational tank works. |

## 16. Remaining product questions

### Identity and presentation

1. What should the product be called?
2. Should its visual character lean toward an electronic schematic, a musical pedal/workstation, or a restrained hybrid?
3. How much explanation should appear automatically versus on demand?

### Graph semantics

4. What parameter sockets should Delay and Allpass expose to modulation?
5. What modulation-rate ceiling separates control processing from audio-rate processing?
6. Are one-sample safety delays ever inserted automatically, or only offered as an explicit fix?
7. Should tempo-synchronized delay units be part of the first musical release?

### Live editing and safety

8. What happens visually and sonically when the network runs away?
9. Do we permit gains or allpass coefficients outside stable conventional ranges?
10. Which delay-time edit behavior is the default: clean crossfade, pitch glide, or selectable?

### Inspection

11. Should users be able to freeze or step through an offline impulse one processing quantum at a time?
12. When should spectrogram, echo-density, and stereo-correlation views enter the roadmap?

### Platform and licensing

13. Target formats: standalone, VST3, AU, CLAP, LV2, AAX?
14. Target operating systems?
15. Which open-source license fits the intended contributor and commercial-use policy?
16. Is a webview acceptable inside the shipped plugin?
17. Does the first release need DAW automation beyond fixed macros?

### Compatibility and scope

18. Can the open-source MVP legally distribute the transformed BarrVerb ROM, or should it use a clean-room hand-authored reference patch?
19. Is exact fixed-point/low-sample-rate coloration a later engine mode or a separate historical module set?
20. Can users export generated C++/FV-1 code or only audio/patches?
21. Are subgraphs/macros part of v1, or intentionally deferred?

## 17. Feasibility assessment

### High feasibility

- Visual node placement and cable editing.
- A small curated module set.
- JSON patch serialization.
- Offline impulse-response rendering.
- Loop detection and highlighting.
- VST3/AU/standalone delivery with a conventional C++ framework.
- Safe block-boundary graph swaps.

### Moderate difficulty

- Smooth tail-preserving topology edits.
- Reliable stability analysis.
- Rich live visualization inside diverse plugin hosts.
- Cross-platform embedded web UI.
- Host automation mapped onto dynamic node parameters.
- Sample-accurate modulation across arbitrary feedback graphs.

### High risk if included too early

- Arbitrary control-rate patching.
- User-defined code.
- General nonlinear modules inside feedback loops.
- Automatic conversion between arbitrary channel widths.
- Full historical hardware emulation and a modern free-form engine in one runtime.
- Mobile/AAX/Linux plus every desktop plugin format in the first release.

## 18. Proposed MVP boundary

- Standalone plus one plugin format during development.
- Mono audio-rate cables, typed control cables, and stereo exposed through explicit I/O ports.
- The primitive module set needed to express BarrVerb, plus LFO/modulation mapping.
- Explicit summing and explicit state in every loop.
- Create/connect/delete/move/edit, undo/redo, save/load.
- Fixed macro bank for DAW automation.
- Impulse trigger, response plot, decay estimate, loop highlighting.
- Emergency output protection.
- Topology edits applied at block boundaries with a short click-suppression crossfade; no long orphan-tail preservation.
- A small factory library: Schroeder baseline, MIDIVerb-I-inspired network, Barr later single loop, Bloom-like diffuser, gated, and reverse examples.

## 19. Suggested discovery plan

1. Decide graph semantics using paper examples of three reverbs.
2. Build a clickable UI prototype with no production DSP.
3. Define the versioned graph schema from prototype interactions.
4. Implement a headless C++ renderer for the eight primitives.
5. Add cyclic scheduling, validation, offline impulse tests, and safety tests.
6. Connect the prototype UI to the native renderer.
7. Test a webview and a native canvas inside representative DAWs before committing to the production UI stack.
8. Add live runtime swapping, telemetry, and plugin state.

## 20. Decisions needed for v0.3

The next specification revision should lock down:

1. Product name and visual character.
2. MVP platforms and plugin framework.
3. Control-rate ceiling and modulation parameter policy.
4. Exact legal-cycle and one-sample-delay policy.
5. Delay-time editing/interpolation behavior.
6. Runaway-feedback UX.
7. Open-source license and BarrVerb ROM strategy.
8. Whether the first prototype is a clickable UI, an audible fixed Barr graph, or a thin vertical slice of both.
