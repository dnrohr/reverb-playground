# Standalone startup and recovery

## Why the launch used to look stalled

The previous standalone used JUCE's stock application wrapper. That wrapper
constructed `StandalonePluginHolder` before it constructed or displayed a
window. The holder synchronously enumerated and opened Windows audio devices,
restored plugin state, and started playback. On the reference Windows 11 system,
the process therefore remained windowless for 8.16 to 9.97 seconds across saved
and fresh-settings launches. CPU use during most of that interval was negligible;
processor construction is empty, and the schematic appeared about 0.38 seconds
after the old native window was finally created.

Removing the saved settings did not remove the delay: a controlled fresh-state
launch took 9.97 seconds. A missing previously selected input may make recovery
less predictable, but it is not the primary explanation for the windowless
startup.

## Startup ownership

The standalone now uses a project-owned JUCE application with an explicit
handoff:

1. The message thread creates and shows a small DPI-aware native status window.
2. A startup worker constructs the standard `StandalonePluginHolder`. JUCE still
   owns device discovery, saved-device fallback, plugin-state restore, and the
   audio callback/player connection.
3. The worker transfers the completed holder to the message thread.
4. The message thread constructs the existing `StandaloneFilterWindow` and
   WebView editor, shows it, and removes the status window.

The graph processor and audio callback contracts are unchanged. No device scan,
file access, logging, allocation, or lock was added to the real-time callback.
The VST3 never compiles or displays the startup window; its host continues to own
audio devices and transport.

Startup phases are monotonic: Showing Shell, Connecting Audio, Opening Editor,
then Ready. An unfinished phase may instead enter Failed. The shell exposes the
current state using text and a progress line, so status does not depend on color.

The progress line is deliberately a presentation timer rather than invented
device telemetry. It advances linearly from empty to full over eight seconds,
shows `Loading...` before the eight-second boundary, and shows `Welcome!` at and
after that boundary. The editor still replaces the shell as soon as the actual
audio holder is ready; the bar does not delay audio or claim to measure driver
progress.

## Shutdown and failure behavior

Closing the shell requests ordinary application shutdown. Shutdown marks the
application as unavailable, joins the startup worker, and only then releases
settings and any completed holder. The queued UI handoff checks that the same
application instance is still alive before touching windows. Closing the ready
editor saves plugin state through the same holder and properties file used by
the previous wrapper.

If holder construction fails, the shell remains visible with a failure message.
Windows audio readiness is still bounded by the installed device drivers and the
operating system; the improvement is that a slow scan no longer looks like an
ignored launch or freezes the message thread.

## Measurement contract

Startup timing begins immediately before process creation:

- **Shell visible** is the first non-zero native main-window handle.
- **Editor ready** is the replacement main window at the normal editor bounds,
  with the schematic rendered and the audio status visible.
- Measurements use a Release build and retain the user's saved settings unless
  a test explicitly says otherwise.

The M15 qualification records five warm launches on the reference Windows 11
machine at 125% display scaling. The target is shell visibility in at most one
second. Editor time is reported separately because device/driver behavior is an
environmental dependency rather than something the application should conceal.

| Release run | Shell visible | Editor ready |
|---:|---:|---:|
| 1 | 0.370 s | 8.690 s |
| 2 | 0.400 s | 8.170 s |
| 3 | 0.400 s | 8.200 s |
| 4 | 0.410 s | 8.270 s |
| 5 | 0.390 s | 8.290 s |
| Mean | 0.393 s | 8.321 s |

The slowest shell result was 0.411 seconds, below the one-second contract. The
editor still reflects the roughly eight-second Windows audio dependency, but the
application is visible and its message thread remains responsive throughout it.

Reviewed evidence is in
[`artifacts/ui/m15-fast-startup/`](../artifacts/ui/m15-fast-startup/): the two
PNG files show the connecting and ready states at 125% Windows scaling, and
[`startup-to-editor.mp4`](../artifacts/ui/m15-fast-startup/startup-to-editor.mp4)
shows the automatic handoff without a blocked or blank window.

The timed presentation refinement is shown at
[`artifacts/ui/startup-waveform-polish/`](../artifacts/ui/startup-waveform-polish/):
the four-second and post-eight-second screenshots prove the two labels and bar
positions, while `loading-to-welcome.mp4` shows the continuous transition.
