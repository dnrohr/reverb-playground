# Compact audition drawer

Status: rebuilt and verified in M25.4.

## Behavior

The standalone keeps the everyday audition path in one thin strip at the
bottom of the window, leaving the schematic between the command bar and deck:

- source mode;
- Load File;
- Play/Pause and Stop;
- file name, transport state, and underrun count;
- independent Wet and Dry Gain;
- Quick Impulse, which is an audible audition trigger rather than a
  measurement; and
- a `+`/`−` details control.

The strip starts collapsed. Loading a file opens the drawer automatically so
the waveform and loop tools are immediately discoverable. Closing it never
changes source mode, playback, loop, Wet/Dry Gain, or an active export.

The open drawer grows upward from the strip and contains waveform seeking,
loop enable/range, Export WAV, export range and progress, plus the
isolated response-capture settings in the web surface immediately above it.
It explicitly says that Wet + Dry is unnormalized and may sum above unity.
The source selector
replaces three permanent source buttons without changing the mutually exclusive
Live Input, Audio File, and Test Impulse behavior.

The loaded waveform keeps the complete file visible in subdued slate. The
current range is redrawn in bright cyan, providing a stronger luminance and hue
boundary than the previous single-color waveform. Enabling Loop changes that
same selected range to the deck's existing amber, while the unselected waveform
remains slate. The spatial range and Loop toggle continue to provide non-color
cues.

## Responsive contract

The native layout is calculated from the current editor width and height.
The safety bar stays at the top; the compact deck is anchored to the bottom;
and the WebView receives exactly the non-overlapping rectangle between them.
Below 1200 logical pixels, the compact controls use two rows so gain sliders
and file identity remain reachable rather than clipping horizontally.

Automated tests cover 640, 720, 899, 900, 1200, 1536, and 1920 logical pixels,
with the drawer open and closed. Expanded cases cross empty, loaded, playing,
looping, and exporting states. They assert that every control is non-empty,
inside the drawer, and non-overlapping; opening the drawer changes only vertical
chrome, preserves compact-strip geometry, and leaves a non-overlapping WebView.

## Evidence

- Desktop editor with the bottom deck closed:
  [`compact-desktop.png`](../artifacts/ui/m25-4-bottom-audition-drawer/compact-desktop.png)
- Desktop editor with loaded waveform, capture, loop, and export details:
  [`expanded-desktop.png`](../artifacts/ui/m25-4-bottom-audition-drawer/expanded-desktop.png)
- Loop, playback, isolated capture, resize, and export workflow:
  [`audition-drawer-workflow.mp4`](../artifacts/ui/m25-4-bottom-audition-drawer/audition-drawer-workflow.mp4)
- Explicit Live Input / Audio File switching without changing loop bounds:
  [`source-switching.mp4`](../artifacts/ui/m25-4-bottom-audition-drawer/source-switching.mp4)
- Current active-loop waveform treatment:
  [`artifacts/ui/startup-waveform-polish/03-active-loop-amber.png`](../artifacts/ui/startup-waveform-polish/03-active-loop-amber.png)

The evidence was captured from the rebuilt Release standalone using a real
three-second WAV. Capture visibly routes to Analyze and labels the result as an
isolated measurement. Export completes without changing loop bounds, source,
Wet/Dry values, or drawer state.
