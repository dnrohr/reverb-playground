# Roadmap progress

Last updated: 2026-08-08

| Task | Status | Evidence |
|---|---|---|
| M0.1 Select the primary implementation stack | Complete | ADR 0001; Markdown links verified; commit `e913f5a` on `origin/main` |
| M0.2 Create the project skeleton | Complete | Windows Debug and Release standalone/VST3 builds; Debug and Release native tests; standalone smoke launch and screenshot |
| M0.3 Establish continuous integration and quality checks | Complete | Shared local/CI verifier; warnings-as-errors; repository checks; deliberate failing-test proof |
| M0.4 Define the graph schema v1 | Next | Not started |

## M0.2 verification

Environment:

- CMake 4.4.2
- Visual Studio 2022 Build Tools
- MSVC 19.44.35228.0
- Windows SDK 10.0.26100.0

Commands:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel 2
ctest --preset windows-debug
cmake --build --preset windows-release --parallel 2
ctest --test-dir build/windows-msvc -C Release --output-on-failure
```

Results:

- Debug standalone: built and launched.
- Debug VST3: built with generated `moduleinfo.json`.
- Release standalone: built.
- Release VST3: built with generated `moduleinfo.json`.
- Debug tests: 2/2 passed.
- Release tests: 2/2 passed.
- UI evidence: [`artifacts/ui/m0-2-project-skeleton/standalone-smoke-shell.png`](../artifacts/ui/m0-2-project-skeleton/standalone-smoke-shell.png).

## M0.3 verification

Required local/CI command:

```powershell
./scripts/verify.ps1 -Configuration Debug
```

Results:

- Repository checks passed for all project files.
- MSVC project-owned targets compiled with `/W4 /WX /permissive-`.
- Debug standalone, VST3, and native test targets built.
- Native tests passed 2/2 after restoration.
- A temporary `REQUIRE_FALSE` assertion was added to `DspGainTests.cpp`; CTest failed 1/2 with exit code 8, proving the test gate rejects failures. The assertion was removed before the final passing verification.
- CI uses the same `scripts/verify.ps1` entry point on pushes to `main` and pull requests.
- UI unchanged; no new screenshot or video required.
