# Standalone audio-file audition

Status: implemented in M14.3.

The current standalone presents the transport as a
[compact audition drawer](compact-audition-drawer.md). Source, load,
Play/Pause, file/transport summary, export, and drawer toggle remain visible;
waveform, loop, comparison, and detailed export controls can be hidden to give
the schematic editor more room.

The standalone can audition a local WAV, AIFF, or FLAC through the current
schematic without a DAW. The VST3 retains its host-input workflow and does not
show these controls.

## Loading and transport

Use **Load File…** or drop one supported file anywhere on the standalone
window. A successful load selects **Audio File** and starts playback. Mono is
duplicated to both graph inputs; stereo keeps its channel identity. Files with
more than two channels and corrupt or unsupported files are rejected without
discarding the previous valid source.

The waveform is an unclipped stereo overview generated outside the audio
callback. Click or drag across it, or use the position slider below it, to seek.
**Play/Pause** retains the cursor. **Stop** returns to the first frame and clears
the graph's signal history. The two-handle lower control sets a source-frame
loop; **Loop** enables or disables it. Loop joins use a five-millisecond
equal-power splice.

The status beside the waveform reports cursor/duration, transport state, and
underrun count. An underrun means source data was not ready in time; unavailable
frames become silence and stale samples are never replayed.

## Source and comparison controls

- **Live Input** sends only the standalone audio-device input into the graph.
- **Audio File** sends only the prepared file source. It is disabled until a
  valid file has loaded.
- **Test Impulse** first reaches silence, then sends the existing deterministic
  impulse through the graph.
- **Processed** auditions the current graph. Turning it off displays **Dry
  Bypass** and routes the selected source around the graph for level-matched
  comparison.

Source changes use a fixed 10 ms equal-power transition. Leaving Audio File
pauses its cursor after the transition; returning resumes automatically when
the source switch caused that pause. An explicit Pause, Stop, or Seek stays
paused until Play. Live input is never summed into file or impulse audition.

Master Audition Gain, Emergency Mute, the numerical-safety latch, energy view,
response capture, runtime diagnostics, topology crossfades, continuous edits,
and A/B comparison remain authoritative for file audition. Dry bypass still
passes through gain, mute, and numerical safety.

## Persistence and privacy

The selected path, source mode, cursor, loop, waveform, and transport state are
session-only standalone data. They are not stored in patches, clipboard/history,
factory files, or VST3 state. Restarting the standalone intentionally starts in
Live Input without reopening a local path.

## Processed export

Choose **Wet Only** or **Audition Mix**, then use **Export WAV…**. The standalone
writes deterministic 48 kHz stereo 24-bit PCM through a temporary file, shows
progress, and offers cancellation. Audition Mix is a 50/50 dry/processed blend;
Wet Only contains only graph output. See [Processed-file export](processed-file-export.md)
for tail, safety, overwrite, and failure behavior.

Reverse playback and arbitrary source reversal are not part of the transport;
reverse-style factory graphs remain causal buffered DSP.
