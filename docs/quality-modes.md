# Processing quality modes

Reverb Playground stores one explicit `qualityPolicy` in every newly saved patch. The choice travels with the graph through the standalone application, VST3 host state, offline export, and reopen. Older schema-v2 patches without the field open as **Normal**, so an existing sound never changes merely because this feature was added.

## Policies

| Policy | Pitch Shift interpolation | Pitch latency and storage | Modulation resolution | UI telemetry | Intended use |
|---|---|---|---|---|---|
| Draft | Nearest sample | Unchanged | 1 kHz with audio-rate ramps | 30 Hz | Lowest-cost construction and routing checks; rougher fractional-delay motion and more imaging/alias risk are expected. |
| Normal | Linear | Unchanged | 1 kHz with audio-rate ramps | 30 Hz | Released sound and default for new and legacy patches. |
| High | Four-point cubic | Unchanged | 1 kHz with audio-rate ramps | 30 Hz | Auditioning and export when smoother fractional-delay interpolation is worth the extra work. Cubic interpolation can overshoot between samples; the existing safety limiter remains authoritative. |

The policy dimensions are deliberately independent. This first bounded implementation changes only Pitch Shift interpolation: it does not quietly lower control motion, visualization responsiveness, window size, storage, or declared latency. That makes comparisons honest and keeps automation and graph timing identical among modes.

The selector is in the editor header beside the factory-patch selector. Its labels disclose the main tradeoff before selection. Changing it republishes the visible graph using the same safe off-thread compile and block-boundary crossfade as other audible topology edits.

## Musical interval contract

Pitch Shift remains continuously adjustable from -12 through +12 semitones in every policy. A perfect fifth (`+7` or `-7`) follows the same ratio, grain, window, and interpolation path as every other setting. There is no octave-only or fifth-only shortcut and no interval-dependent hidden algorithm.

## Measurements and listening comparison

The checked-in [machine-readable validation](../artifacts/measurements/pitch-shift-validation-v1.json) measures Draft, Normal, and High with identical 48 kHz, 256-sample-block, one-second forward and reverse fixtures. It records interpolation, declared pitch accuracy, a folded-alias probe, elapsed time, and real-time load. On the qualification machine Draft measured about 0.077% realtime load versus 0.106-0.116% for Normal. The short High result overlapped measurement noise with Normal, so no claim that High is always slower or faster is made.

The spectral probe confirms equal pitch placement but is not a perceptual quality score: its folded component is nearly identical among these particular octave fixtures. The automated output-distinctness tests and offline factory render are the canonical listening fixtures for comparing the modes; audition them at matched gain, especially on bright sustained material and moving pitch parameters. Normal remains the release reference and its existing golden renders remain unchanged.

## Compatibility guarantees

- Quality is graph state, not a machine preference.
- Native and web readers reject unknown policy names.
- Factory patches write `normal` explicitly.
- Host-state and offline-render tests prove the selected policy survives and controls the compiled Pitch Shift implementation.
- All modes keep the same declared Pitch Shift latency; mode switching does not create a hidden host-latency change.

