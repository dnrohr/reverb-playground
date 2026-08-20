# Split-Feedback Shimmer topology design

M12.1 introduces the feedback architecture that distinguishes classic evolving
shimmer from the stable parallel halo shipped in M11. The graph is authored by
`makeSplitFeedbackShimmerGraph` and uses only the public mono-cable primitives.
There is no hidden shimmer or feedback processor.

## Shared tank and split returns

Stereo input first becomes the deterministic mono sum `0.5 * left + 0.5 *
right`, followed by 4.7 and 8.9 ms input Allpasses. The shared tank is:

`Sum -> Allpass 13.7 ms -> Delay 149 ms -> Allpass 23.9 ms -> Low-pass 6.5 kHz`

The damped tank output feeds the wet stereo extraction and also splits into two
explicit returns:

- **Normal return:** Gain `normal-feedback` -> Delay 67 ms -> feedback Sum.
- **Shifted return:** subtractive high-pass -> Pitch Shift +12 semitones ->
  Low-pass -> Gain `shifted-feedback` -> Delay 83 ms -> feedback Sum.

The feedback Sum returns to the shared tank-entry Sum. Unequal 11.9 and 19.7 ms
Allpasses create related but non-identical left and right wet outputs. Every
inter-block connection remains a mono cable.

## Independent responsibilities

Normal Feedback defaults to 0.48 and is limited to 0...0.58. It changes the
ordinary decay even when Shifted Feedback is zero. Shifted Feedback defaults to
0.10 and is limited to 0...0.14; it controls how much +12-semitone material
returns for another circulation. It does not own the ordinary decay path.

At their simultaneous maxima the two visible return gains sum to 0.72. This is
a conservative operating ceiling, not a mathematical peak guarantee for every
correlated or modulated signal. The runtime numerical guard remains the final
authority for non-finite or runaway output.

## Visible anti-accumulation filtering

The pre-shift high-pass is assembled as `x - Low-pass(x)` using a Sum and an
inverting Gain. Its default corner is 320 Hz and its supported construction
range is 120...1,200 Hz. Because the public Low-pass is one pole, this
subtractive complement has an approximately 6 dB/octave low-frequency slope.

The post-shift damping block is a one-pole Low-pass at 5.2 kHz by default, with
a 1.2...9 kHz construction range and an approximately 6 dB/octave high-frequency
slope. It darkens every shifted circulation before gain and return. These
filters reduce unsuitable low-frequency transposition and repeated high-frequency
or folded-alias buildup; they do not make the linear-interpolation Pitch Shift
alias-free.

## Cycle legality and failure behavior

Both returns belong to one strongly connected component around the shared tank,
but they remain separately traceable. The normal cycle crosses the 149 and 67 ms
Delays. The shifted cycle crosses the 149 and 83 ms Delays as well as Pitch
Shift. The existing feedback compiler accepts the graph because no algebraic
sub-loop exists.

A checked regression removes both the tank and shifted-return Delays to create
an algebraic shifted sub-loop. Publication rejects that edit and the previously
compiled runtime continues to produce audio. Extreme legal settings render
finite, below-full-scale output at 44.1, 48, and 96 kHz. Existing emergency mute,
state reset, and topology-crossfade mechanisms remain applicable without any
audio-thread allocation, locking, or graph work.

## M12 boundary

M12.1 establishes the editable native topology and its safety invariants. It
does not add a factory-menu entry or change the editor, so no UI capture is
required. M12.2 will add time-resolved spectral evidence for cumulative +12 and
+24-semitone ascent, automation qualification, alias disclosure, and stereo
measurements. M12.3 will tune and publish the factory patch, teaching overlay,
screenshots, and interaction video.
