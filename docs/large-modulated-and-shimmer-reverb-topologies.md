# Large modulated, inverse, and shimmer reverb topologies

This note separates documented behavior from topology inference. Product names
identify sources only; Reverb Playground does not claim to reproduce a
proprietary algorithm or preset.

## Public evidence

Eventide's [Blackhole user guide](https://downloads.eventide.com/audio/manuals/plug-ins/Blackhole%2BUser%2BGuide.pdf)
documents a negative **Gravity** region for inverse/reverse-like settings,
**Size** for virtual scale, up to 2,000 ms of predelay, tail shelving, and
modulation depth/rate. It says modulation can reduce ringing and add motion.
The more detailed [Blackhole Immersive guide](https://downloads.eventide.com/audio/manuals/plug-ins/Blackhole%20Immersive%20User%20Guide.pdf)
adds that larger Size values take longer to build and decay more smoothly,
negative Gravity progresses from smeared puffs toward asymmetric inverse
behavior, and feedback/crossfeed extend and animate the decay. These are useful
behavioral constraints, but neither manual publishes the internal graph.

Sean Costello's [ValhallaShimmer control notes](https://valhalladsp.com/2010/11/27/valhallashimmer-the-controls/)
describe networks of diffusors that behave like delay lines at zero diffusion,
then spread each repeat into reflection clusters as diffusion rises. Large modes
use longer delays and high echo density; slow modulation moves delay lengths.
His [envelope notes](https://valhalladsp.com/2010/12/01/valhallashimmer-tips-and-tricks-adjusting-the-reverb-envelope/)
explain the interaction: delay length establishes scale, diffusion grows echo
density, and feedback recirculates that increasingly dense result.

True shimmer has a further non-negotiable element. Costello's
[Eno/Lanois topology summary](https://valhalladsp.com/2010/05/11/enolanois-shimmer-sound-how-it-is-made/)
places an octave-up pitch shifter and a long modulated reverb inside a global
feedback loop, with gain and equalization controlling stability and evolution.
ValhallaShimmer similarly puts pitch shift in the diffuse-delay feedback loop;
its reverse modes reverse individual pitch-shifter grains, not the entire
reverb response.

## Topology families worth building

| Family | Visible construction | What it proves | Missing work |
| --- | --- | --- | --- |
| Modulated cosmic inverse | Increasing Delay/Gain taps → diffused delayed feedback → damping → unequal stereo diffusion; independent slow LFOs move allpass times | Causal late rise, large recirculating space, reduced static ringing, stereo motion | Implemented as **Modulated Cosmic Reverse** |
| Classic shimmer | Long modulated diffusion network with a visible +12-semitone shifter, damping, and bounded feedback | Each circulation rises one octave while density grows | Requires a pitch-shift module and aliasing/stability contract |
| Reverse-grain shimmer | Windowed/granular pitch shift whose individual grains run backward inside the diffuse feedback loop | Smooth organ-like upward/downward evolution without claiming whole-tail reversal | Requires grain size, overlap/window, latency, reset, and CPU budgets |
| True reverse reverb | Finite capture or reversed IR, explicit lookahead/latency, then wet/dry alignment | Exact sample-order reversal and optional pre-echo | Requires capture/convolution and host-latency reporting |

## Implemented original design: Modulated Cosmic Reverse

The factory graph is an original project-authored interpretation of the public
behavior above. It is not named Blackhole and does not copy a preset:

1. Two input allpasses feed 80/240/520 ms branches weighted 0.18/0.42/0.72.
2. Explicit sums form a causal rising envelope.
3. A 19.7/31.3 ms allpass pair, 173 ms Delay, 4.8 kHz Low-pass, and 0.58 Gain
   form a legal feedback loop whose every cycle crosses the visible Delay.
4. Independent 0.11 Hz sine and 0.073 Hz triangle LFOs move the two tank and
   two output allpass taps. This is deliberately pitch-active modulation.
5. A 0.55 output gain and unequal 11.9/17.3 ms allpasses create bounded stereo.

At 48 kHz, the checked-in five-second impulse fixture begins at 253 ms and its
10 ms smoothed energy peaks 455 ms later. The peak is about 0.0447 for a unit
impulse; multirate tests require a 0.1 audition impulse and bounded noise to
remain finite and no louder than their input bound.

## Shimmer implementation contract

Do not label detuned delay modulation as shimmer. The future Pitch Shift block
must expose semitones, grain/window length, overlap, channel policy, latency,
and quality. Its prepared processor must allocate all windows before
publication, reset deterministically, remain finite inside delayed feedback,
and document aliasing. The first factory should use +12 semitones, low shifted
mix, visible damping after the shifter, and conservative feedback; tests must
show octave energy increasing on later circulations rather than only chorusing.

