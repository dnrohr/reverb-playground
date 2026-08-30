# Focused workspace shell

M25.1 begins the canvas-first interface by consolidating application commands
without removing graph, audition, safety, or diagnostic capability. The app icon
is deliberately deferred.

## Command organization

The compact application bar keeps the factory patch, patch identity,
saved/unsaved state, A/B slot, Energy state, Save action, and graph/runtime
status visible. Less frequent actions live in four conventional menus:

| Menu | Commands |
|---|---|
| File | Save Patch, Open Patch, Audio Device, Reset Patch |
| Edit | Undo, Redo, Copy, Paste, Delete Selection |
| View | processing quality, Energy, Diagnostics |
| Help | contextual learning, Keith Barr architecture notes, build version |

Save, Open, Undo, Redo, Copy, Paste, Delete Selection, and Reset Patch retain
their documented keyboard shortcuts. Escape closes an open application menu.
The patch-canvas bar now contains only canvas navigation and contextual Tune or
Matrix actions instead of duplicating file and editing commands.

The standalone exposes Audio Device in the File menu. The VST3 does not expose
standalone device setup because its host owns the audio device. Emergency Mute
remains continuously reachable in the native strip; Reset Safety appears only
after safety has latched and recovery is possible.

## Space returned to the schematic

At the 1200 x 720 reference size, the released wide native header used 123
logical pixels while the focused shell uses 91, returning 32 pixels. The web
application command row changes from 80 to 48 logical pixels, returning another
32. The 50-pixel measurement row is unchanged. Together, M25.1 returns 64
logical vertical pixels to the schematic at the reference size.

Narrow layouts retain the taller wrapped native controls so Wet Gain, Dry Gain,
Trigger Impulse, and Emergency Mute remain reachable rather than being clipped.
Later M25 tasks will make the palette, context area, and audition drawer
independently collapsible.

## Evidence

- [`compact-command-shell.png`](../artifacts/ui/m25-1-focused-workspace-shell/compact-command-shell.png)
  shows the maximized standalone with the compact native strip, application
  menus, patch identity, safety action, measurement row, and expanded canvas.
- [`file-menu-command-coverage.png`](../artifacts/ui/m25-1-focused-workspace-shell/file-menu-command-coverage.png)
  shows the standalone-only Audio Device action alongside Save, Open, and Reset
  in the compact File menu.
- Automated layout coverage proves the 32-pixel native gain at 1200 pixels wide,
  preserves the audition drawer's 96-pixel expansion, and checks the web shell's
  32-pixel command-row gain and command mapping.
