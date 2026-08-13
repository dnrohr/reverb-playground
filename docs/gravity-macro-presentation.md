# Gravity macro presentation

M8.4 presents a factory-designated Macro as a prominent bipolar Gravity
instrument while keeping the saved Macro, Curve Mappers, destination blocks,
and dashed cables as the executable source of truth.

## Explicit designation

A schema-v2 Macro may carry `"presentation": "gravity"`. The designation is
saved in browser documents and native host state, participates in history and
clipboard snapshots, and has no DSP meaning. It is accepted only on a Macro;
unknown values or presentation fields on other node types fail before runtime
publication. Ordinary user-created Macros remain ordinary unless a factory
document explicitly designates one.

## Control surface

The surface shows **INVERSE** at `-1`, **BLOOM** at `0`, and **FORWARD** at
`+1`. Its large slider uses the Macro's normalized value, fixed 20 ms runtime
ramp, and optional `0.02` center detent. A numeric field remains visible and
keyboard-editable with `0.001` precision. The saved default and detent remain
visible rather than becoming hidden settings.

Moving Gravity retains the M8.3 reachability highlight on its Macro, every
traversed Curve Mapper, destination block, and cable. The mapping list continues
to show exact predicted parameter ranges.

## Predicted envelope

The envelope is labeled **DESIGN PREDICTION / NOT MEASURED AUDIO**. It is a
teaching guide derived only from the normalized coordinate, not an impulse
response or a claim about the current graph's sound. Its normalized peak is:

`peak = 0.48 - 0.35 * gravity`

The pre-peak guide rises with exponent `1.65`; the post-peak guide decays
exponentially across the remaining horizon. This supplies ordered late
Inverse, center Bloom, and early Forward silhouettes without inventing timing,
level, RT60, or causality evidence. The panel directs users to impulse capture
for actual response.

## Expand / Focus and teaching independence

**Expand / Focus** fits the selected Macro plus every reachable Curve Mapper
and destination into the viewport with `0.22` padding and at most `1.1` zoom.
It uses the same bounded traversal that drives highlighting. Reduced-motion
mode makes the move immediate.

Gravity control, exact value, predicted guide, focus, mapping list, and
highlights are outside the optional teaching-card condition. **Learn Off** may
hide contextual cards and response annotations but leaves all Gravity control
and inspection available.

## Evidence

- [Inverse state](../artifacts/ui/m8-4-gravity-presentation/01-gravity-inverse.png)
- [Bloom center](../artifacts/ui/m8-4-gravity-presentation/02-gravity-bloom.png)
- [Forward state with Learn Off](../artifacts/ui/m8-4-gravity-presentation/03-gravity-forward-learn-off.png)
- [Continuous sweep and Expand / Focus](../artifacts/ui/m8-4-gravity-presentation/gravity-sweep-focus.mp4)
