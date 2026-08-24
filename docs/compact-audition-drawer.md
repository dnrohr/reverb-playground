# Compact audition drawer

Status: implemented as a post-M14 standalone UI refinement.

## Behavior

The standalone keeps the everyday audition path in one compact strip:

- source mode;
- Load File;
- Play/Pause;
- file name, transport state, and underrun count;
- Export WAV or Cancel Export; and
- a `+`/`−` details control.

The strip starts collapsed, reducing the standalone header by 96 logical pixels
compared with the open drawer. Loading a file opens the drawer automatically so
the waveform and loop tools are immediately discoverable. Closing it never
changes source mode, playback, loop, processed/dry state, or an active export.

The open drawer contains waveform seeking, Stop, loop enable/range,
Processed/Dry Bypass, export mode, and export progress. The source selector
replaces three permanent source buttons without changing the mutually exclusive
Live Input, Audio File, and Test Impulse behavior.

The loaded waveform keeps the complete file visible in subdued slate. The
current range is redrawn in bright cyan, providing a stronger luminance and hue
boundary than the previous single-color waveform. Enabling Loop changes that
same selected range to the deck's existing amber, while the unselected waveform
remains slate. The spatial range and Loop toggle continue to provide non-color
cues.

## Responsive contract

The native layout is calculated from the current editor width rather than a
fixed 210-pixel header. At widths below 900 logical pixels, device/safety
actions reflow into a dedicated row and every compact/drawer control receives a
bounded rectangle. At 900 pixels and above, the same controls use the wider
two-row header. The schematic WebView always begins immediately below the
calculated chrome.

Automated tests cover 640, 720, 899, 900, 1200, 1536, and 1920 logical pixels,
with the drawer open and closed. Expanded cases cross empty, loaded, playing,
looping, and exporting states. They assert that every control is non-empty,
inside the drawer, and non-overlapping; opening the drawer changes only vertical
chrome and preserves compact-strip geometry.

## Evidence

- Default 1200×720 editor, closed at 125% Windows scaling:
  [`artifacts/ui/compact-audition-drawer/01-collapsed-default.png`](../artifacts/ui/compact-audition-drawer/01-collapsed-default.png)
- Minimum 640×400 editor, open at 125% Windows scaling:
  [`artifacts/ui/compact-audition-drawer/02-expanded-minimum.png`](../artifacts/ui/compact-audition-drawer/02-expanded-minimum.png)
- Open/resize/close workflow:
  [`artifacts/ui/compact-audition-drawer/drawer-resize-workflow.mp4`](../artifacts/ui/compact-audition-drawer/drawer-resize-workflow.mp4)
- Current active-loop waveform treatment:
  [`artifacts/ui/startup-waveform-polish/03-active-loop-amber.png`](../artifacts/ui/startup-waveform-polish/03-active-loop-amber.png)

The screenshots were captured from the rebuilt Release standalone. Both drawer
states fill the native window without audition controls extending past its
right edge. At the minimum size, the existing schematic itself remains a
pan/zoom workspace; the drawer does not introduce an additional horizontal
scroll surface.
