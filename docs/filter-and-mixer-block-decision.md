# Filter and Mixer block decision

M27.4 evaluates two tempting palette shortcuts against the product rule that
the schematic remains the executable explanation. The decision is to ship no
new primitive in this milestone. Keep Low-pass, Gain, and Sum authoritative;
retain the qualified 4×4 Matrix Mixer as a collapsible compound presentation.

## Measured need

The reproducible audit in `scripts/audit_filter_mixer_need.py` examines all
nine released schema-v2 factory graphs. Its checked result is
`artifacts/measurements/m27-filter-mixer-need.json`.

| Measure | Released factories |
| --- | ---: |
| Nodes | 334 |
| Low-pass primitives | 20 |
| Structural `x - Low-pass(x)` high-passes | 3 |
| Band-pass primitives | 0 |
| Explicit two-input Sums | 61 |
| Terminal Sum trees with at least three inputs | 13 |
| Qualified 4×4 Matrix Mixers | 1 |

High-pass is a real recurring need, but all three instances serve the same
shimmer teaching pattern and are already explicit, inspectable, and compiled
from public primitives. No released architecture requires a band-pass. Mixing
is common, but most Sum trees are embedded in feedback, tap weighting, stereo
extraction, or other structures whose boundaries are not interchangeable with
one generic mixer panel.

## Filter alternatives

### Separate Low-pass, High-pass, and Band-pass primitives

This gives each transfer a stable type and avoids a mode switch, but expands
the palette before Band-pass has a demonstrated use. A new High-pass kernel
would duplicate the currently exact complementary construction and introduce
new persistence, modulation, DSP, and factory-equivalence obligations.

### One mode-selectable Filter primitive

A coherent contract is possible:

- Low-pass uses the released one-pole response.
- High-pass is exactly `x - Low-pass(x)` at the same cutoff.
- Band-pass would be a one-pole high-pass followed by a one-pole low-pass with
  explicit lower and upper cutoffs.
- Cutoffs may use the existing 1 kHz control interpolation and coefficient
  clamps below Nyquist; mode itself must not be modulated.
- Mode changes are topology-affecting publications. They rebuild off-thread
  and crossfade because state count and transfer identity change; they are not
  disguised as continuous runtime parameter edits.

However, replacing `lowpass` would require migration to a new type and a
default `mode=lowpass`, while adding `filter` alongside it would duplicate the
same job. Band-pass also needs a two-cutoff UI, ordering validation, two states,
and worst-case preparation. With zero released uses, those costs are not yet
earned. The unified Filter is therefore rejected for now.

A future non-DSP High-pass compound is preferable if visual repetition becomes
a usability problem: it can collapse the exact Low-pass, negative Gain, and Sum
while expansion retains every authoritative primitive and cable.

## Mixer prototype and decision

The released Four-Line Dense Room already supplies the required prototype. Its
Matrix Mixer summary projects one existing graph of 16 signed Gains and 12
two-input Sums. Collapse creates no saved node, DSP kernel, hidden fan-in, or
normalization. Expand restores the same IDs and cables in place.

Consequently its audio, compiled latency, Energy telemetry, numerical safety,
save/load state, feedback-loop inspection, and warnings are identical: those
systems continue to consume the unchanged authoritative primitive graph. Native
qualification compares compilation with and without provenance metadata, while
web tests cover boundary cable identity, expansion, copy/paste, coefficient
energy, and refusal of amplifying matrices.

An arbitrary channel-strip Mixer is rejected for now. The measurement proves
multi-input Sum trees exist, but not that they share stable gain, pan, channel,
normalization, feedback, or boundary semantics. Generalizing solely to reduce
block count would hide meaningful topology. Automatic Sum insertion and visual
groups remain the safer construction tools; new compound recognizers should be
admitted only for repeated, structurally exact patterns.

## Reconsideration gates

Reopen the Filter decision when at least two released architectures require a
Band-pass, or usability sessions repeatedly fail on the explicit complementary
High-pass. Reopen a general Mixer when repeated graphs share a provable
primitive expansion and users need to operate that expansion as one unit—not
merely because a patch contains many Sums.

This task changes no visible UI or audio behavior, so new screenshot/video
evidence is not required. The existing qualified compound evidence remains in
`artifacts/ui/m26-5-compound-presentation/`.
