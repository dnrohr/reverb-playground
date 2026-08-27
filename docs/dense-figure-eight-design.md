# Dense figure-eight reference

Status: M20.1 architecture and verification contract.

The Dense Figure Eight is the first deliberately dense late-reverb reference in Reverb Playground. It is a visible 37-block, 44-cable graph rather than a hidden DSP primitive. Four short input allpasses turn the initial impulse into a small packet of energy, then opposite-polarity injections feed two unequal branches. Each branch contains three more allpasses, two delays, damping, and a return gain; the branches return into one another to form the figure eight. Unequal signed taps are mixed and diffused again before the stereo output.

## Decay calculation

The nominal branch traversal times are 209.3 ms and 242.9 ms. For a requested decay `T60`, each return uses:

`g = 10 ^ (-3 * branchSeconds / T60)`

Consequently one complete A-plus-B circulation reaches -60 dB after `T60`, within floating-point tolerance. The requested range is 0.4–8 seconds and return magnitude is additionally capped at 0.98. This is a low-frequency target: the two loop lowpasses intentionally make high frequencies decay faster, so a broadband measurement is expected to be no longer than the nominal value. M20.2 will tune and qualify an audible factory and publish measured tolerances for the complete response.

## Density and motion

Delay lengths and allpass lengths are unequal across branches to avoid a shared obvious repeat period. Two independent, slow LFOs modulate selected tank allpasses by at most 1.5 ms; the four input diffusers remain stationary. The extraction taps use different branch locations, weights, signs, and final allpass times. This grows density without relying on damping to conceal discrete echoes.

The executable comparison requires middle-region normalized echo density to exceed both the Barr reference and Gravity Diffusion by at least 0.08, and late active-peak rate to exceed Gravity Diffusion by at least 2×. These are development gates, not claims that one architecture is universally better.

## Safety and determinism

Every feedback cycle crosses the explicit delay blocks in both branches. Compilation identifies one sample-wise feedback region. Tests cover exact JSON round trips, finite and sub-unity output at 44.1, 48, and 96 kHz, minimum and maximum controls, and sample-exact results with 64- and 257-sample processing partitions. There is no UI change in M20.1; factory controls, teaching overlays, listening evidence, and screenshots belong to M20.2.
