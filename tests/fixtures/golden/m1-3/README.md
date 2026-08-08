# M1.3 offline-render goldens

These three stereo PCM16 WAV files are generated entirely by Reverb Playground engine version 0.1 at 48 kHz for 250 ms (12,000 frames): deterministic silence, a unit impulse on the left input, and bounded pseudo-random input from the documented fixed seed. They contain no recorded, sampled, ROM-derived, or third-party audio and are legally redistributable under the repository license.

The fixtures total about 141 KiB. Tests compare every channel/sample after PCM16 quantization with a tolerance of one least-significant bit and identify the first mismatching frame, channel, expected value, actual value, and difference. The one-LSB tolerance covers permitted compiler/platform floating-point variation; the primary Windows/MSVC build additionally checks exact FNV-1a hashes in machine-readable analysis.

Regenerate intentionally after an accepted engine change:

```powershell
build/windows-msvc/src/render/Debug/reverb_render_cli.exe --input impulse --sample-rate 48000 --duration-ms 250 --output tests/fixtures/golden/m1-3/impulse-48k-250ms.wav
```

Use `silence` and `noise` for the other two files. Review analysis and audible/architectural impact before accepting updated goldens; never refresh them merely to make a regression pass.
