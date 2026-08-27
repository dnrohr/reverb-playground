# Audio workflow guide

Use the standalone when you want to learn, inspect, compare, or export a reverb
with a saved audio file. Use Live Input when playing or speaking into the current
audio device. Use Test Impulse for a repeatable view of onset, diffusion, decay,
feedback circulation, and stereo response. Use the VST3 when audio and automation
belong to a DAW session.

## Choose a workflow

| Goal | Best route | Why |
|---|---|---|
| Compare factory or edited reverbs on a recording | Standalone Audio File | Loop, waveform seek, A/B, energy, response, and diagnostics stay together |
| Hear an instrument or microphone immediately | Standalone Live Input | The selected device feeds the graph directly |
| Measure or understand a topology | Test Impulse | The stimulus is deterministic and excludes unexpected live/file input |
| Automate the effect in a song | VST3 in a DAW | The host owns audio, transport, automation, latency, and project recall |
| Create a shareable processed WAV | Standalone Export WAV | Deterministic offline processing does not depend on the device callback |

## Supported files

Input accepts mono or stereo WAV, AIFF, and FLAC. Mono is duplicated; stereo is
preserved. Files above two channels, corrupt data, invalid metadata, and files
longer than 24 hours are rejected. Playback converts to the current device rate;
export always writes stereo 48 kHz, 24-bit PCM WAV.

Paths, source mode, cursor, loop, waveform, and export jobs are session-only.
They are never embedded in a patch or VST3 state.

## Recovery behavior

| Event | Result and recovery |
|---|---|
| Standalone restart | Starts in Live Input. Load the file again; no local path is reopened implicitly. |
| Audio-device/rate change | Playback pauses, prepared storage is rebuilt, and the nearest source cursor is retained. Press Play. |
| Source moved or deleted before load/export | The operation is rejected; the graph and previous valid source stay intact. Locate or restore the file and retry. |
| Graph save/reload or successful topology edit | File transport is unchanged. The newest valid graph becomes the audition/export graph. |
| Failed graph edit | Last valid graph and transport remain active. Correct the graph and republish. |
| Underrun | Missing frames become silence, the cursor stalls, and a counter increments. Reduce device load or buffer pressure. |
| Safety latch | Output is silent until the graph/input is made safe and Reset Safety is pressed. |
| Export cancellation/failure | Temporary data is deleted; an existing destination is preserved. Retry after correcting the reported cause. |
| Missing VST3 source controls | Expected: the VST3 always uses host-provided stereo input. Use the standalone for files. |

## Safe audition sequence

Start with Wet Gain near 0.5, Dry Gain at 0, and confirm Emergency Mute is available.
Load the source, choose a short loop, then raise Dry Gain to compare the immediate
source against the graph output.
Watch the underrun and graph-safety diagnostics while editing. For feedback-heavy
shimmer or reverse-cosmic designs, raise controls gradually. If safety latches,
mute, undo or reduce the risky return, then reset safety.

Live audition and export use the same explicit formula: `Wet Gain × graph output
+ Dry Gain × selected source`. The gains are independent, linear, and are not
silently normalized, so their sum can exceed unity. The export captures both
gains and the current graph at start; later live edits do not mutate an in-flight
job.

## Current limitations

- Transport does not reverse source sample order or provide varispeed.
- The waveform is an overview, not a destructive audio editor.
- Looping is for audition only; export renders the complete source once.
- Export rate and bit depth are currently fixed at 48 kHz / 24-bit stereo.
- No recent-file list or automatic path reopening is stored.
- Standalone file transport is intentionally unavailable in the VST3.

See [Standalone audio-file audition](standalone-audio-file-audition.md) and
[Processed-file export](processed-file-export.md) for the detailed contracts.
