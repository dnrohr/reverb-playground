# Welcome to Reverb Playground

This package contains Reverb Playground 0.1.0 alpha for 64-bit Windows 10 and 11. It includes a standalone application for immediate use and a VST3 effect for compatible DAWs.

## The quickest start

1. Extract the complete ZIP to a folder you can write to, such as `Documents\Reverb Playground`. Do not run the application from inside the ZIP preview.
2. Open `Standalone\Reverb Playground.exe`.
3. Choose **Audio Device…** and select your stereo output.
4. Begin at a conservative speaker or headphone level.
5. Load an audio file or choose **Trigger Impulse**, then edit the visible reverb schematic.

Open **Help > User Guide** for the shipped searchable walkthrough. It covers
first sound, construction, nested schematics, comparison, diagnosis, tuning,
save/reopen, export, recovery, and Emergency Mute without requiring internet access.

The startup screen may remain visible for eight seconds while the editor opens. The standalone is portable: there is no installer and it does not place program files elsewhere. It does use the normal JUCE settings and WebView2 profile locations for your user account.

## If Windows shows a warning

The alpha is not yet code-signed, so Microsoft Defender SmartScreen may say that Windows protected your PC. Confirm that the ZIP came from the official Reverb Playground GitHub repository before choosing **More info** and **Run anyway**. Do not bypass a warning for a copy obtained elsewhere.

The download includes a `.sha256` file beside the ZIP. Advanced users can compare it in PowerShell:

```powershell
Get-FileHash .\ReverbPlayground-0.1.0-windows-x64.zip -Algorithm SHA256
```

The value should match the first value in `ReverbPlayground-0.1.0-windows-x64.zip.sha256`.

## Using the standalone

The standalone is the easiest option for exploring factory topologies, learning from the visualizations, auditioning audio files, and exporting processed audio. No DAW is required.

If the editor says WebView2 is unavailable, install Microsoft's current Evergreen WebView2 Runtime and reopen Reverb Playground. Windows 10 and 11 commonly already include it.

## Installing the VST3 plug-in

The VST3 is for using Reverb Playground as a stereo effect inside a DAW.

For a current-user installation, right-click `install-vst3.ps1`, choose **Run with PowerShell**, and follow the prompt. Alternatively, copy the complete `VST3\Reverb Playground.vst3` directory to:

`%LOCALAPPDATA%\Programs\Common\VST3`

For an all-users installation, copy it to `C:\Program Files\Common Files\VST3`; administrator permission is required. Rescan VST3 effects in your DAW, then insert **Reverb Playground** on a stereo track. The plug-in also requires the Evergreen WebView2 Runtime.

## What the other files are

- `build-info.json` records the exact version, source commit, build type, and included formats.
- `LICENSE`, `THIRD_PARTY_NOTICES.md`, and `ASSET_PROVENANCE.md` describe licensing and provenance.
- `install-vst3.ps1` performs a current-user VST3 installation.

Keep these files with any redistributed copy of the application.

## Removing Reverb Playground

Delete the extracted package folder to remove the standalone. Remove `Reverb Playground.vst3` from the VST3 folder where you installed it to remove the plug-in. Existing DAW projects retain their saved state but report a missing plug-in until it is reinstalled.

## Recovering or reporting a crash

After an abnormal standalone exit, the next launch offers **Restore muted**,
**Start clean**, and **Open reports**. Restore is optional, validates the last
known-good graph through the normal safety gates, and keeps Emergency Mute on.
Press **Ctrl+Shift+M** at any time to silence final output.

Crash reports remain local under `%APPDATA%\Reverb Playground\Crash Reports`.
The text summary excludes audio, full patch content, and user document paths.
The matching minidump may contain process memory, so review it before sharing.
Nothing is uploaded automatically. Use **Help > Open crash reports folder** to
inspect the files and include the displayed version/commit and incident ID when
filing an issue.

## Confirming the build

The editor header shows `v0.1.0 / <commit>`. The commit should match `build-info.json`. This makes it possible to identify the exact development or release build when reporting a problem.

For a guided first session, visit [Getting started: hear and inspect the Barr reference](https://github.com/dnrohr/reverb-playground/blob/main/docs/getting-started-barr-tutorial.md). Report issues through the official [GitHub Issues page](https://github.com/dnrohr/reverb-playground/issues) and include the displayed commit, Windows version, audio device or DAW, and steps to reproduce the problem.
