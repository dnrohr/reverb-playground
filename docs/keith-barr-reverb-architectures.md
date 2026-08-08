# Keith Barr reverb architectures

## Executive summary

Keith Barr's mature reverb vocabulary was built around delay allpasses placed inside feedback structures. His own account describes a progression:

1. Basic Schroeder-derived structures sounded thin or tinny.
2. Putting allpasses inside comb/feedback loops improved echo-density growth.
3. Summing several independently regenerating paths created frequency peaks.
4. Joining the sections into one large loop produced a flatter, more thoroughly mixed, richer decay.

His preferred later building block was approximately:

```text
input diffusion -> inject at several points

        +->[ AP -> AP -> delay ]->[ AP -> AP -> delay ]-+
        |                                               |
        +-------------------- feedback -----------------+

                         taps -> stereo outputs
```

Barr told Sean Costello that he liked “2AP, delay, 2AP, delay, in a loop,” with each long delay somewhat shorter than the sum of the two preceding allpass delays. Input could be injected at multiple points, while only a small number of internal taps fed the output. The important result is that recirculating energy encounters new diffusion and delay sections before repeating.

That description is a mature family resemblance, not a universal schematic. Barr and Costello both describe many variations in input points, output taps, series allpass count, diffusion stages, and feedback routing.

## The design problem Barr was solving

An artificial reverb needs several perceptual properties at once:

- a convincing onset or early-reflection region;
- echo density that grows quickly enough to stop sounding like discrete delays;
- a tail without a few obvious pitches or flutter patterns;
- controllable decay and high-frequency loss;
- stereo outputs that are related but not identical;
- stability and acceptable peak gain;
- an implementation cheap enough for the target hardware.

Barr emphasized psychoacoustics and empirical listening. His forum explanation was that listeners cannot precisely deconvolve a sufficiently complex ambient return. The goal therefore was not necessarily a literal geometrical room simulation; it was a compact network whose time, density, spectrum, and motion evoke a useful space.

## Why allpasses are central

A delay allpass changes phase and impulse shape while retaining a flat long-term magnitude response. One stage turns a single event into a structured series of echoes; several stages make the response progressively denser. This is valuable before and inside a feedback tank because it increases temporal complexity without imposing the static comb magnitude response of a simple multi-tap sum.

Barr contrasted this with equal-weight taps inside a feedback loop. Two taps already create alternating reinforcement and cancellation. More taps can create fewer but much larger coincident peaks. Recirculation repeatedly reinforces those favored frequencies, so the tail can collapse into pronounced ringing. Allpasses provide diffusion with a much better behaved magnitude response.

Practical caveat: an allpass has a flat magnitude response, but it still has poles and a recognizable temporal pattern. Poor delay lengths, coefficients, or excessive feedback can still ring. Long plain delays between allpass groups add a sense of scale and reduce the “all-allpass” signature.

## Architectural evolution

### 1. Early Schroeder-style work

Barr recalled Schroeder's published arrangement as poor sounding when followed too literally and described his MXR digital reverb as tinny. The important historical point is not that Schroeder structures are inherently unusable, but that the sparse canonical arrangement did not supply the density and spectral behavior Barr wanted under his constraints.

### 2. Parallel feedback paths containing allpasses

His next major step was to embed one or more allpasses within comb-like regenerated delays. This makes each circulation generate more echoes than a plain comb. The Quadraverb is reported by Sean Costello, based on correspondence with Barr, as using four such paths/sections.

“Parallel” can be misleading in diagrams of this family. Input may be injected in front of four sections and output taps may be summed in parallel, while the end of one section also feeds the next. The feedback connectivity determines whether the network behaves as four short repeating loops or as one long ring.

### 3. Single allpass loop or ring

Barr's mature solution was one large recirculating loop containing multiple allpasses and delays. Advantages described by Barr and Costello:

- energy traverses the whole network before exactly repeating;
- the sections mix one another rather than reinforcing independent subsection patterns;
- density continues to build around the ring;
- fewer problematic peaks arise than when several regenerated outputs are simply summed;
- input placement and sparse stereo taps give many voicing options without changing the core tank.

Costello reports the single-loop design as definitely present by the MIDIVerb IV and Wedge era. He cautions that the original MIDIVerb likely predates Barr's finalized single-loop approach.

## Alesis MIDIVerb I: architecture forced into four operations

The original MIDIVerb is a special case: its “DSP” is discrete logic rather than a general-purpose signal processor. Brombaugh and Schreiber's reverse engineering documents:

- 6 MHz master clock;
- 3 million DSP operations per second;
- 6 MHz / 256 = 23.4375 ksample/s audio rate;
- 128 fixed instructions per audio sample;
- mono summed input and stereo output;
- 16K x 16-bit dynamic RAM;
- a 16-bit microinstruction containing a two-bit opcode and 14-bit relative address offset;
- no branching and no live coefficient parameters in the DSP program.

The four conceptual operations are:

| Opcode | Data source/write | Accumulator result, conceptually |
|---|---|---|
| `00` | read RAM | `acc + data/2 + sign correction` |
| `01` | read RAM | `data/2 + sign correction` |
| `10` | write accumulator | `acc + acc/2 + sign correction` |
| `11` | write inverted accumulator | an inverted/halved form used for subtraction |

Three instruction positions have hard-wired I/O behavior: step `0x00` writes the ADC sample, `0x60` captures the right DAC value, and `0x70` captures the left DAC value. Accumulator loading is suppressed at those positions.

### Relative-address circular memory

Each microinstruction adds its 14-bit offset to the previous RAM address modulo 16K. The offsets across one 128-instruction program sum so that the address advances by one location after the complete pass. This turns the entire RAM into a continuously crawling circular time base. A program expresses a delay by jumping between relative locations, not by maintaining independent read pointers for named delay objects.

