# Dense figure-eight reference

Status: M20.1 architecture and verification contract.

The Dense Figure Eight is the first deliberately dense late-reverb reference in Reverb Playground. Its shipped factory is a visible 40-block, 50-cable graph rather than a hidden DSP primitive. Four short input allpasses turn the initial impulse into a small packet of energy, then opposite-polarity injections feed two unequal branches. Each branch contains three more allpasses, two delays, damping, and a return gain; the branches return into one another to form the figure eight. Unequal signed taps are mixed and diffused again before the stereo output.

## Decay calculation

The nominal branch traversal times are 209.3 ms and 242.9 ms. For a requested decay `T60`, each return uses:

`g = 10 ^ (-3 * branchSeconds / T60)`

Consequently one complete A-plus-B circulation reaches -60 dB after `T60`, within floating-point tolerance. The requested range is 0.4–8 seconds and return magnitude is additionally capped at 0.98. This is a low-frequency target: the two loop lowpasses intentionally make high frequencies decay faster, so a broadband measurement is expected to be no longer than the nominal value. M20.2 will tune and qualify an audible factory and publish measured tolerances for the complete response.

## Density and motion

Delay lengths and allpass lengths are unequal across branches to avoid a shared obvious repeat period. Two independent, slow LFOs modulate selected tank allpasses by at most 1.5 ms; the four input diffusers remain stationary. The extraction taps use different branch locations, weights, signs, and final allpass times. This grows density without relying on damping to conceal discrete echoes.

The executable comparison requires middle-region normalized echo density to exceed both the Barr reference and Gravity Diffusion by at least 0.08, and late active-peak rate to exceed Gravity Diffusion by at least 2×. The selected 71.1/97.3/83.7/109.1 ms tank delays came from a deterministic curated-candidate comparison that rejected shared lengths and obvious integer relationships; M22 generalizes that one-off process into a reusable tuning tool. These are development gates, not claims that one architecture is universally better.

## Factory controls

The checked factory adds three explicit bipolar macros without hiding any DSP:

- **Decay** moves both signed, delay-calculated return gains by a conservative 14 percent of their nominal value.
- **Tone** moves both loop low-pass cutoffs while retaining their small branch offset.
- **Motion** moves the two independent LFO rates; modulation remains visible on the selected tank allpasses.

The JSON asset is generated from the native builder and admitted by exact SHA-256. Native and browser tests require it to be identical, editable, schema-v2 stable, and composed only from public modules.

## Safety and determinism

Every feedback cycle crosses the explicit delay blocks in both branches. Compilation identifies one sample-wise feedback region. Tests cover exact JSON round trips, finite and sub-unity output at 44.1, 48, and 96 kHz, minimum and maximum controls, and sample-exact results with 64- and 257-sample processing partitions. Prepared memory stays within the fixed arena budget and estimated work remains below the explicit qualification ceiling. There is no pitch shifter; the only pitch-active behavior is the documented subtle moving-allpass Doppler.

Representative speech/percussion, sustained-chord, and full-mix fixtures must all export complete bounded stereo WAV files through the factory. Current desktop and minimum-width UI evidence is stored under [`artifacts/ui/m20-2-dense-figure-eight/`](../artifacts/ui/m20-2-dense-figure-eight/).
