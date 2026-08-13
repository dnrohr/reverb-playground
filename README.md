# Reverb Playground

An open-source visual instrument for constructing, hearing, and understanding algorithmic reverbs.

Licensed under [GNU AGPLv3 only](LICENSE). Dependency terms, generated-asset provenance, and contribution requirements are documented in [Third-party notices](THIRD_PARTY_NOTICES.md), [Asset provenance](ASSET_PROVENANCE.md), and [Contributing](CONTRIBUTING.md).

The project begins with a visible reconstruction of the Keith Barr/Alesis MIDIVerb-I signal architecture, then grows into a small reverb-specific patching environment. The diagram is the program: mono audio cables, explicit summing, delays, allpasses, filters, feedback paths, modulation, and stereo output branches remain visible and inspectable.

## Project status

The project has a deterministic Barr-inspired DSP reference and an embedded schematic editor whose visible parameters continuously control the native runtime with smoothing, delay crossfades, and undo/redo. The current downloadable build is the [Windows v0.1.0 alpha prerelease](https://github.com/dnrohr/reverb-playground/releases/tag/v0.1.0-alpha.1); its notes disclose the supported boundary and deferred external usability validation.

Watch the [alpha demonstration](artifacts/ui/m7-6-alpha-release/reverb-playground-alpha-demo.mp4), or report a reproducible problem through [GitHub Issues](https://github.com/dnrohr/reverb-playground/issues).

## Documentation

- [Product specification](docs/visual-reverb-constructor-spec.md)
- [Execution roadmap](docs/roadmap.md)
- [Current progress](docs/progress.md)
- [Windows package installation](docs/windows-package-installation.md)
- [Getting started with the Barr reference](docs/getting-started-barr-tutorial.md)
- [Module and visualization reference](docs/module-and-visualization-reference.md)
- [Factory patch compatibility](docs/factory-patch-compatibility.md)
- [Alpha usability and safety protocol](docs/alpha-usability-safety-protocol.md)
- [Windows package and host validation](docs/windows-alpha-package-and-host-validation.md)
- [Schematic editor interactions](docs/schematic-editor-interactions.md)
- [Continuous parameter editing](docs/continuous-parameter-editing.md)
- [Saving and loading patches](docs/patch-saving-and-loading.md)
- [Contextual teaching](docs/contextual-teaching.md)
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

No transformed MIDIVerb ROM data is part of this repository. The distributable Barr-inspired reference is implemented from documented primitives, original project parameter choices, and generated fixtures.

## Building

The production scaffold uses pinned source dependencies and a Visual Studio 2022 CMake preset. See the [development guide](docs/development.md) for prerequisites and verified commands.
