# M32 crash recovery and emergency-safety qualification

- The Release gate passed 208 web tests, 25 repository-policy tests, and 256
  native/CLI tests, including a Windows minidump fixture and muted-export gate.
- `sanitized-report.json` records the bounded text-report schema without a real
  incident ID, path, graph hash, or process memory.
- `pluginval-release.txt` is the complete Tracktion pluginval 1.0.4 strictness
  10 run using seed `0x960096`; it ends in `SUCCESS`.
- `vst3-validator-release.txt` is the complete Steinberg VST3 validator 3.8.1
  extensive run; 537 tests passed and none failed.
- Desktop and minimum-size recovery/Help evidence is under
  `artifacts/ui/m32-crash-recovery/`.

The actual `.dmp` fixture is deliberately not committed because minidumps may
contain process memory. The test verifies that the matching dump is non-empty
and debugger-readable through `MiniDumpWriteDump`, then removes only its own
temporary incident pair.
