# Windows package installation

Package: Reverb Playground 0.1.0 alpha for Windows 10/11 x64.

Open `build-info.json` to see the exact product version, source commit, build configuration, and formats. Verify the adjacent `.sha256` file before extracting when the package was downloaded separately.

## Standalone

1. Extract the complete ZIP to a writable folder.
2. Open `Standalone/Reverb Playground.exe`.
3. If the editor reports that WebView2 is unavailable, install Microsoft's current Evergreen WebView2 Runtime and reopen the application.
4. Choose **Audio Device…**, select a stereo output, and use **Trigger Impulse** at a conservative monitor level.

The standalone is portable; it does not write program files outside its normal JUCE settings and WebView2 profile locations. Keep `LICENSE`, `THIRD_PARTY_NOTICES.md`, and `ASSET_PROVENANCE.md` with redistributed copies.

## VST3

Run `install-vst3.ps1` from PowerShell to install for the current user, or copy the complete `VST3/Reverb Playground.vst3` directory manually to one of:

- `%LOCALAPPDATA%\Programs\Common\VST3` for the current user; or
- `C:\Program Files\Common Files\VST3` for all users (administrator permission required).

Rescan VST3 effects in the host and insert **Reverb Playground** on a stereo track. The plug-in requires the Microsoft Evergreen WebView2 Runtime; Windows 10/11 installations commonly already have it, but the package deliberately does not redistribute that separately licensed runtime.

## Removal

Delete the extracted standalone folder and remove `Reverb Playground.vst3` from the VST3 location used above. Existing DAW projects retain their saved state but will report a missing plug-in until it is reinstalled.

## First verification

The editor header shows `v0.1.0 / <commit>`. Compare that commit with `build-info.json`. Load **Causal Reverse Envelope**, move a block, close and reopen the editor, then save/reopen the host project. The same graph, parameters, cable layout, and viewport should return.
