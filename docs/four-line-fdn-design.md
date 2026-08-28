# Four-line FDN reference

Status: M21.1 expanded native architecture.

The reference feedback delay network uses four unequal delay lines at 53.9, 67.7, 79.9, and 97.1 ms. Stereo input is visibly summed and diffused, then injected with alternating signs. Every line has its own moving allpass, delay, low-pass damper, and delay-aware return gain. Two slow LFOs move alternating lines, and unequal signed pickup vectors create stereo output.

## Normalized Hadamard feedback

The feedback matrix is the normalized four-by-four Hadamard matrix. Every coefficient is exactly `+0.5` or `-0.5`, so the matrix is orthogonal, preserves vector energy within floating-point tolerance, and is its own inverse. M21.1 intentionally expands the matrix into 16 ordinary Gain blocks and 12 ordinary two-input Sum blocks. This is visually large but proves that no hidden matrix DSP exists. M21.2 will add a compact Matrix Mixer compound while preserving this expanded form as the authoritative equivalent view.

The matrix redistributes energy but does not set decay. Each line independently uses `10 ^ (-3 * lineTraversalSeconds / T60)` after its damping filter. The requested low-frequency RT60 range is 0.35–8 seconds; damping intentionally shortens high-frequency decay, and every return remains capped below unity.

## Determinism and safety

All feedback cycles cross the four explicit Delay blocks. Tests require the 75-node/100-cable graph to compile as one legal feedback region, serialize exactly through schema v2, fit the fixed delay-memory budget, remain finite and sub-unity at 44.1/48/96 kHz and control extremes, and render sample-identically with 64- and 257-sample host partitions. Reset must reproduce the original impulse exactly.

This task adds the native expanded reference only. The Matrix Mixer presentation, expansion interaction, reject/normalize policy, factory controls, teaching overlays, density comparison, and UI evidence belong to M21.2 and M21.3.
