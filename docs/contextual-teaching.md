# Contextual guidance and rendered Help

M28 replaces the old global Learn preference with two independent,
presentation-only surfaces. **Learn & Inspect** remains a workspace arrangement
that widens the context dock. **Help** owns the rendered offline article library.
Neither surface changes the graph, audio, undo history, selection, or viewport.

## Selection-specific guidance

The **Guide** tab shows architecture or listening guidance only when it is
relevant to the selected block or active factory. Barr cards retain three
evidence labels:

- **Documented Barr / MIDIVerb** states what cited research supports.
- **This reconstruction** identifies an approximation or implementation choice.
- **Listen / Notice** gives a concrete perceptual or structural question.

The Mono Sum, stereo taps, tank stages, diffuser, filter, input, and output have
specific cards. An unrelated selection never falls back to a permanently
repeated Barr overview. Factory-specific panels remain available for Dense
Figure Eight, Four-Line Dense Room, shimmer, and Reverse Cosmic designs. The ×
button dismisses only the current card and does not change selection or audio.

## Offline article library

**Help → User Guide**, **Keith Barr Architectures**, and **Module Reference**
open a modal article library. The library renders the repository Markdown with
headings, tables, code/ASCII diagrams, lists, links, breadcrumbs, and searchable
navigation. The source path and offline/rendered provenance are visible in each
article. Known local documentation links stay inside the library; external
source links remain links.

Closing with **Return to Editor** or Escape restores focus and exposes the same
graph, audio, history, viewport, and selection because Help state is never part
of patch persistence. Compact widths stack navigation above the article. Native
focus order, visible focus styling, textual evidence labels, and the global
reduced-motion rule remain authoritative.

## Preference migration

The obsolete `reverb-playground-teaching` local-storage preference is removed
once during startup and otherwise ignored. No migration touches patch JSON or
host state. Contextual explanations and measured response annotations are now
available consistently rather than being enabled in several different places.

## Verification

Web tests cover the article catalog, search, local-link routing, rendered block
types, keyboard return, absence of duplicate Learn controls, contextual-card
specificity, vocabulary completeness, and patch-persistence separation. Current
M28 evidence is stored under `artifacts/ui/m28-guidance-rendered-help/`.
