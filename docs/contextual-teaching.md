# Contextual teaching

M2.5 turns the inspector into an optional learning surface without changing its editing role. With **Learn On**, the empty inspector explains the whole reference and each selected block receives a short contextual card. The card sits below ordinary identity, parameter, and history controls inside the already-scrollable inspector, so it never covers the patch canvas.

## Evidence labels

Every card separates three ideas:

- **Documented Barr / MIDIVerb** states what the cited research supports;
- **This reconstruction** identifies the product's deliberate approximation or current limitation;
- **Listen / Notice** gives the user a concrete perceptual or structural question.

This distinction is especially important for feedback. Barr's mature vocabulary and analyzed MIDIVerb programs use recirculation, but the current M2 development reference is a finite feed-forward slice. Tank cards say that plainly instead of drawing or implying a feedback path that the runtime does not contain.

## Required contexts

- **Mono Sum** explains the documented mono-summed input and why this stereo-input plugin deliberately becomes mono at one visible block.
- **Left Tap** and **Right Tap** explain different views of one shared field and distinguish the simplified terminal allpasses from literal ROM tap maps.
- **Tank** explains why diffusion is valuable inside a historical feedback loop and why future feedback must remain explicit, delayed, bounded, and visible.
- Input, output, low-pass, diffuser, and the unselected reference overview provide the same fact/implementation separation.

## Dismissal and preference

The × button dismisses the current context without changing selection or parameters. Selecting another context can show its explanation. **Learn On/Off** disables or enables all teaching cards and stores that preference locally in the embedded browser profile. Editing, audition, save/load, and canvas navigation continue while teaching is off.

## Offline research

**Read Offline Architecture Research** opens a dismissible, scrollable reader containing the complete repository document [Keith Barr reverb architectures](keith-barr-reverb-architectures.md). Vite bundles that Markdown text into the editor JavaScript at build time; opening it performs no network request and does not depend on a browser or external site.

## Verification

Web tests assert the required mono-sum, shared stereo-tap, and honest feedback language. Interactive QA verified context switching, per-card dismissal, global disable, preference persistence after reload, and opening/closing the bundled research reader. Evidence is stored under `artifacts/ui/m2-5-contextual-teaching`.
