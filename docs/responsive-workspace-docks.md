# Responsive workspace docks

M25.2 makes the module palette, context dock, and standalone audio drawer
independently collapsible while keeping the schematic as the stable center of
the application.

## Workspace arrangements

The workspace selector appears above the canvas and in the View menu.

| Arrangement | Module palette | Context dock | Audio drawer | Purpose |
|---|---:|---:|---:|---|
| Balanced | Open | Open | Closed | Construct while retaining immediate module and selection context |
| Create Focus | Closed | Closed | Closed | Give the complete workspace width and maximum height to the schematic |
| Learn & Inspect | Closed | Open, widened | Open in standalone | Prioritize explanation, parameters, analysis, and source details |

Each dock can be changed independently after selecting an arrangement. Closed
side docks leave visible Modules or Context reveal buttons on the canvas. The
native audio drawer retains its `+`/`−` control and can also be changed from the
View menu. Independently changing a side dock labels the resulting arrangement
Custom so it is not mistaken for an untouched preset. The VST3 omits the audio
drawer control because file audition belongs to the
standalone.

Workspace presentation is saved under the local preference key
`reverb-playground-workspace-v1`. It is deliberately absent from patch JSON and
host state. Changing or resizing the workspace therefore cannot change the
graph, selection, transport position, loop, Wet/Dry gains, captured analysis,
export progress, or safety latch.

## Responsive behavior

At 900 logical pixels and wider, open docks occupy explicit grid columns and
the canvas consumes all remaining width. Learn & Inspect widens the context
dock from the balanced 20% range to a bounded 28% range.

Below 900 logical pixels, a side dock becomes an overlay so opening it never
shrinks the canvas into an unusable strip. If both docks are requested, the
most recently requested one is shown; closing it exposes the other dock's
reveal control. The native audition controls wrap into their established narrow
layout. CSS and native layout use logical pixels, so the same contract applies
at 100%, 125%, and 150% Windows scaling.

Automated coverage exercises 640, 720, 899, 900, 1200, 1536, and 1920 logical
pixels; every open/closed side-dock combination; both narrow overlay priorities;
100%, 125%, and 150% scale factors; and empty, loaded, looping, exporting,
selected, and safety-latched representative states.

## Evidence

- [`balanced-workspace.png`](../artifacts/ui/m25-2-responsive-workspace-docks/balanced-workspace.png)
  shows both side docks with the central schematic.
- [`create-focus-workspace.png`](../artifacts/ui/m25-2-responsive-workspace-docks/create-focus-workspace.png)
  proves the graph fills the width released by both closed docks.
- [`learn-inspect-workspace.png`](../artifacts/ui/m25-2-responsive-workspace-docks/learn-inspect-workspace.png)
  shows the widened context dock and expanded standalone audio drawer.
- [`custom-dock-arrangement.png`](../artifacts/ui/m25-2-responsive-workspace-docks/custom-dock-arrangement.png)
  shows the honest Custom label after independently closing the module palette.
- [`narrow-modules-overlay-125-percent.png`](../artifacts/ui/m25-2-responsive-workspace-docks/narrow-modules-overlay-125-percent.png)
  shows the 640-logical-pixel narrow overlay at 125% Windows scaling.
- [`workspace-arrangements.mp4`](../artifacts/ui/m25-2-responsive-workspace-docks/workspace-arrangements.mp4)
  demonstrates Balanced → Create Focus → Learn & Inspect → Balanced transitions
  while the graph remains intact.
