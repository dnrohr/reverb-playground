# Output normalization UI evidence

- `output-gain-and-auto-normalize.png` shows the selected Stereo Output at a
  high `42x` gain and the enabled **Auto-normalize output** capture option.
- `stereo-output-gain-inspector.png` shows the same persisted gain in the node,
  numeric field, state layers, and slider with the documented `0...100` range.
- `startup-logo.png` is a native `PrintWindow` capture of the freshly built
  standalone startup shell at 125% Windows display scaling. It shows the full
  embedded icon in the right-side space without clipping the status copy.

The two editor screenshots were captured from the current Vite development build
at the default desktop viewport on 2026-09-05. The development fixture supplies the
same runtime snapshot contract as the native editor; native DSP behavior is
covered by `BarrReferenceTests`, `AcyclicRuntimeTests`, and the complete CTest
matrix.
