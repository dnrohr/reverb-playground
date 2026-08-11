# Windows alpha package and host validation

This record defines the M7.2 Windows alpha package, the install boundary, and the host checks required before publishing it. The deliverable is an x64 ZIP containing the standalone application, the complete VST3 bundle, license/provenance notices, build identity, and a current-user VST3 installer.

## Build and package contract

- Primary platform: Windows 10/11 x64.
- Product version: `0.1.0`.
- Build identity: CMake embeds `REVERB_PRODUCT_VERSION` and the 12-character source commit in both standalone and VST3 binaries. The editor displays both below **Patch architecture**.
- Archive identity: `build-info.json` repeats the version, full source commit, target platform, configuration, and included formats.
- Reproducibility: archive paths are sorted; timestamps use the source commit time; permissions are normalized; an adjacent SHA-256 file is emitted.
- Source hygiene: a normal package refuses a dirty worktree. `-AllowDirty` exists only for local pre-commit validation.

Run from a Visual Studio 2022 developer shell:

```powershell
./scripts/package_windows.ps1
```

The package is written below `out/packages/`. Installation and removal are documented in [Windows package installation](windows-package-installation.md). The package intentionally excludes local research checkouts, ROM material, build trees, tests, and validation hosts.

## Validation environment

Validation was performed on Windows 11 x64 at 125% display scaling with a 1536-by-960 primary display. The Release VST3 bundle installed for testing was byte-for-byte identical to the freshly built bundle; validation did not reuse an older nested or cached bundle.

Named hosts and tools:

| Host/tool | Version | Role | Result |
|---|---:|---|---|
| Tracktion pluginval | 1.0.4 | Maximum-strictness automated plugin host | Pass |
| Steinberg VST3 validator | 3.8.1 build 84 | Official format conformance host | 47 passed, 0 failed |
| JUCE AudioPluginHost | 8.0.13 | Interactive graph host and editor lifecycle | Pass |

The downloaded pluginval ZIP had SHA-256 `C08E61CE3B96DB41636F8EC7E76F4C7E2C13EBDAC7FA1B5A1F52B4F32EC715AB`. pluginval and AudioPluginHost are validation tools only and are not distributed in the package.

## Automated host results

pluginval ran at strictness level 10. It passed scanning, cold and warm opening, editor creation while idle and processing, state and state-restoration checks, automation, editor automation, parameter/thread-safety checks, bus enable/disable/layout checks, parameter fuzzing, and non-releasing audio processing. Processing covered 44.1, 48, and 96 kHz with block sizes 64, 128, 256, 512, and 1024.

Steinberg's validator passed 47 tests. Its process-format test exercised 22.05, 32, 44.1, 48, 88.2, 96, 192, and 384 kHz plus the validator's unusual 1234.5678, 12345.678, 123456.78, and 1234567.8 Hz rates. It also passed state transitions, bus consistency/activation, bypass persistence, threaded processing, silence handling, variable blocks, parameter accuracy, and stereo arrangement checks.

The official validator initially found two release-blocking defects: an unnamed default VST3 program and a low-rate Nyquist exception in the Barr input filter. Both were fixed and covered by native regression tests before the passing run. Lack of 64-bit sample processing is reported as unsupported and accepted for this alpha; all corresponding validator cases pass through the format's supported-capability path.

## Interactive AudioPluginHost results

The Release VST3:

- scanned and appeared under the **Reverb Playground** manufacturer;
- instantiated as a stereo effect;
- accepted two mono input cables and two mono output cables in the host graph;
- remained active at 48 kHz while its editor was open;
- changed from Barr Reference to the complete 13-block Level-Gated Room by keyboard-driven factory selection;
- preserved all 13 blocks, 16 cables, parameters, layout, and viewport after editor close/reopen;
- passed AudioPluginHost's **Test state save/load** command and reopened as the same restored custom graph;
- accepted keyboard focus/navigation inside WebView2;
- resized from its preferred 1280-by-800 logical size to a maximized 1536-by-960 host window; and
- filled the available window at 125% scaling without the former 1920-by-1200 native maximum or a second scale conversion.

The host-facing non-parameter-state notification is emitted whenever a valid graph state changes so project hosts know that the plugin requires re-saving.

## Evidence and release decision

Reviewed visual evidence is stored in `artifacts/ui/m7-2-windows-package/`. It includes the maximized VST3 editor, routed AudioPluginHost graph, packaged standalone, and a short state/reopen interaction recording. Screenshots display the package version and source commit where the editor is visible.

All observed validation failures were corrected. There is no outstanding M7.2 validation exception that blocks the Windows alpha package.
