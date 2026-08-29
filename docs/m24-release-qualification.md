# M24 dense-reverb release qualification

M24.2 qualifies the current dense-reverb product path across native safety,
continuous editing, state, offline export, host sample-rate probes, VST3
conformance, and the reproducible Windows package. Subjective listening remains
tracked separately in M24.1.

## Qualified source and package

The qualification build is commit `b2deaaf11d01`. The deterministic Windows x64
archive is `ReverbPlayground-0.1.0-windows-x64.zip`:

- archive size: 6,782,343 bytes;
- archive SHA-256: `0bd062a1d345ad61a038f8f48af05d145987b124f9b151e87e4624c89bbbd670`;
- standalone SHA-256: `1136848e677f840d430f140d2c4f3cda8c8ab6d1c9f3bc730d209f37a72da386`;
- VST3 binary SHA-256: `63cbe93e13b02da019509efdc132145c04d3f583a511d4fbf34952e6ca5e6103`.

The package validator recomputed the adjacent archive checksum, matched the
embedded standalone and VST3 commit identities, and required the installer,
license, notices, provenance, and build information. The packaged standalone
also remained alive through a ten-second cold-start smoke test, past the
eight-second loading presentation.

## Safety and workflow matrix

The Release suite maps the milestone requirements to deterministic fixtures:

| Requirement | Evidence |
|---|---|
| Extreme controls and continuous editing | All factory noise/extreme fixtures, 32 Gravity endpoint combinations, five-macro sweeps, Four-Line macro sweep, rapid automation, and bounded topology-crossfade tests |
| Save and reopen | Complete factory graph host-state round trips, quality-policy persistence, independent Wet/Dry migration, and invalid-state retention tests |
| Offline export | Wet-only, dry-only, combined Wet/Dry, selected-loop range, resampling, bounded tail, cancellation, deterministic repeat, invalid destination, and unsafe-output tests |
| Sample-rate changes | 44.1/48/96 kHz dense fixtures, over-budget runtime retention, device reprepare/reset, and the complete Steinberg unusual-rate sequence |
| Real-time safety | Prepared fixed storage, callback work ceilings, zero telemetry observation when disabled, numerical latching/recovery, and last-valid runtime publication |
| Package and formats | Exact-commit standalone/VST3 archive validation, standalone cold-start smoke, pluginval strictness 10, and Steinberg VST3 validator extensive suite |

No test permits one composite score to hide non-finite output, runaway
publication, an invalid state replacement, or a failed perceptual dimension.
Callback-allocation safety is established structurally by prepared arenas and
bounded plans and exercised by the fixed-storage and host fuzz fixtures; this is
not a claim of whole-process zero allocation outside the audio callback.

## Host regression found and corrected

The first current-package host pass found a crash after Steinberg's 192 kHz
process-format probe. The audio-file transport had begun rejecting rates above
192 kHz by throwing from `prepareToPlay`; the host API cannot safely propagate
that exception. Preparation now accepts every finite positive host probe while
the read-ahead ring remains capped at two seconds of 192 kHz storage and below
the existing 8 MiB budget.

A native regression replays all twelve validator rates: 22.05, 32, 44.1, 48,
88.2, 96, 192, and 384 kHz plus 1234.5678, 12345.678, 123456.78, and
1234567.8 Hz. Every processed sample remains finite. A loaded-source fixture
also reprepares at the highest probe without exceeding its prepared-memory
budget.

## Named-host results

- Tracktion pluginval 1.0.4, strictness 10, seed `0x960096`: **SUCCESS**.
  Coverage includes scan, cold/warm open, editor lifecycle, audio at 44.1/48/96
  kHz and five block sizes, state restore, automation, thread safety, buses, and
  parameter fuzzing.
- Steinberg VST3 validator 3.8.1 build 84, extensive mode: **537 passed, 0
  failed**. Single-precision processing, unusual rates, state transitions,
  variable blocks, silence, threaded processing, parameter accuracy, bypass,
  and stereo bus behavior pass. Double-precision audio remains explicitly
  unsupported and passes through the format's capability path.

The exact logs are stored in
[`artifacts/validation/m24-2-release/`](../artifacts/validation/m24-2-release/).
No UI changed during M24.2, so new screenshot or video evidence is not required;
the validators create the current editor, and prior packaged UI evidence remains
linked from the Windows alpha validation record.

## Supported envelope

The product-qualified audio envelope remains Windows 10/11 x64, stereo
single-precision processing, 44.1/48/96 kHz, and host blocks from 32 through
1024 samples. The runtime safely rejects visible graph preparation above 192
kHz or beyond 64 MiB of delay memory while retaining the last valid runtime.
Higher and unusual host rates are compatibility probes, not supported creative
operating points; they must remain finite and must never crash the host.

