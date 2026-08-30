# Audio-file source and transport contract

Status: accepted for M14.1 and normative for M14.2-M14.5.

This contract adds familiar source material to the standalone audition path
without turning the patch graph into a file player or changing how the VST3
receives host audio. It extends, and does not weaken, the
[real-time and safety contract](real-time-safety-contract-v1.md).

## Product boundary

The standalone has exactly three mutually exclusive source modes:

- **Live Input** passes the two prepared device-input channels to the graph.
- **Audio File** passes a prepared file transport to the graph and ignores live
  device input.
- **Test Impulse** passes silence except for an explicitly requested bounded
  impulse. Response capture may use its existing `0.1` impulse level and
  manual audition may use its existing unit impulse; the UI labels which
  operation is being requested.

The source selector is an audition setting, not a graph module. Every mode
enters the same public stereo graph boundary, and every cable inside the graph
remains mono. The VST3 always uses host-provided stereo input and does not
expose or restore the standalone file transport.

```text
device input L/R ---- Live Input ---\
prepared file L/R --- Audio File ----+--> source router --> public graph
bounded impulse ----- Test Impulse -/                         |
                                                              v
                         energy / response / diagnostics <--- wet output
                                                              |
                         Dry Gain x source + Wet Gain x graph output
                                                              |
                                      numerical guard / mute
                                                              |
                                      standalone device output
```

Emergency Mute is downstream of every source and the wet/dry sum. No file,
dry, or export preview path may bypass the numerical guard. Impulse-response
capture observes graph wet output before Wet and Dry Gain so measurement does
not depend on listening level. Energy and diagnostics state
whether they describe source, wet graph output, or final audible output.

## Channel and format policy

M14 supports files JUCE can identify as WAV, AIFF, or FLAC:

- one-channel input is copied to both graph input channels;
- two-channel input preserves channel order and values through source-rate
  conversion; and
- more than two channels is rejected before transport publication with
  `Audio files must contain one or two channels; this file contains N.`

There is no implicit surround downmix. A zero-channel, empty, corrupt,
unsupported, non-finite-metadata, or impractically long file fails without
replacing the last prepared source. The UI displays the filename, format,
channel count, source sample rate, duration, and preparation error; it never
needs to expose a full path.

Source samples are converted to finite `float` values. Non-finite decoded
samples become zero and increment a source-data diagnostic before they can
enter feedback. Audible graph output still passes through the existing
non-finite and runaway guards.

## Ownership and thread boundary

The message/control side owns file selection, path validation, format-reader
construction, metadata, waveform analysis, and transport commands. A dedicated
read-ahead worker owns decoding, filesystem reads, source-rate conversion, and
filling a fixed-capacity stereo ring prepared for the current device rate.

The audio callback owns no file, reader, stream, path, thumbnail, worker, or
resampler object. It may only:

1. consume a bounded lock-free command snapshot at block entry;
2. read at most the requested block from already-published stereo ring slots;
3. apply a preallocated source/loop transition ramp; and
4. write silence for any unavailable frame while advancing bounded counters.

The callback never allocates, frees, locks, waits, decodes, performs filesystem
I/O, formats a string, logs, analyzes a waveform, or destroys the last owner of
worker-managed storage. Ready source generations publish atomically. Retired
storage and reader lifetime are reclaimed on the control/worker side only
after the callback can no longer observe them.

Read-ahead capacity is a documented fixed duration with a hard byte ceiling.
Loading a file does not allocate in proportion to its complete duration.
Waveform display uses a bounded multiresolution peak summary built off-thread,
not the audio ring and not full-file samples retained for the UI.

## Transport state machine

The public state is `empty`, `preparing`, `ready`, `playing`, `paused`,
`tail`, or `error`. Commands carry a monotonically increasing generation;
newest valid command wins at a block boundary.

- **Load** prepares a new source without disturbing the active one. Successful
  publication enters `ready` at source frame zero. Failure leaves the previous
  prepared source and its state intact while exposing the new error.
- **Play** starts or resumes at the current source-frame cursor. At end of file
  with looping disabled, Play restarts from frame zero.
- **Pause** retains the cursor and sends zeros into the graph, allowing the
  existing reverb tail to decay. Resume does not reset the graph.
- **Stop** sends zeros, returns the cursor to frame zero, and requests the same
  block-boundary graph-state reset used by explicit safety recovery. It ends in
  `ready` and produces deterministic silence after the reset.
- **Seek** clamps to `[0, frameCount]`, invalidates old read-ahead generations,
  and requests a block-boundary graph reset. Publication waits until frames at
  the target are prepared, so stale pre-seek audio is never emitted.
