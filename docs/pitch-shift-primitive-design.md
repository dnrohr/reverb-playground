# Visible Pitch Shift primitive design

M10.1 fixes the behavioral and resource contract for the smallest public
pitch-shifting primitive that can support the later shimmer graphs. The block
is one **mono input to one mono output** dual-read-head granular processor. It
does not conceal a reverb, stereoizer, feedback path, dry mix, or factory-only
behavior. A stereo construction uses two visible blocks and two mono cables.

This document is the contract for M10.2-M10.4. M10.2 implements the prepared
mono DSP as `reverb::dsp::PitchShift`; graph/runtime/editor exposure remains
deferred to M10.3.

## Parameters and units

| Parameter | Unit and range | Default | Meaning |
|---|---:|---:|---|
| Semitones | `-24...+24 st` | `+12 st` | Musical transposition, clamped before conversion to ratio |
| Grain length | `20...120 ms` | `60 ms` | Duration of one read-head cycle and its splice opportunity |
| Overlap | normalized `0.10...1.00` | `0.50` | Width of the equal-power handoff region as a fraction of a half-cycle |
| Direction | `forward` / `reverse` | `forward` | Playback direction inside each grain |

The exact musical ratio is

```text
ratio = 2^(clamp(semitones, -24, +24) / 12)
```

Therefore `-24`, `-12`, `0`, `+12`, and `+24` semitones mean ratios `0.25`,
`0.5`, `1`, `2`, and `4`. A non-finite control value resolves to the `+12 st`
default before conversion. Semitone automation is smoothed in semitone/log-
ratio space, not linear ratio space, so a midpoint remains perceptually midway.

“Overlap” does not change the number of heads and is not an undocumented grain
density control. The two heads remain one half-cycle apart. The value changes
the width of their equal-power crossfade around each handoff: `0.10` is a short
splice and `1.00` uses the complete half-cycle. The implementation normalizes
the two window gains so their squared sum is one wherever both heads are
active. This permits a conservative instantaneous magnitude ceiling of
`sqrt(2)`, recorded as `1.414214` for tests and feedback planning.

## Causal read geometry and direction

The write head always advances with live input. A forward grain advances its
read position by `+ratio` samples per output sample; a reverse grain advances
by `-ratio`. Equivalently, forward delay changes by `1 - ratio` and reverse
delay changes by `1 + ratio`. At a splice, the inactive head is repositioned
inside already-written history and crossfaded in. Neither direction may read
the current unwritten sample or future input.

Reverse therefore means **the samples inside each finite grain run backward**.
It does not mean whole-response reversal, reversed impulse-response
convolution, offline reversal, or wet output before the input event. Both modes
remain causal and begin no earlier than the declared latency. A reverse grain
can soften or reverse local attacks, but it cannot create true pre-echo.

At the worst permitted ratio (`4`) and grain length (`120 ms`), reverse playback
needs `(1 + 4) * 120 = 600 ms` of historical excursion. The implementation uses
that worst case for one fixed latency at a given sample rate. Keeping latency
fixed prevents grain or semitone automation from asking the host to change
delay compensation while audio is running.

## Latency and prepared storage

For sample rate `Fs`, the mono block reports:

```text
latencySamples = ceil(0.600 * Fs) + 2
excursionSamples = ceil(0.600 * Fs)
ringSamples    = latencySamples + excursionSamples + 2
ringBytes      = ringSamples * sizeof(float)
```

The reported latency is the newest legal read. A head then moves up to 600 ms
farther into history, so the ring must cover both quantities; the original
M10.1 draft counted only one and was corrected during M10.2 implementation.
The four total guard samples cover the linear interpolator at both ends of the
legal range. The input ring is shared by both heads and by old/new head state
during parameter crossfades; a parameter transition does not allocate or
double the audio history.

| Rate | Reported latency | Ring allocation |
|---:|---:|---:|
| 44.1 kHz | `26,462 samples` | `52,924 samples / 211,696 bytes` |
| 48 kHz | `28,802 samples` | `57,604 samples / 230,416 bytes` |
| 96 kHz | `57,602 samples` | `115,204 samples / 460,816 bytes` |
| 192 kHz maximum | `115,202 samples` | `230,404 samples / 921,616 bytes` |

Preparation accepts finite rates from 22.05 through 192 kHz. The qualified
audio fixtures are 44.1, 48, and 96 kHz. An unsupported rate or an allocation
that would exceed the graph budget fails before publication and leaves the
last valid runtime audible. During the existing two-runtime topology crossfade,
one maximum-rate Pitch Shift contributes at most `1,843,232` ring bytes across
both runtimes.

Pitch Shift latency participates in graph latency analysis. Parallel dry or
ordinary-reverb branches must use a visible compensating Delay before
recombination; the block never hides dry alignment.

## Real-time work ceiling

The implementation budget includes circular addressing, four-point bounds
bookkeeping around two linear reads, window evaluation/normalization, smoothing,
and output mixing. It is deliberately conservative rather than a CPU claim:

| State | Ceiling |
|---|---:|
| Normal two-head processing | `72 scalar/buffer operations per sample` |
| 20 ms parameter transition, old and new head states | `136 operations/sample + 64/block` |
| Existing topology crossfade with both runtimes transitioning | `272 operations/sample + 128/block` |

