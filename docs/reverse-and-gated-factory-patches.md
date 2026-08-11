# Reverse-envelope and gated factory patches

M6.3 ships two editable schema-v2 factory patches beside the Barr reference. Both are ordinary public graphs: loading one creates the same visible nodes and mono cables that a user can place by hand. There is no factory-only processor, opaque macro, imported impulse response, lookahead, or hidden gate.

## Causal Reverse Envelope

`factory-patches/causal-reverse-envelope.rvp.json` is deliberately named for what it does. It is causal and does not reverse sample order. Stereo input is explicitly summed, diffused through two Allpass blocks, and sent to three parallel Delay/Gain branches:

| Branch | Delay | Gain |
| --- | ---: | ---: |
| Early | 45 ms | 0.25 |
| Middle | 115 ms | 0.55 |
| Peak | 210 ms | 0.95 |

Explicit Sum blocks recombine the branches. A 6.5 kHz Low-pass supplies tone, a 0.75 Gain supplies wet level, and unequal 8.7/14.3 ms output Allpasses create stereo difference. Increasing delay and absolute gain are the visible cause of the rising response. The 48 kHz fixture begins at 40 ms and reaches its smoothed peak 195 ms later; its following decay is gradual enough that a conventional RT60 estimate remains meaningful.

## Level-Gated Room

`factory-patches/level-gated-room.rvp.json` builds a short dense room from three high-coefficient Allpasses, a 7.2 kHz Low-pass, a 0.8 level Gain, and unequal stereo Allpasses. In parallel, a visible Envelope Follower measures the summed input with 0.1 ms attack and 20 ms release. Its control cable drives two visible Hold Gates with threshold 0.004, 2 ms attack, 120 ms hold, and 8 ms release.

The follower threshold is low enough for both a unit-sample fixture and the product's safe 0.1-peak live audition impulse to open the gate at 44.1, 48, and 96 kHz. The design remains level-triggered: sustained or repeated input can retrigger it, unlike a fixed window. At 48 kHz the unit-fixture peak occurs 5 ms after onset and the response crosses the -40 dB cutoff 205 ms later. Its 84.79 dB one-window drop makes a smooth exponential RT60 interpretation misleading, so the measurement marks RT60 as not meaningful.

## Reproducible comparison

Envelope measurements sum left/right squared energy into non-overlapping 10 ms windows. Onset is the first window above `peak * 1e-8`; cutoff is the first post-peak window at least 40 dB below peak. Residual energy is everything from that cutoff onward divided by total energy. These rules are deterministic and intentionally separate a late-rising response from an abruptly truncated one.

| 48 kHz / 1 s impulse | Onset | Time to peak | Peak to -40 dB | Residual energy | Largest 10 ms drop | RT60 meaningful |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Causal Reverse Envelope | 40 ms | 195 ms | 125 ms | 0.00296% | 3.87 dB | Yes |
| Level-Gated Room | 0 ms | 5 ms | 205 ms | 0% | 84.79 dB | No |

The checked-in WAVs contain only a mathematical unit impulse processed by project code:

- [`causal-reverse-envelope-48k.wav`](../artifacts/audio/m6-3-factory-patches/causal-reverse-envelope-48k.wav)
- [`level-gated-room-48k.wav`](../artifacts/audio/m6-3-factory-patches/level-gated-room-48k.wav)

Their adjacent analysis, response-measurement, and envelope-measurement JSON files are the machine-readable evidence. Regenerate one with:

```powershell
build/windows-msvc/src/render/Release/reverb_render_cli.exe `
  --patch factory-patches/causal-reverse-envelope.rvp.json `
  --input impulse --sample-rate 48000 --duration-ms 1000 `
  --output artifacts/audio/m6-3-factory-patches/causal-reverse-envelope-48k.wav `
  --analysis artifacts/audio/m6-3-factory-patches/causal-reverse-envelope-analysis.json `
  --measurements artifacts/audio/m6-3-factory-patches/causal-reverse-envelope-response-measurements.json `
  --envelope-measurements artifacts/audio/m6-3-factory-patches/causal-reverse-envelope-envelope-measurements.json
```

The editor's **Factory Patch** menu switches between these two graphs and the Barr reference. Selecting a factory patch replaces the editable canvas and publishes that visible graph for audition; saving produces an ordinary `.rvp.json` document.

Reviewed native evidence at 125% Windows scaling:

- [`01-causal-reverse-envelope.png`](../artifacts/ui/m6-3-factory-patches/01-causal-reverse-envelope.png) shows all 17 visible blocks and 20 cables fitted from input through output.
- [`02-level-gated-room.png`](../artifacts/ui/m6-3-factory-patches/02-level-gated-room.png) shows the follower, dashed branched control cable, two gates, and complete stereo route.
- [`factory-patch-switching.mp4`](../artifacts/ui/m6-3-factory-patches/factory-patch-switching.mp4) demonstrates Barr/reference, reverse-envelope, and gated selection plus live graph revision publication.
