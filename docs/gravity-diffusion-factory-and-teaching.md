# Gravity Diffusion factory patch and teaching view

M9.5 ships the complete Gravity Diffusion instrument as an ordinary editable
schema-v2 graph. It is built from the same public blocks available in the
module palette: stereo I/O, Sum, Gain, Delay, Allpass, Low-pass, Macro, LFO,
and Curve Mapper. There is no factory-only processor, hidden destination table,
embedded impulse response, or proprietary preset data.

## Identity and provenance

The factory identity is **Gravity Diffusion**. Its catalog family is
`gravity-diffusion`, its checked document is
`factory-patches/gravity-diffusion.rvp.json`, and its source of truth is the
project-authored native builder in `src/graph/Source/GravityDiffusionGraph.cpp`.
The catalog deliberately does not use the Blackhole product name. Public
descriptions of other products informed the behavioral question explored by
Gravity; they did not disclose or supply this topology.

Generate the checked document after an intentional builder change:

```powershell
.\scripts\generate_gravity_factory_patch.ps1
node scripts/generate_factory_patches.mjs
```

The first command exports the complete native graph. The second verifies its
known hash while regenerating the catalog and the other deterministic factory
documents. CI uses `node scripts/generate_factory_patches.mjs --check` to reject
drift. Native and browser tests load the result, validate its public types,
write schema v2, parse it again, and require byte-identical second output. That
round trip is the evidence that the editor does not depend on hidden state.

## Reconstructing the topology

Read the graph from left to right:

1. Stereo input is summed to mono through two half-gain branches.
2. Four serial input Allpasses establish initial density.
3. The tank entry feeds eight progressive Delay/Allpass stages.
4. Every stage exposes a tap Gain. Eight visible Gravity Curve Mappers drive
   those gains while preserving the normalized weighting contract.
5. Alternating taps feed two visible sum trees for different stereo outputs.
6. A 97 ms Delay, Gain, and Low-pass form the one legal delayed feedback return.
7. Size maps the fixed delay lengths; Feedback maps return gain; Damping maps
   return cutoff; Modulation maps the four input-Allpass coefficients.
8. Two slow LFOs independently move odd and even stage-Allpass delays.

The graph contains 58 nodes and 94 cables. All 58 nodes have stored positions,
so **Fit** exposes the entire construction rather than relying on a factory-only
layout rule. The detailed equations, clamps, ownership boundaries, and safety
limits remain in [Normalized Gravity weighting](normalized-gravity-weighting.md)
and [Gravity Diffusion complementary controls](gravity-diffusion-controls.md).

## Safely modifying it

- Keep at least one Delay in every feedback cycle. Removing the 97 ms return
  Delay creates an algebraic loop and publication must fail while the last valid
  runtime keeps playing.
- Keep each parameter input single-source. Insert an explicit Curve Mapper or
  Sum instead of inventing an implicit control merge.
- Preserve the declared Delay, Allpass, feedback, node, cable, control-node,
  and mapping bounds. Diagnostics reports the prepared memory and any rejected
  publication.
- Use continuous Macro edits for sound design. A topology edit is compiled and
  published off the audio thread; parameter-only edits remain on the prepared
  runtime path.
- Save after editing. The exported file is ordinary schema v2 and reloads as a
  custom patch with the same nodes, cables, parameters, names, and layout.

## Teaching evidence hierarchy

Selecting the Gravity Macro opens a prominent bipolar control and an envelope
comparison:

- the violet path is a **prediction** derived only from the current Gravity
  coordinate;
- the teal dashed path is the nearest checked M9.4 **reference fixture**, with
  its exact five-Macro controls, measured peak, and early/late ratio;
- after **Capture Impulse**, the response viewer is the **current measurement**
  and therefore authoritative for the graph that is actually running.

The fixture card explicitly says it is not the current capture. The measured
overlay likewise says that disagreements remain visible rather than being
corrected. This prevents the attractive macro sketch from being mistaken for
audio analysis and prevents a checked reference render from being mistaken for
the user's modified graph.

## A/B and principal states

Choose **Gravity Diffusion** from the factory menu to make it the B target.
**A / Barr** loads Barr Reference; **B / Gravity** returns to Gravity Diffusion
without forgetting the selected design. Both choices use ordinary graph
replacement and the existing bounded runtime publication path—there is no
hidden parallel engine.

The principal teaching states are:

| State | Gravity | Checked reference controls |
|---|---:|---|
| Inverse | `-1.00` | Size `+1.00`, Feedback `+0.50`, Damping `+0.15`, Modulation `+0.25` |
| Bloom | `0.00` | Size `-0.35`, Feedback `+1.00`, Damping `0.00`, Modulation `+1.00` |
| Forward | `+1.00` | Size `-0.65`, Feedback `-0.80`, Damping `+0.10`, Modulation `+0.15` |

These are reference comparisons, not forced compound presets when the Gravity
slider moves. The other four Macros remain independent, visible, and editable.

## Verification evidence

Current screenshots and the interaction video are stored under
`artifacts/ui/m9-5-gravity-factory/`. They cover the complete fitted graph,
Inverse/Bloom/Forward teaching states, A/B selection, a continuous Gravity
sweep, impulse measurement, destination highlighting, save, and schema-v2
reload.

The browser workflow recording uses a deterministic local capture transport to
exercise the response UI without depending on an audio device; that temporary
transport is not part of the committed product. Production capture parsing,
publication, and finite audio behavior remain covered by the native and browser
test suites. M9.6 performs the named standalone/VST3 host and package validation.