At 192 kHz the single-runtime transition ceiling is 26.112 million operations
per second, excluding the small per-block term. M10.4 replaces this planning
ceiling with measured CPU results. `prepare` owns allocation and validation;
`process` and `reset` are bounded and `noexcept`, with no allocation, locks,
logging, filesystem access, resizing, or topology discovery.

## Automation, reset, and modulation

- Semitones ramp over 20 ms in semitone space. A mapped control socket may
  target semitones only within `-24...+24`.
- Grain length and overlap changes create a 20 ms equal-power crossfade between
  old and new head states. Both states read the same prepared ring.
- Direction is a discrete control. A forward/reverse change uses the same 20 ms
  state crossfade; it is not sample-stepped or continuously modulatable.
- The M10.3 public block may expose parameter sockets for semitones, grain
  length, and overlap. Existing 1 kHz control ticks feed the audio-rate
  smoothing above.
- Reset zeroes the prepared ring, write index, interpolation history, window
  phases, transition state, and smoothing history. Head phases restart at `0`
  and `0.5`; current values snap to serialized targets. There is no random seed
  in the first mono primitive.
- Silence after reset must remain bit-exact silence. Identical configuration,
  input, block partition, and reset must produce identical samples on the
  primary MSVC toolchain.

## Quality limits and executable expectations

The first implementation uses linear interpolation and windowed time-domain
splicing; it is not a phase vocoder and is not band-limited resampling.
Transposition can add window sidebands, transient smearing, and aliases,
especially for upward shifts near Nyquist. The UI must disclose this rather
than call the block transparent or studio quality. Factory shimmer paths should
high-pass before shifting where useful and low-pass afterward to prevent
recirculating aliases.

M10.2 and M10.4 use these pass/fail rules:

- For steady sines from 110 Hz through 1 kHz whose target remains below 40% of
  Nyquist, `+12` and `-12` semitone peaks must be within 15 cents of the expected
  octave at 44.1, 48, and 96 kHz.
- With a full-scale 1 kHz sine, the largest adjacent output-sample step across
  a grain handoff or supported parameter transition must remain below `0.25`.
- Exact silence must produce exact silence. Input bounded to `[-1, +1]` must
  stay finite and within `[-1.414214, +1.414214]` for every parameter endpoint.
- Tests must distinguish ratio pitch shifting from fixed-Hz frequency shifting
  and from Doppler motion made by a modulated Delay.
- Aliasing measurements report energy above the expected target band rather
  than hiding it behind a subjective quality label.

## Feedback-use boundary

Pitch Shift supplies no feedback internally. Every feedback cycle must still
cross an explicit Delay and pass the existing graph validator. Because the
equal-power mix can peak at `sqrt(2)`, a shifted feedback branch is initially
limited to gain `0.60`; after any branch normalization, the sum of absolute
normal and shifted return gains must not exceed `0.85`. These are conservative
factory/inspector limits for M10-M12, not a proof that arbitrary user graphs are
stable. The numerical-safety latch, emergency mute, last-valid publication,
10 ms topology crossfade, Undo, and explicit recovery remain mandatory.

M10.4 must exercise forward and reverse grains in a delayed feedback harness
with silence, impulse, bounded noise, continuous edits, and every qualified
sample rate before a shimmer factory patch can ship.

## M10.2 implementation

`PitchShift::prepare` either owns the exact ring allocation or accepts an
exactly sized caller-owned span. Both paths reject non-finite or unsupported
sample rates and mismatched storage before processing. `process`, `reset`,
`setParameters`, and `settleParameters` are `noexcept` and do not allocate,
resize, lock, log, or discover topology.

The two heads are one half-cycle apart. Each head follows the specified delay
slope, uses linear interpolation, and reaches zero window contribution when its
read ramp wraps. The overlap control narrows or widens a sine/cosine
equal-power handoff around the two crossover points. A fixed `1/sqrt(2)`
headroom factor keeps simultaneous coherent heads and old/new state transitions
inside the documented output ceiling without a signal-dependent normalizer.

The prepared implementation now has executable checks for:

- `+12` and `-12 st` octave identity using a phase-coherent 400 Hz fixture at
  44.1, 48, and 96 kHz;
- exact silence, bounded alternating input, every parameter endpoint, both
  directions, and caller-owned canaries around the prepared ring;
- no impulse output before reported latency;
- bit-identical reset/re-render behavior and a complete forward/downward to
  reverse/upward parameter transition;
- an adjacent-sample ceiling below `0.25` across grain handoffs and the
  documented 20 ms transition.

The phase-coherent tone is intentionally a narrow M10.2 identity fixture.
M10.4 still owns broad tone/chord spectra, sideband and alias-energy reporting,
CPU measurements, and delayed-feedback safety evidence.

## Deferred implementation details

M10.1-M10.2 do not choose a visual animation style, tune a shimmer sound, or
claim measured CPU/aliasing performance. M10.3 adds schema/runtime/editor
integration and honest visualization; M10.4 records broad spectral, latency,
CPU, storage, aliasing, and feedback-safety evidence.