This is an unusually important Barr idea: algorithm, instruction encoding, DRAM row/column timing, and physical parts count were co-designed. Relative addressing eliminated a separate address counter and let each instruction perform a RAM access while the global memory position advanced one sample per program cycle.

### Coefficients without a multiplier

The only native scale is one-half, implemented with a signed right shift. Larger and smaller dyadic coefficients are synthesized through short sequences of loads, repeated 1.5x accumulator growth, inversion, storage to dummy addresses, and addition. The reverse-engineering slides demonstrate 2.53125 and 0.625 examples. This makes coefficients program-space costs, not cheap parameters.

### Allpass construction

The reverse engineering shows a coefficient-0.5 allpass synthesized in four instructions. One analyzed program uses nine allpasses ranging from 76 to 1306 samples. The typical MIDIVerb I program shown by Brombaugh has:

- input low-pass filtering and gain;
- a series diffuser of roughly six allpasses;
- a recirculating tank with three long delays, allpass sections, gain points, and loop low-pass filtering;
- several internal delay taps summed differently to left and right outputs.

This is direct evidence that “the MIDIVerb architecture” is more nuanced than a generic set of parallel combs.

### Analog and fixed-point character

The machine's sound is inseparable from its implementation:

- steep input anti-alias filtering before a roughly 23.4 kHz sampling system;
- 12-bit conversion arranged as a 13-bit-ish DSP interface;
- saturated DAC handoff and analog reconstruction filters;
- fixed-point wrap, complement, sign-correction, and quantization behavior;
- no hardware mute on program change, so the existing RAM/tank must settle;
- time-invariant MIDIVerb I programs, because changing a delay or coefficient means changing microcode.

Limited bandwidth can hide metallic high-frequency detail and resembles air/surface absorption. Noise and grain can also become part of the expected sound rather than merely defects.

## MIDIVerb II and “Bloom”

Brombaugh reports that MIDIVerb II retained a related DSP architecture but moved logic into an ASIC and replaced fixed program EPROM behavior with dual-port RAM accessible by the 8031 controller. That allowed the controller to update code/data for LFO-driven delay sweeps, chorus, and interpolation.

The MIDIVerb II Bloom programs are an artistic use of a constraint. Long series of simple allpasses, commonly around coefficient 0.5, cause the response to rise slowly into a dense wash before decaying. Costello describes coefficients in roughly the 0.5–0.618 range as capable of strongly elongating the attack. Rather than hiding the original “building” tendency, Bloom makes it the effect's identity.

## Modulation

Modulating delay length moves cancellation and resonance frequencies over time, blurring static patterns and adding motion. Barr's later architectures and FV-1 examples often use modulated allpasses/delays. The benefit is not just chorus: it reduces the audibility of fixed ringing and holes. Modulating an allpass coefficient is a different operation and may reduce diffusion or produce a less controlled metallic quality.

MIDIVerb I itself is fixed per program. MIDIVerb II's controller-updated program RAM made real-time modulation possible, reportedly including four-point interpolation.

## Product-era map

| Era/product | What is supported by current evidence |
|---|---|
| MXR digital reverb | Early, sparser work Barr later characterized as tinny. |
| MIDIVerb I | Discrete four-op machine; fixed 128-step programs; extensive 0.5 allpasses; varied microcoded networks. Fully reverse engineered. |
| MIDIFex | Same basic hardware, different program ROM with additional effects. |
| MIDIVerb II | Related ASIC engine; controller can modify dual-port program RAM for modulation; many topologies/presets, including Bloom. Partially compatible with MV I emulators. |
| Quadraverb | Reported four-section/parallel-allpass-loop family; exact preset-by-preset topology still needs primary code or measurement. |
| MIDIVerb IV / Wedge | Costello reports Barr's single large loop as definitely in use. |
| Spin Semiconductor FV-1 era | Barr published compact examples and informal notes; two-instruction allpasses and modulation support make the chip well matched to his design vocabulary. |

## Transferable design principles

These are the strongest lessons for a new design inspired by Barr rather than a literal emulation:

1. Co-design the topology with the machine. Memory access, instruction shape, fixed-point math, and sample rate can suggest the network.
2. Spend computation on temporal mixing. A few carefully connected allpasses can be more valuable than many undiffused taps.
3. Prefer one thoroughly mixed recirculating path over a sum of small resonant paths when richness and spectral evenness matter.
4. Inject broadly and listen sparsely. Multiple injection points and a few decorrelated internal output taps can create width without independent stereo tanks.
5. Treat bandwidth as part of the architecture. Filtering before and within the loop controls both realism and the exposure of artifacts.
6. Use modulation to move defects, not merely to add pitch wobble.
7. A “flaw” can become a program. Bloom turns slow density growth into a musically distinctive envelope.
8. Do not mistake a family of techniques for one canonical Barr algorithm.

## Open research questions

- Decompile and graph all 63 MIDIVerb I programs to classify their actual topology families.
- Compare BarrVerb's bundled permuted ROM against Brombaugh's depipelining conventions and MAME's implementation.
- Locate surviving copies of Barr's complete FV-1 Informal Notes and all posts under the Spin forum account `Keith`.
- Analyze the downloadable Spin reverb programs and identify which are authored by Barr.
- Obtain primary evidence for Quadraverb, MIDIVerb III/IV, and Wedge program connectivity rather than relying on correspondence summaries.
- Measure real hardware impulse responses, frequency response, startup behavior, and program changes against BarrVerb.
- Separate topology from converter/filter/fixed-point contributions through controlled null tests.
