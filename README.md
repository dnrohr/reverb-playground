# Reverb Playground

An open-source visual instrument for constructing, hearing, and understanding algorithmic reverbs.

The project begins with a visible reconstruction of the Keith Barr/Alesis MIDIVerb-I signal architecture, then grows into a small reverb-specific patching environment. The diagram is the program: mono audio cables, explicit summing, delays, allpasses, filters, feedback paths, modulation, and stereo output branches remain visible and inspectable.

## Project status

The project has a deterministic Barr-inspired DSP reference and an embedded schematic editor whose visible parameters continuously control the native runtime with smoothing, delay crossfades, and undo/redo.

## Documentation

- [Product specification](docs/visual-reverb-constructor-spec.md)
- [Execution roadmap](docs/roadmap.md)
- [Current progress](docs/progress.md)
- [Schematic editor interactions](docs/schematic-editor-interactions.md)
- [Continuous parameter editing](docs/continuous-parameter-editing.md)
- [Saving and loading patches](docs/patch-saving-and-loading.md)
- [Architecture decisions](docs/adr/README.md)
- [Patch format v1](docs/patch-format.md)
- [Keith Barr reverb architecture research](docs/keith-barr-reverb-architectures.md)
- [BarrVerb code review](docs/barrverb-code-review.md)
- [Research source map](docs/sources.md)

## Initial delivery target

- Windows 10/11
- Standalone application
- VST3 plugin
- C++20, CMake, and JUCE 8
- React Flow interaction prototype with a host-compatibility checkpoint before the production UI is fixed

No transformed MIDIVerb ROM data is part of this repository. The first distributable Barr-inspired patch must be implemented from documented primitives and legally redistributable parameters or generated fixtures.

## Building

The production scaffold uses pinned source dependencies and a Visual Studio 2022 CMake preset. See the [development guide](docs/development.md) for prerequisites and verified commands.