- **End of file** changes `playing` to `tail`, sends zeros, and preserves graph
  state so the reverb can decay. Stop, Seek, Play-from-start, file replacement,
  or the bounded tail-completion policy leaves `tail`.
- **Loop** uses the half-open source-frame interval `[start, end)`. The minimum
  loop is one output block or 20 ms at the source rate, whichever is larger.
  Invalid ranges are rejected without changing the active range. Wrapping does
  not reset the graph and uses a prepared 5 ms equal-power source crossfade so
  a discontinuity is not injected into feedback.

Positions, loop points, and duration are represented canonically as integer
source frames. Seconds are a UI projection. Source-to-device conversion uses a
deterministic fractional phase and band-limited JUCE resampling path; changing
callback block partition must not change the rendered sample sequence.

## Source switches, topology changes, and device changes

Live Input and Audio File switches occur at a block boundary with a prepared
10 ms equal-power crossfade. Switching to Test Impulse first reaches silence,
then emits only the requested impulse. Switching away from Audio File pauses
its cursor; returning resumes unless Stop or Seek changed it. Live input is
never summed into Audio File or Test Impulse mode.

A successful graph topology publication does not stop, seek, or restart the
file. The existing graph-runtime crossfade receives the continuous selected
source. A failed graph edit leaves both the last valid runtime and transport
unchanged.

When the standalone device sample rate or maximum block size changes, playback
pauses, the source ring and resampler are rebuilt off-thread, the nearest
source-frame cursor is retained, and the graph is reprepared/reset under the
existing device-change policy. Playback never resumes automatically; the user
presses Play after the new generation is ready.

## Underrun and failure behavior

If a requested source frame is unavailable, the callback emits zero for that
frame, does not reuse a stale ring slot, and increments lock-free underrun frame
and event counters. The graph continues processing zeros so an existing tail
can decay. The source cursor does not skip unavailable file frames; playback
therefore stalls rather than losing program material. Recovery begins only
from a published contiguous generation.

A decode/read failure enters `error`, stops further source advancement, and
sends zeros. Missing or moved files discovered before reopen produce an
actionable load error; they cannot invalidate, replace, or mutate the graph.
Emergency Mute remains available during preparation, playback, underrun,
source switching, and error recovery.

## Persistence and privacy

Audio File mode, paths, cursor, loop range, and transport state are excluded
from patch JSON, graph undo/redo, clipboard data, VST3 host state, and factory
patches. Audio bytes and waveform summaries are never embedded in those
documents. Saving or loading a graph does not start, stop, or replace a file.

The first implementation does not automatically reopen the last path on
standalone launch. This avoids unexpected disk access, stale-path failures,
and disclosure of a local path through shared state. A future opt-in recent
file list would be standalone application preference data with filename-only UI
and explicit clearing, not patch or plugin state.

## Offline export boundary

M14.4 may reuse the selected file and graph document, but export is a separate
worker-owned offline render. It does not feed the live read-ahead ring, drive
the physical device, or impersonate the real-time callback. Wet-only and
audition-mix export must use the same prepared graph semantics and declared
source-rate conversion as live audition, with deterministic comparison tests.

## M14.2 executable obligations

The prepared source implementation must prove:

- exact mono duplication and stereo preservation;
- explicit rejection of more than two channels and unsupported/corrupt input;
- deterministic output across callback block partitions and device rates;
- correct start, pause, stop, seek, end, loop, and device-change transitions;
- silence plus monotonic diagnostics on forced underrun, with no stale replay;
- fixed prepared memory and no callback allocation, lock, I/O, decode, logging,
  or unbounded work; and
- graph/persistence compatibility: the same patch and VST3 state bytes remain
  valid without any transport fields.

## M14.2 implementation

`reverb_audio` now owns `PreparedAudioFileSource`, a standalone-only stereo
transport. A worker thread owns the JUCE format reader and resampler and fills
a fixed two-second ring. At the maximum supported 192 kHz rate the ring and
scratch storage remain below the declared 8 MiB ceiling. The callback-facing
`process` operation is `noexcept` and is limited to zero-fill, bounded ring
copies, atomic cursor publication, and atomic underrun diagnostics.

The implementation accepts WAV, AIFF, and FLAC through JUCE readers, duplicates
mono exactly, preserves stereo, rejects channel counts above two, and retains
the previous valid generation after a failed load. Loop points and cursors use
source frames. A five-millisecond equal-power splice joins loop boundaries;
resampling happens before publication into the output-rate ring.

The processor routes the selected file through the same graph, telemetry,
master-gain, numerical-guard, and emergency-mute path as live input. Stop and
seek request a graph signal-state reset without clearing the safety latch.
File identity, path, source selection, cursor, and transport state remain absent
from patch JSON and VST3 state. M14.3 owns source-switch crossfades and all
visible transport/waveform controls.
