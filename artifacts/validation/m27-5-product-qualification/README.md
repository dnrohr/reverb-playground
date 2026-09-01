# M27.5 exact-candidate host validation

- Source commit: `266b62ac8acaeff485c055c2b152def6b10bcc8f`
- Package: `ReverbPlayground-0.1.0-windows-x64.zip`
- Package SHA-256: `FFFC77C144A1AE05463884E96A75260B9AF4D2AC457C39C259F0A1C09A583E29`
- Package manifest/identity validation: pass
- Packaged standalone cold-start smoke: alive after 10 seconds
- pluginval: 1.0.4, strictness 10, seed `0x960096`, success
- Steinberg VST3 validator: 3.8.1 build 84, extensive mode, 537 passed / 0 failed

[`pluginval-release.txt`](pluginval-release.txt) and
[`vst3-validator-release.txt`](vst3-validator-release.txt) are the complete host
logs for the VST3 produced from the source commit above. The package is generated
outside Git but its identity, checksum, required standalone/VST3 layout, license,
notices, and installation documentation were validated by the packaging tool.
