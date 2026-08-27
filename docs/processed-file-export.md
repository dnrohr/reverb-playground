# Processed-file export

Status: implemented in M14.4.

The standalone can render the loaded source through the current published graph
without a DAW or physical audio device. Export runs on its own worker and never
borrows the live read-ahead ring or audio callback.

## Mix, range, and format

Export uses the same independent linear law as live audition: `Wet Gain × graph
output + Dry Gain × selected source`. There is no hidden normalization. Both
gains and the graph are snapshotted when export starts; Emergency Mute remains a
live monitoring control and does not silently erase an offline render.

Every successful file is stereo 24-bit PCM WAV at 48 kHz. Mono sources are
duplicated before graph processing and stereo sources preserve channel identity.
JUCE's prepared resampler converts other supported source rates before the
authoritative graph runtime. **Entire File** processes the complete source once.
**Selected Loop** seeks to the visible loop start, processes that interval once
(it does not repeat it), and begins tail rendering at the loop end. Playback
crossfades are not baked into either export range.

## Tail and safety policy

After the final source frame, export feeds zero into the graph. It observes at
least two seconds of tail before silence can finish the job, preventing a normal
delayed onset from being mistaken for completion. After that observation window,
500 continuous milliseconds below -80 dBFS ends the render. Ten seconds is the
default hard tail maximum; the engine accepts bounded 0.5–60 second requests.

Every output sample must be finite and remain within the hard safety ceiling.
Unsafe output fails the job. Cancellation and all failures close and delete the
temporary WAV, leaving no partial destination.

## Destination publication

Rendering writes a sibling `partial` file. On success it is moved into the
chosen destination; when the platform supports replacement, a user-confirmed
overwrite atomically replaces the old file only after the new WAV is complete.
An existing file is otherwise rejected. The source can never be its own
destination.

The export button becomes **Cancel Export** while work is active. The adjacent
progress indicator reports render progress and the final filename, cancellation,
or failure. Closing the processor requests cancellation and joins the worker.

## Determinism

The exporter compiles a fresh runtime from the current graph snapshot, processes
fixed 256-frame blocks, and uses deterministic source-rate conversion and PCM24
quantization. Repeated requests with the same source, graph, gains, range, and
tail policy produce byte-identical WAV files. Tests cover independent gain mixes,
selected-loop frame bounds, tail bounds, resampling metadata, cancellation,
overwrite, invalid destinations, and numerical-safety failure.
