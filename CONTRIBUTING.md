# Contributing

Reverb Playground welcomes focused bug reports, research corrections, documentation, tests, factory patches, and implementation changes.

## Before contributing

- Discuss large product, schema, DSP-architecture, dependency, or licensing changes before investing substantial work.
- Keep real-time DSP allocation-, lock-, log-, filesystem-, and network-free.
- Do not submit Alesis/MIDIVerb firmware, BarrVerb's transformed `rom.h`, decoded proprietary instruction tables, commercial impulse responses, unlicensed samples, fonts, screenshots, or other material you cannot redistribute under the project terms.
- Record the origin and license of every new dependency, fixture, preset, image, audio file, or video in `THIRD_PARTY_NOTICES.md` or `ASSET_PROVENANCE.md` as appropriate.

## Change requirements

Run `scripts/verify.ps1` before submitting. A change should include proportionate tests and documentation. UI changes require a reviewed screenshot under `artifacts/ui/<task-slug>/`; interactions, animation, or audio-reactive behavior also require a short video. Do not commit build directories, downloaded dependency trees, research clones, or temporary captures.

Contributions are reviewed under the project code and documentation standards and may be revised or declined. By contributing, you agree that your contribution is publicly distributed under AGPL-3.0-only and you certify it under the Developer Certificate of Origin 1.1 in `DCO`.

## Developer Certificate of Origin

Add a sign-off to every commit:

```text
Signed-off-by: Your Name <you@example.com>
```

Use your real name or another identity you are legally entitled to use for this certification. `git commit -s` adds the line automatically. The sign-off is a certification of origin and permission, not a copyright assignment. Contributors retain copyright in their work.
