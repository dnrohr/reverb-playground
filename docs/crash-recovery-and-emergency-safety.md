# Crash recovery and emergency safety

M32 keeps failure evidence private and makes final-output muting independent of
the current editor context. The Windows standalone and VST3 install the same
process-level exception boundary. The standalone additionally owns abnormal-exit
detection and a last-known-valid recovery candidate.

## Local report contract

Reports live in the per-user application-data folder under
`Reverb Playground/Crash Reports`. Each incident has a matching
`reverb-playground-<incident-id>.dmp` and `.txt` summary. A `.partial` suffix
means the write did not complete. Incident names combine local timestamp and random
entropy, and only the newest eight incident pairs are retained.

The versioned summary includes the product version, exact build commit, Windows
version, standalone/VST3 mode, startup/runtime phase, active factory identifier,
graph hash and revision, sample rate and block size, recent publication/safety
status, and at most 16 breadcrumbs of at most 256 characters each. It excludes
source audio, audio samples, full patch JSON, parameter values, user document
paths, loaded-file paths, and host-project content. The minidump can contain
process memory and is therefore potentially sensitive even though the text
summary is deliberately sparse. Nothing is uploaded, attached, or shared
automatically.

Use **Help > Open crash reports folder** to inspect the files. When filing an
issue, attach only the files you have reviewed and intend to share. Keep the
matching incident ID in the issue so maintainers can pair the summary and dump.

## Recovery contract

The standalone writes only a graph that has already passed normal patch
validation to `last-known-valid.autosave.rvp.json`. An active-session marker is
removed only after clean shutdown. On the next launch, an abandoned marker
offers three explicit choices: **Restore muted**, **Start clean**, or
**Open reports**. The prompt shows the candidate filename and content hash so
the exact local candidate being offered remains identifiable.

Restore is opt-in. It validates and compiles the candidate through the normal
graph and delay-memory gates, latches Emergency Mute before publication, and
does not overwrite the original patch. Declining starts the default graph and
retains reports. A candidate that participates in two abnormal recovery attempts
is quarantined; the user can start clean and inspect the retained candidate and
reports. Host projects remain authoritative in VST3 and are never replaced by a
standalone autosave.

## Emergency Mute

Press **Ctrl+Shift+M** on Windows or **Command+Shift+M** on macOS. The shortcut
always sets the existing authoritative manual mute latch; it never toggles on
key repeat and works in text inputs, menus, dialogs, parent/nested schematics,
A/B, Help, and the audio drawer. The native button, Help command, runtime
diagnostics, and screen-reader announcement expose that same state.

Emergency Mute is applied at the final output after audition, comparison, and
graph rendering. Export cannot clear it. A numerical-safety latch is separate:
correct the graph, then use **Reset Safety** explicitly. Neither graph recovery,
navigation, previews, nor temporary audition clears a safety latch.
