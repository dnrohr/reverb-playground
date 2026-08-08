# Reverb Playground

An open-source visual instrument for constructing, hearing, and understanding algorithmic reverbs.

The project begins with a visible reconstruction of the Keith Barr/Alesis MIDIVerb-I signal architecture, then grows into a small reverb-specific patching environment. The diagram is the program: mono audio cables, explicit summing, delays, allpasses, filters, feedback paths, modulation, and stereo output branches remain visible and inspectable.

## Project status

The project is in the foundation milestone. Production code has not been scaffolded yet.

## Documentation

- [Product specification](docs/visual-reverb-constructor-spec.md)
- [Execution roadmap](docs/roadmap.md)
- [Architecture decisions](docs/adr/README.md)
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
