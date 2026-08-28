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

## Rendered response ranking

M22.2 renders the first 16 arithmetic candidates at 48 kHz with four
deterministic sources: an impulse, a short noise burst, a five-hit percussive
pattern, and a two-tone chord. Unnormalized output supplies the measurements;
the listening WAVs are separately peak-normalized to 0.5 (−6.02 dBFS), so
loudness does not masquerade as preference.

Each candidate retains independent pass/fail results for finite/bounded output,
late echo density, late recurrence, spectral coloration, RT60 error, and late
stereo correlation. A failed dimension makes the candidate ineligible before
the aggregate ordering is considered. The aggregate remains published for
transparent ordering among eligible candidates; it never erases a failure.

The deterministic run selected these leading delay sets:

| Rank | Delays (ms) | Density | Recurrence | Coloration penalty | RT60 error | Abs. stereo corr. |
|---:|---|---:|---:|---:|---:|---:|
| 1 | 35.2, 45.9, 72.7, 110.3 | 0.966 | 0.303 | 0.851 | 0.224 | 0.014 |
| 2 | 35.3, 50.3, 67.3, 114.1 | 0.975 | 0.302 | 0.871 | 0.216 | 0.013 |
| 3 | 35.2, 44.7, 53.4, 113.2 | 0.980 | 0.320 | 0.857 | 0.222 | 0.008 |

[`artifacts/measurements/m22-rendered-delay-sets/ranking.json`](../artifacts/measurements/m22-rendered-delay-sets/ranking.json)
contains the full seed/configuration, thresholds, metrics, and ranking. The same
directory contains each top candidate as an ordinary editable schema-v2 patch
plus four normalized stereo PCM16 listening fixtures. Regenerate them with:

```powershell
.\scripts\generate_rendered_delay_set_ranking.ps1 -Configuration Release
```
