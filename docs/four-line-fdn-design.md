# Four-line FDN reference

Status: M21 complete; expanded native architecture and published Dense Room factory.

The reference feedback delay network uses four unequal delay lines at 53.9, 67.7, 79.9, and 97.1 ms. Stereo input is visibly summed and diffused, then injected with alternating signs. Every line has its own moving allpass, delay, low-pass damper, and delay-aware return gain. Two slow LFOs move alternating lines, and unequal signed pickup vectors create stereo output.

## Normalized Hadamard feedback

The feedback matrix is the normalized four-by-four Hadamard matrix. Every coefficient is exactly `+0.5` or `-0.5`, so the matrix is orthogonal, preserves vector energy within floating-point tolerance, and is its own inverse. M21.1 intentionally expands the matrix into 16 ordinary Gain blocks and 12 ordinary two-input Sum blocks. This is visually large but proves that no hidden matrix DSP exists. M21.2 will add a compact Matrix Mixer compound while preserving this expanded form as the authoritative equivalent view.

The matrix redistributes energy but does not set decay. Each line independently uses `10 ^ (-3 * lineTraversalSeconds / T60)` after its damping filter. The requested low-frequency RT60 range is 0.35–8 seconds; damping intentionally shortens high-frequency decay, and every return remains capped below unity.

## Matrix Mixer inspection policy

The editor recognizes the exact 16 Gain and 12 Sum blocks and may replace them
visually with one four-input/four-output Matrix Mixer. Selecting it exposes all
coefficients, explicit `+`/`−` polarity, row/column energy, and orthogonality.
Expand restores the complete equivalent routing. Saving, publishing, audio
execution, and undo always use the original ordinary blocks; the compound is
presentation only. A row or column with energy above unity is left expanded
and reported as amplifying. The editor never silently normalizes a matrix.

## Dense Room tuning and controls

The published factory has 84 nodes and 118 cables. Four tank delays are 53.9,
67.7, 79.9, and 97.1 ms; input diffusion, one moving diffuser per line, unequal
signed injection/pickup vectors, line-specific damping, and three output
allpasses per channel reduce obvious repeats. Size varies the tank delays by
±18%, Decay varies the delay-aware return gains by ±12% within the safe cap,
and Width moves the right pickup vector from mono-compatible matching toward
the wide default. Continuous macro sweeps are smoothed by the shared runtime.

At 48 kHz, the deterministic three-second impulse fixture raises early echo
density from 0.7874 (Dense Figure Eight) to 0.9163 and middle density from
0.9875 to 0.9921. Late recurrence falls from 0.4871 to 0.2770 and late stereo
correlation is 0.0136. These measurements describe the checked-in fixture; they
are not generalized listening claims.

## Determinism and safety

All feedback cycles cross the four explicit Delay blocks. Tests require the
84-node/118-cable graph to compile as one legal feedback region, serialize
exactly through schema v2, fit the fixed delay-memory budget, remain finite and
sub-unity at 44.1/48/96 kHz and control extremes, and render sample-identically
with 64- and 257-sample host partitions. Reset reproduces the original impulse
exactly; macro stress also proves bounded output and non-cancelling mono sum.
