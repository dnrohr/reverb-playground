# Assisted delay-set tuning

## Deterministic candidate generation

M22.1 searches four-line delay sets before any audio rendering. The checked-in
configuration covers 35–120 ms with at least 4 ms between lines, reserves 2 ms
per line for modulation, plans memory at the highest qualified 96 kHz rate,
and calculates the line gain needed for a 2.4-second RT60. A fixed 64-bit seed,
0.1 ms quantization, fixed attempt count, deterministic sorting, and a
lexicographic tie-break make repeated reports byte-identical.

Every eligible set retains three independent diagnostics:

- **common-factor penalty** detects sample-length divisors that reinforce a
  shared recurrence;
- **repeated-difference penalty** detects equal or near-equal spacing between
  multiple line pairs;
- **near-period penalty** detects line-length ratios close to integer periods.

The total penalty ranks pre-render candidates, but the individual dimensions
remain visible. It is only a search heuristic: M22.2 must render and measure
each surviving candidate rather than treating a low arithmetic score as proof
of good sound.

## Admission and rejection

Invalid bounds, insufficient range for four spaced lines, duplicate samples,
insufficient spacing, missing modulation headroom, and delay-memory overflow
are rejected before a graph or audio runtime is created. The report records
each rejection count, the exact seed/configuration, planned bytes, selected
delays, component scores, and calculated RT60 gains. There is no hidden retry
with relaxed constraints.

The authoritative report is
[`artifacts/measurements/m22-delay-set-candidates.json`](../artifacts/measurements/m22-delay-set-candidates.json).
Regenerate it after intentionally changing the algorithm:

```powershell
.\scripts\generate_delay_set_candidates.ps1 -Configuration Release
```

Tests compare the report byte-for-byte, prove repeated searches agree, exercise
intentionally irregular/common-factor/equal-difference/integer-period fixtures,
and prove invalid or one-byte-budget searches admit no renderable candidate.
