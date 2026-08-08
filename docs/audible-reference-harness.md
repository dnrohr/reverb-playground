# Audible reference harness

The standalone application and VST3 now run the fixed Barr-inspired reference wet path in real time. The interface exposes only the controls needed to audition it safely before graph editing exists.

## Standalone use

1. Build and open `build/windows-msvc/src/app/ReverbPlayground_artefacts/Debug/Standalone/Reverb Playground.exe`.
2. JUCE restores the last audio configuration or opens the operating-system default output. The status line shows the selected output and active sample rate.
3. Choose **Audio Device...** to select the driver, input/output device, sample rate, and buffer size. The wrapper's **Options** control opens the same JUCE settings.
4. Press **Trigger Impulse** to inject a unit impulse directly into the wet reference, independent of hardware input. The button confirms **Impulse sent** for one second.
5. Adjust **Master Audition Gain** from silence to unity. The safe default is `0.5` linear.
6. Press **Emergency Mute** for immediate latched manual silence; press it again to resume. **Reset Safety** becomes available only after the numerical guard detects a non-finite or runaway output.

JUCE mutes standalone audio input by default to avoid acoustic feedback. The yellow banner and **Settings...** button control that protection. Leave it muted for impulse audition; explicitly enable it in settings only when the routing cannot feed speakers back into the input.

In a plugin, the host owns audio-device selection. The Audio Device button explains that boundary, while the status line reports the host sample rate.

## Real-time behavior

`prepareToPlay` prepares and clears the complete reference whenever the host/device sample rate changes. `processBlock` performs bounded span processing with no allocation or locking. Live stereo input is summed to mono inside the reference, then the two distinct wet branches replace the stereo output.

Impulse, gain, manual mute, and safety-reset requests cross to the audio thread through atomics. Non-finite or absolute output above the numerical safety threshold zeros both channels and latches safety silence. Reset is consumed by the audio thread, where it clears DSP and guard state without a cross-thread data race.

If no audio device has prepared the engine, processing returns deterministic stereo silence. The editor remains usable and reports that it is waiting for audio. Native tests cover unprepared startup, impulse output, gain, emergency mute, numerical safety latch/reset, and reprepare at a changed sample rate.

## UI evidence

- [Harness screenshot](../artifacts/ui/m1-5-audible-reference-harness/audible-reference-harness.png)
- [Impulse and emergency-mute video](../artifacts/ui/m1-5-audible-reference-harness/audible-reference-controls.mp4)

The video triggers an impulse, shows its confirmation state, enables emergency mute and visible silent-output status, clears mute, and triggers a second impulse. The captured machine used the default 48 kHz Windows output; the device-selection dialog was also opened and closed during manual QA.
