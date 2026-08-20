# Visible Pitch Shift primitive design

M10.1 fixes the behavioral and resource contract for the smallest public
pitch-shifting primitive that can support the later shimmer graphs. The block
is one **mono input to one mono output** dual-read-head granular processor. It
does not conceal a reverb, stereoizer, feedback path, dry mix, or factory-only
behavior. A stereo construction uses two visible blocks and two mono cables.

This document is the contract for M10.2-M10.4. M10.2 implements the prepared
mono DSP as `reverb::dsp::PitchShift`; M10.3 exposes that same processor through
the public graph, editor, persistence, and host-state paths.

## Parameters and units

| Parameter | Unit and range | Default | Meaning |
|---|---:|---:|---|
| Semitones | `-24...+24 st` | `+12 st` | Musical transposition, clamped before conversion to ratio |
| Grain length | `20...120 ms` | `60 ms` | Duration of one read-head cycle and its splice opportunity |
| Overlap | normalized `0.10...1.00` | `0.50` | Width of the equal-power handoff region as a fraction of a half-cycle |
| Direction | `forward` / `reverse` | `forward` | Playback direction inside each grain |
| Phase | `0.000...0.999 cycles` | `0` | Deterministic starting phase for paired mono instances |

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
- Phase is a saved, non-modulated start offset. The two heads remain one
  half-cycle apart. Pairing `0.000` and `0.373` produces different grain
  boundaries without a random generator or hidden stereo state.
- Reset zeroes the prepared ring, write index, interpolation history,
  transition state, and smoothing history. Head phases restart at the saved
  phase and `phase + 0.5`; current values snap to serialized targets.
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

## M10.3 visible graph and inspector

The public `pitch-shift` node has one mono audio input and output. Three typed
control inputs map semitones, grain length, and overlap through the existing
saved scale/polarity/clamp contract; direction is a saved discrete choice and
is intentionally not continuously modulatable. All five values and every
mapping field live in schema-v2 graph data. There is no factory-only pitch
state. M13.1 adds the saved Phase control; released schema-v2 Pitch Shift nodes
with four parameters migrate deterministically to phase `0`, while newly saved
nodes contain all five parameters. Schema-v1 documents continue to migrate
without inventing the node.

The acyclic runtime allocates the processor's exact prepared ring from the
existing delay arena and reports its fixed latency and allocation through the
same resource plan as Delay and Allpass. Pitch Shift does not legalize a cycle:
feedback still requires a visible Delay. Constant and mapped controls use the
same `PitchShift` parameter entry points as direct DSP use.

The block says **Musical ratio · not frequency shift**. Its inspector exposes
the five saved controls plus read-only `Dual grain · linear interpolation`
quality and the rate-derived latency. Two moving head markers are explicitly
labeled **Illustrative grain phase** and **Design-state animation**. Their phase
comes from saved controls, not measured audio or sample-accurate telemetry.
Reduced-motion mode fixes both markers in an inspectable static state; the OS
reduced-motion media query provides the same fallback before application state
is available.

Reviewed evidence:

- [Pitch Shift block and grain explanation](../artifacts/ui/m10-3-visible-pitch-shift/01-pitch-shift-block-and-grains.jpg)
- [Lower inspector controls and direction](../artifacts/ui/m10-3-visible-pitch-shift/02-pitch-shift-complete-inspector.jpg)
- [Continuous semitone edit and moving-grain recording](../artifacts/ui/m10-3-visible-pitch-shift/pitch-shift-continuous-edit.mp4)

The recording shows the editor's bound 48 kHz status while semitones move from
`-12` through `+12` and both illustrative heads continue moving. Native graph
and processor tests, rather than the visual recording, are authoritative for
finite audio, exact DSP binding, and host restoration.

## M10.4 octave, resource, and safety validation

The checked format-v1 report at
[`artifacts/measurements/pitch-shift-validation-v1.json`](../artifacts/measurements/pitch-shift-validation-v1.json)
is generated by the Release measurement executable:

