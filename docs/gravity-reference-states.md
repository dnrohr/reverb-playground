# Gravity Diffusion reference states

M9.4 freezes three measurable reference settings before the graph becomes a
factory patch. They are tuning evidence, not three hidden algorithms or final
preset names. Each state uses the same expanded M9.3 graph and changes only the
five visible Macro values.

Regenerate all evidence from a clean configured checkout with:

```powershell
.\scripts\generate_gravity_references.ps1 -Configuration Release
```

The command builds the checked-in native generator and writes three five-second
48 kHz stereo PCM16 WAVs plus adjacent format-v1 JSON measurements under
[`artifacts/audio/m9-4-gravity-references`](../artifacts/audio/m9-4-gravity-references/).
The stimulus is a unit impulse on the left input at frame zero. Every runtime is
freshly prepared, so LFO phase and all delay/filter state begin deterministically.

## Fixed settings

| Reference | Gravity | Size | Feedback | Damping | Modulation |
|---|---:|---:|---:|---:|---:|
| Inverse | -1.00 | +1.00 | +0.50 | +0.15 | +0.25 |
| Bloom | 0.00 | -0.35 | +1.00 | 0.00 | +1.00 |
| Forward | +1.00 | -0.65 | -0.80 | +0.10 | +0.15 |

Size separates buildup time without changing Gravity's sign. Inverse uses the
largest network and moderate feedback; Bloom keeps Gravity centered while using
a mildly compact geometry and long, diffused return; Forward uses a compact network and short return. The
different Feedback settings are intentional reference tuning, not evidence
that Gravity itself changes decay.

## Machine-readable results

All shape comparisons use the M8 centered 20 ms energy smoother and a common
520 ms comparison horizon from causal onset. The horizon covers the direct
eight-stage buildup in all three settings while placing the Inverse peak in its
late quarter. The WAVs are matched by one scalar per state to Bloom's complete
five-second integrated stereo energy; raw metrics remain in each JSON file.

| Reference | Causal onset | Smoothed peak frame | Early/late ratio | Raw post-peak energy | Match gain | Matched energy |
|---|---:|---:|---:|---:|---:|---:|
| Inverse | 5,766 (120.1 ms) | 419.48 ms | -2.523 dB | 67.1% | 0.99938 | -20.5829 dB |
| Bloom | 968 (20.2 ms) | 153.92 ms | +7.384 dB | 73.2% | 1.00000 | -20.5829 dB |
| Forward | 852 (17.8 ms) | 8.15 ms | +14.283 dB | 100.0% | 0.96497 | -20.5829 dB |

The centered smoothing window can locate Forward's energy maximum up to 10 ms
before its first unsmoothed active frame. This is a documented measurement
edge effect, not pre-input audio; the first nonzero sample remains frame 852.
Inverse is therefore a causal rising envelope, not sample reversal. Its peak is
266 ms later than Bloom and 411 ms later than Forward. Bloom's 73.2% post-peak
energy confirms a substantial tail rather than a terminal impulse. In the first
700 ms from onset, 15 of 70 non-overlapping 10 ms windows exceed one-millionth
of the strongest window, and the strongest three contain only 32.5% of that
interval's energy. This is the reproducible density/anti-three-tap criterion;
it does not claim perceptual smoothness without listening.

The adjacent JSON files contain exact control values, raw and matched metrics,
normalization gain, render dimensions, and per-channel PCM16 FNV-1a hashes.
Native tests lock those hashes, finite output, causal ordering, loudness match,
and byte-equivalent floating renders after schema serialization/reload.

## Critical audition notes

These notes are an engineering listening checklist paired with the checked-in
audio, deliberately separated from measured claims above:

- **Inverse:** listen for whether the sparse first arrivals reveal the depth
  sequence before the 419 ms crest. The likely failure mode is audible stepping
  or metallic ringing as the deep taps accumulate. Confirm that the alternating
  LFO paths create width without an obvious pitch wobble and that the tail after
  the crest feels continuous rather than like a second event.
- **Bloom:** listen for a soft attack into a distributed center cluster, not three
  isolated taps. Its high Feedback setting makes flutter and narrow-band ringing
  the most important failure checks. Stereo motion should be slow and decorrelated,
  without the image repeatedly collapsing to the center.
- **Forward:** listen for an immediate conventional onset followed by a smooth
  reduction in density. The compact Size can expose coloration and discrete
  early echoes; check that it reads as a decay rather than a short multitap delay.
  Its low Feedback setting should avoid a persistent resonant tail.

The checklist does not convert preference into a pass/fail measurement. M9.6
will add formal listening/host validation and may motivate parameter retuning;
any such change must regenerate the WAVs, JSON, hashes, and this table together.

UI unchanged in M9.4. This task adds offline tuning evidence and no shipped
factory entry or editor styling, so the project UI-evidence policy requires no
screenshot or video.
