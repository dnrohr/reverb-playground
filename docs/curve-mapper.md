# Curve Mapper

M8.2 evolves the visible `control-map` block from Scale / Offset into a bounded
nonlinear Curve Mapper. It remains an ordinary typed control node: one mono
control input, one branchable mono control output, no hidden destinations, and
no audio processing.

## Evaluation order and equations

The block evaluates at the prepared 1 kHz control rate:

`output = clamp(offset + scale * curve(normalize(input)), clampMin, clampMax)`

Normalization first clamps the input to `0..1` in Unipolar mode or `-1..+1` in
Bipolar mode. The supported curve families are:

- **Linear:** `curve(x) = x`. Curve amount and exponent are ignored. This is
  bit-for-bit equivalent to the released Scale / Offset equation.
- **Power:** Unipolar uses `x^exponent`; Bipolar uses
  `sign(x) * abs(x)^exponent`. Exponent is finite and bounded to `0.1..8`.
- **Exponential:** convert Bipolar input to `t = (x + 1) / 2` (Unipolar uses
  `t = x`), then `q = expm1(amount * t) / expm1(amount)`. Amount is `-8..8`.
  At amount zero the exact limiting result is `q = t`. Bipolar output converts
  back with `2q - 1`.

Scale is `-4..4`, offset is `-1..1`, and output clamps are each `-1..1` with
the invariant `clampMin < clampMax`. Negative scale reverses direction, but all
supported curves remain monotonic over either input domain. Finite endpoint
evaluation is sufficient for the inspector's predicted minimum/maximum because
of that monotonic contract.

## Runtime and safety

Compilation rejects fractional/unknown curve selectors, non-finite values,
out-of-range amount/exponent/clamps, and reversed or equal clamps. A failed edit
does not replace the last valid audible runtime. Curve evaluation is scalar,
constant-work, `noexcept`, and allocation-free after runtime preparation.
Prepared control values update at 1 kHz and the existing `ControlRamp` linearly
interpolates connected audio parameters across the following quantum, avoiding
sample discontinuities at control ticks.

The editor preview uses the same equations at a display-only rate. It labels
the family in text (`LINEAR`, `POWER`, or `EXP`), shows a signed live value, and
draws a dashed/double-rule range panel, so identity and bounds do not rely on
color.

## Persistence and compatibility

New schema-v2 Curve Mapper nodes save all eight parameters in stable order:
scale, offset, polarity, curve family, curve amount, exponent, clamp minimum,
and clamp maximum. Round-trip tests preserve every numeric value exactly.

Released schema-v2 Scale / Offset nodes with only the first three parameters
remain readable. The browser loader deterministically supplies Linear, amount
zero, exponent one, and `-1/+1` clamps, then writes the complete Curve Mapper on
the next save. The native compiler accepts the same legacy three-field shape as
that exact Linear behavior. No existing factory patch requires hand editing.

## Evidence

- [Current inspector screenshot](../artifacts/ui/m8-2-curve-mapper/curve-mapper-inspector.png).
- [Linear/Power/Exponential interaction video](../artifacts/ui/m8-2-curve-mapper/curve-mapper-preview.mp4).