```powershell
build/windows-msvc/src/render/Release/reverb_pitch_shift_validation_cli.exe `
  --output artifacts/measurements/pitch-shift-validation-v1.json
```

It covers the only first-release quality, `dual-grain-linear-v1`, in both grain
directions and at every qualified rate. CPU is one second of 256-sample Release
processing measured with the monotonic clock on the validation workstation;
it is a comparative result, not a cross-machine guarantee.

| Rate | Latency | Storage | CPU forward / reverse | Folded alias vs 400→800 Hz reference, forward / reverse |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 26,462 / 600.05 ms | 211,696 bytes | 0.194% / 0.187% | -6.07 / -6.03 dB |
| 48 kHz | 28,802 / 600.04 ms | 230,416 bytes | 0.220% / 0.226% | -0.001 / -0.001 dB |
| 96 kHz | 57,602 / 600.02 ms | 460,816 bytes | 0.438% / 0.466% | -0.001 / -0.001 dB |

The alias fixture sends a sine at `0.35 × Fs` through `+12 st`: its desired
`0.70 × Fs` result exceeds Nyquist and folds to `0.30 × Fs`. Linear
interpolation does not suppress that fold. At 48 and 96 kHz the folded line is
essentially as strong as the ordinary 400→800 Hz reference line; 44.1 kHz is
about 6 dB lower for this grain/rate alignment. This is a disclosed quality
limit, not a pass implying transparency. Shimmer graphs must band-limit before
and after shifting, and shifted feedback must darken each circulation.

Executable spectral fixtures also render a 220/277.18/329.63 Hz chord at
44.1, 48, and 96 kHz. Each expected +12-semitone band dominates its unshifted
band by at least 4:1. Separate 330 and 550 Hz inputs land around 660 and
1,100 Hz, proving different hertz offsets rather than fixed-hertz translation.
A 440 Hz moving Delay remains concentrated around 440 Hz and has negligible
880 Hz energy compared with the Pitch Shift output, separating controlled
Doppler sidebands from ratio transposition.

The reusable feedback fixture is `input + delayed return → Pitch Shift → 0.35
Gain → 11 ms Delay → return`. The explicit Delay is what makes the cycle legal.
Impulse plus deterministic bounded noise remains finite below unity peak for
two seconds in forward and reverse modes at all qualified rates. A direction
edit completes the existing 10 ms two-runtime crossfade. Dedicated guard and
processor tests prove numerical latching, manual emergency mute, zeroed output,
state-clearing recovery, and resumed silence while Pitch Shift remains inside
the active loop.

M10.4 changes tests, measurement tooling, and documentation only, so the UI
evidence policy requires no new screenshot or video. M10.3 remains the current
visual evidence for the block.

## M13.1 reverse grains and deterministic stereo pairing

M13.1 makes the head start phase explicit from DSP through graph compilation,
schema persistence, host restoration, and the inspector. It is not a random
seed: equal phase, input, settings, and reset produce bit-identical output.
Two visible mono blocks may instead use `0.000` and `0.373 cycles`, the reference
pair for the later cosmic topology. That changes splice timing while retaining
ordinary mono cables and reproducible recalls.

The same checked measurement report now records, at 44.1, 48, and 96 kHz:

- zero wet samples before the fixed declared latency in both paired instances;
- bit-identical repeated reverse renders after reset;
- paired deterministic-noise correlations of `-0.00426`, `0.00170`, and
  `-0.00008`, all below the `|0.95|` decorrelation ceiling;
- normalized forward/reverse 2 ms-smoothed transient-envelope differences of
  `1.358`, `1.350`, and `1.350`, proving that reverse grains substantially
  reshape local attacks even where the maximum envelope step is essentially
  unchanged;
- separate forward/reverse octave error, CPU, storage, latency, aliasing, and
  the existing delayed-feedback safety fixtures.

Reverse grains still cannot reverse the complete wet response or emit pre-echo.
The phase markers remain an illustrative design-state view rather than audio
telemetry. M13.2 owns the first factory topology that uses the paired setting.
