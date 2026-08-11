# Envelope Follower and Hold Gate

M6.2 adds the two reverb-specific primitives required for an inspectable level-gated construction. They do not add lookahead, source recording, arbitrary logic, a hidden detector, or a general-purpose modular control system. The causal Reverse Envelope selected in M6.1 still uses only the existing Delay, Gain, Sum, Allpass, and Low-pass blocks.

## Envelope Follower

The block has one mono audio input and one normalized mono control output. For finite input `x[n]`, the detector level is `min(abs(x[n]), 1)`. A one-pole envelope uses the attack coefficient when that level rises and the release coefficient when it falls:

`e[n] = level[n] + exp(-1 / (sampleRate * timeSeconds)) * (e[n - 1] - level[n])`

Attack is editable from 0.1 to 500 milliseconds and defaults to 5 ms. Release is editable from 1 to 5,000 milliseconds and defaults to 100 ms. Output is finite and clamped to `0...1`; a non-finite detector input is treated as zero. Reset clears the envelope. Coefficients are prepared at the host sample rate, and processing performs no allocation, locking, logging, or topology work.

The block visibly states `AUDIO -> ENVELOPE 0...1`. Its solid audio input and dashed control output make the signal-domain change inspectable without relying on colour.

## Hold Gate

The block has one mono audio input/output and a separate mono control input. It compares finite control, clamped to `0...1`, against an explicit threshold. Control must **exceed** the threshold, so silence remains closed even at a zero threshold. A trigger ramps gain linearly toward one over attack time and reloads the hold counter on every above-threshold sample. Once control falls, gain remains unchanged for the complete hold interval, then ramps linearly to zero over release time. Retriggering during hold or release immediately returns to the attack state from the current gain.

Defaults and editable bounds are:

| Parameter | Default | Range |
|---|---:|---:|
| Threshold | 0.5 | 0...1 |
| Attack | 2 ms | 0.1...100 ms |
| Hold | 250 ms | 1...2,000 ms |
| Release | 20 ms | 0.1...1,000 ms |

Millisecond intervals convert with nearest-sample rounding at preparation. The gain is always bounded to `0...1`, reset closes the gate and clears hold state, and the block performs `audio * control-derived gain`; it cannot amplify or create feedback energy by itself. The node visibly states `AUDIO x CONTROL GATE` and exposes the detector cable separately, rather than hiding it inside the audio block.

## Restricted control route

The Hold Gate control accepts an Envelope Follower directly or through exactly one existing Scale / Offset block. An LFO or another general control source is rejected during graph compilation. An Envelope Follower output may drive only a Hold Gate or one intervening Scale / Offset block, and an envelope-fed mapper may drive only a Hold Gate. That mapper uses base scale, offset, and polarity only; nested parameter modulation is rejected rather than silently ignored. This restriction keeps the pair focused on gated-reverb construction while preserving an explicit place to adjust detector range and polarity.

Attack, release, hold, and threshold are base controls in M6.2; they do not advertise parameter-modulation sockets. Schema v2 therefore permits a parameter to omit its optional `modulation` object, matching the native graph model. Existing modulated patches retain and round-trip every mapping field exactly.

## Runtime and safety

Graphs containing either primitive use the runtime's already bounded sample-wise schedule so the follower observes upstream audio causally and the gate observes that sample's visible control result. All buffers and processor state are prepared before publication. Ordinary graphs keep their block schedule, and delayed feedback still uses split-phase Delay read/write processing.

Deterministic tests cover attack/release equations, exact attack/hold/release samples, reset, silence and non-finite control, 44.1/48/96 kHz timing, direct and mapped control routes, unsupported-source rejection, schema/browser/native round trips, and a Hold Gate inside a legal delayed feedback loop. The feedback fixture remains finite, never exceeds its injected peak, and passes the numerical safety guard.

## Milestone boundary

M6.2 supplies primitives, not presets. M6.3 will build and measure distinct Reverse Envelope and Gated factory patches from visible blocks. A true sample-order reverse, lookahead pre-echo, convolution, and polyphonic trigger policy remain deferred under the M6.1 requirements.
