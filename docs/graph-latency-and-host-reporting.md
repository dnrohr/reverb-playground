# Graph latency and host reporting

Status: implemented in M16.2

## User contract

The prepared graph compiler calculates latency from the same causal schedule used by audio processing. The Diagnostics drawer shows the active graph total in samples and milliseconds, each stereo output path, and every parallel audio join whose inputs arrive at different prepared times. The VST3 reports the active processed graph total to its host.

The schematic remains literal: the runtime does not insert hidden branch delays. An uncompensated join is therefore both audible and listed by node ID. The audition `DRY BYPASS` is also literal and has zero internal latency; while it is selected, the plugin reports zero samples to the host. Returning to `PROCESSED` restores the active graph report.

## Compile rules

- A Delay contributes its prepared nominal delay, rounded to the exact integer sample count used by the runtime.
- Pitch Shift contributes its fixed prepared dual-grain latency.
- Serial contributions add.
- A parallel join continues at the maximum input latency and records the minimum, maximum, and uncompensated difference.
- Allpass has a direct term and therefore contributes zero fixed onset latency. Its delay still consumes prepared memory and shapes phase/time response.
- Gain, Sum, Low-pass, Envelope Follower, Hold Gate, controls, and I/O contribute zero fixed samples.
- Feedback uses the compiler's Delay-read/evaluate/Delay-write DAG. The incoming write dependency of a feedback Delay is cut, so a loop contributes its Delay state once and can never increase the result recursively.

Delay-time modulation changes an artistic delay continuously. The compiled figure is the prepared nominal state, not sample-by-sample telemetry; the UI labels it as compiled rather than measured. Pitch Shift latency is fixed across its supported controls.

## Publication and real-time safety

Compilation occurs off the audio thread. The immutable latency plan is registered with its requested graph revision. When the audio callback activates that prepared revision, it performs only lock-free scalar publication of the total sample count; it never calls JUCE's host latency API.

A 30 Hz message-thread timer observes the active count and calls `setLatencySamples` only when the reported value must change. This also means a host may defer or ignore a dynamic latency notification without changing rendered audio. During the existing 10 ms topology crossfade, the newly active revision is the report source; no concealed alignment delay is added between old and new runtimes.

## Verification

Native tests cover serial Delay plus Pitch Shift addition, parallel maximum/difference reporting, bounded feedback handling, active topology changes, state restoration, dry-bypass reporting, and the deferred-host-update case. Web tests validate the diagnostics payload including path and join identities.

Current standalone evidence is stored in `artifacts/ui/m16-2-compiled-latency/`: the first capture shows the compiled total and host value; the second shows per-output paths and uncompensated join identities after scrolling the same diagnostics drawer.
