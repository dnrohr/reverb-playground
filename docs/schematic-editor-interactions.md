# Schematic editor interactions

M2.1 establishes the presentation and navigation contract for the Barr reference patch. The graph is an editable UI copy; runtime DSP identity and parameter binding begin in M2.2.

## Layout

The module library is on the left, the patch canvas occupies the centre, and the contextual inspector is on the right. The native audition strip remains above the web editor in the plugin so an impulse, device selection, gain, mute, and safety reset stay reachable.

The reference patch opens fitted to the available canvas. Blocks retain stable IDs and use a compact snake layout so the complete signal path is readable at the default 1280 by 800 editor size.

## Pointer and keyboard contract

- Select a block or cable with primary click. The inspector follows the selection.
- Add to or box-select a set while holding Shift.
- Pan by holding Space while dragging, or by dragging with the middle or secondary mouse button.
- Zoom with the mouse wheel or the canvas zoom controls. The fit button restores the complete reference view.
- Press Tab to move keyboard focus through graph elements and Enter to select the focused item.
- Press Delete or Backspace to remove selected blocks or cables from the UI copy.
- Press R, or choose **Reset view copy**, to restore the reference graph and fitted viewport.

Deletion cannot affect audio in M2.1 because the visible graph is deliberately not bound to the runtime yet.

## Signal semantics

Audio and control connections never rely on colour alone. Audio uses a solid cable and circular ports; control uses a dashed cable and diamond ports. The module library and canvas legend repeat those labels in text.

Every cable is mono. Stereo boundary blocks expose separate left and right ports; the reference patch explicitly sums its stereo input to the shared mono diffusion path and explicitly branches to its wet outputs.

## Display scaling

The supported Windows display scaling range for this prototype is 100%, 125%, and 150%. Typography and panes use CSS-relative sizing, blocks have minimum dimensions, and the canvas can always be fitted or zoomed. At narrow effective widths the inspector and library reduce in width before the canvas is sacrificed.
