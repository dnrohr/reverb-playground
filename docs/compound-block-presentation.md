# Compound-block presentation

M26.5 established the original rule that a compound is a derived view over
ordinary primitives, never a hidden DSP node. M29 keeps that authority rule and
replaces inline expansion with a saved movable parent and a live nested canvas.
The complete current contract is in
[Hierarchical schematics](hierarchical-schematics.md).

## Authority

The saved and executable graph remains the exact primitive node and cable
arrays. A compound recognizer supplies stable membership, a display name, and
boundary mappings. Its parent and nested Input/Output blocks exist only in the
editor projection. Automation, compilation, latency, safety, Energy, audition,
offline rendering, and host restore address the primitive IDs.

The parent position, name, collapse state, named proxy ports, member IDs, and
nested viewport are serialized as optional schema-v2 layout data. Presentation
changes therefore affect the document/save hash but never its semantic hash or
rendered samples.

## Matrix qualification

The Four-Line Dense Room's 4×4 Matrix Mixer is the first qualified compound.
It owns 16 finite signed Gain coefficients and 12 explicit Sums. Its four input
proxies each fan out to four explicit Gain-input cables from one external
source; its four output proxies map one-to-one to the final Sum outputs. The
compact parent therefore has eight ordinary mono ports while retaining all 20
crossing cable identities in its boundary metadata.

The recognizer admits normalized orthogonal and non-amplifying matrices. If a
row or column energy exceeds unity, the edited primitives remain visible and no
automatic normalization conceals the change. Complete copies receive fresh
primitive, cable, hierarchy, and boundary IDs. Partial or structurally changed
copies become ordinary graph material.

Parent Energy and loop/safety/focus decoration are derived from members without
double counting. Inspect reports the authoritative primitive/cable counts,
stable proxy count, Matrix coefficients, row/column energy, compiled-latency
authority, and active warnings; opening the nested canvas reveals the exact
primitive or feedback path behind that summary.

## Historical M26.5 evidence

The former inline presentation remains documented by:

- `artifacts/ui/m26-5-compound-presentation/matrix-summary-inspector.png`
- `artifacts/ui/m26-5-compound-presentation/matrix-expanded-authority.png`
- `artifacts/ui/m26-5-compound-presentation/compound-expand-copy.mp4`
- `artifacts/ui/m26-5-compound-presentation/compound-640x400.png`

Current M29 evidence is under
`artifacts/ui/m29-hierarchical-compounds/`.
