# BarrVerb code review

Repository: <https://github.com/ErroneousBosh/BarrVerb>

Reviewed revision: `84d06af` (`master`, 2025-01-08). DPF submodule revision: `f5815166`.

## What it is

BarrVerb is a compact DPF audio plugin that interprets a prefetched/permuted copy of the original MIDIVerb program ROM. It does not implement a generic reverb topology in normal DSP objects. Instead, it emulates the tiny machine on which Keith Barr expressed 64 fixed programs.

The meaningful code is almost entirely in:

- `plugin/barrverb.cpp` - filters, plugin parameters, and interpreter;
- `plugin/barrverb.hpp` - DSP/plugin state;
- `plugin/rom.h` - program names and 64 x 128 16-bit instruction words;
- `plugin/DistrhoPluginInfo.h` - DPF format metadata.

## Signal path

```text
stereo input
    -> average to mono
    -> two cascaded low-pass SVF sections
    -> consume one sample for every two host samples
    -> execute 128 ROM operations on 16K-word state RAM
    -> capture R at step 0x60 and L at step 0x70
    -> duplicate each output sample to two host frames
    -> wet-only stereo output
```

The interpreter runs at host sample rate / 2. At 48 kHz this is 24 kHz, near but not equal to the hardware's 23.4375 kHz. At other host rates, all program delays and decay timing scale because there is no sample-rate conversion to a fixed emulated clock.

## Interpreter mapping

Each ROM word uses its high two bits as the operation and low 14 bits as the relative RAM address increment. The state is:

- `acc` - signed 16-bit accumulator;
- `ai` - current adder/data-bus value;
- `li` - next accumulator latch value;
- `ptr` - 14-bit circular RAM address;
- `ram[16384]` - delay/state memory.

The ROM has already been shifted to hide the hardware instruction pipeline, so `rom[program + step]` can be interpreted directly. After each instruction, `ptr` advances by the encoded offset modulo `0x4000`.

At step 0, BarrVerb writes the filtered ADC sample into the current RAM cell. At steps `0x60` and `0x70`, it emits `ai` to right and left respectively. As in the hardware, those three special steps suppress the normal accumulator latch update.

## What the implementation gets right

- The small state machine mirrors the hardware at a useful conceptual level.
- It preserves relative addressing and the 128-instruction fixed loop rather than translating presets into generic modern reverbs.
- It includes all named programs, including gated/reverse programs and Defeat.
- Program selection is automatable and DPF exposes the plugin in several formats.
- Mono-in/stereo-out and magic I/O steps match the reverse-engineered machine.
- The code is small enough to audit and modify experimentally.

## Deliberate approximations

The README explicitly says arithmetic was simplified. Brombaugh's reference emulator includes the sign-bit correction needed by the physical one's-complement/two's-complement behavior. BarrVerb uses ordinary signed expressions. This can change low bits, especially in inverted/subtractive instruction sequences.

Other documented approximations:

- 24 kHz engine at a 48 kHz host rather than 23.4375 kHz;
- crude decimation by taking every other filtered input sample;
- zero-order hold upsampling by duplicating every wet sample;
- approximate input filter;
- no output reconstruction filter.

The plugin also omits the hardware's analog wet/dry path and is wet-only.

## Correctness and safety findings

### High: odd host block sizes can write out of bounds

`run()` increments `i` by two and unconditionally reads/writes `i + 1`. An odd `frames` value makes the last iteration access one element beyond the block. A plugin must either retain a half-rate phase/sample across calls or handle the final frame separately.

### High: scratch buffer size assumes the constructor's buffer size never grows

`lowpass` is allocated with `getBufferSize()` once, but `run()` indexes it through `frames`. If the host can deliver a larger block later, this overruns the allocation. The safest design avoids the block scratch buffer entirely or resizes outside the real-time callback when DPF announces a buffer-size change.

### Medium: heap allocations have no matching destructor

`ram` and `lowpass` are created with `new[]`; the class declares no destructor that calls `delete[]`. This leaks per plugin instance. RAII containers or fixed storage would remove the issue.

### Medium: output saturation differs materially from the reference hardware model

BarrVerb clamps `ai` to `[-2047, 2047]` after every decoded operation. Brombaugh's emulator saturates only when capturing a DAC output, to `[-4096, 4095]`, while internal arithmetic and RAM retain 16-bit behavior. BarrVerb's clamp happens after `li` is computed, so it mainly changes the bus value seen by the DAC steps, but its range and placement still differ from the reverse-engineered design. This deserves an audio/null test rather than being grouped casually with the acknowledged one- or two-LSB arithmetic simplification.

### Medium: initialization and program changes do not fully model or deliberately manage settling

RAM is cleared only at construction. Loading another program changes the ROM offset without clearing or fading the old tank. Real hardware also retained state and could produce a burst because it had no hard reset/mute, so this behavior may be historically defensible. In a plugin it should be an explicit choice, with optional authentic and safe/faded modes.

### Medium: sample-rate-dependent timing and filter coefficients

The half-rate engine means a 44.1 kHz host runs the emulation at 22.05 kHz and a 96 kHz host at 48 kHz. Reverb times, allpass lengths in seconds, modulation assumptions, bandwidth, and pitch-like resonances all change. The filters are configured only in the constructor. A fixed 23.4375 kHz internal engine with proper resampling would make presets stable.

### Low/medium: no explicit strategy for host block discontinuities

Half-rate processing restarts each block at local index zero. For even blocks this preserves the alternating phase, but odd blocks would require phase state even after the out-of-bounds bug is fixed. A robust implementation stores decimator/interpolator phase and the held output across calls.

### Low: `activate()` does not reset state

Hosts may expect activation after transport or suspension to begin deterministically. Whether to clear is a product decision, but the current empty `activate()` should be documented because old tail state persists.

### Low: allocation failure and realtime ownership

Allocation occurs in the constructor, not the audio callback, which is good. Using `std::vector<int16_t>`/`std::vector<float>` or `std::unique_ptr<T[]>` would make ownership exception-safe and obvious.

## Differences from Brombaugh's reference emulator worth testing

| Area | MIDIVerb_RE emulator | BarrVerb |
|---|---|---|
| Program range | 0-62; invalid values mute | 64 named slots including Defeat |
| Input | summed integer stereo, scaled into hardware-like bus | floating stereo average, filtered, scaled by 2048 |
| Engine rate | caller supplies one sample per exact emulated pass | every second host frame |
| Sign correction | explicit `+(ai < 0)` | ordinary signed shifts/arithmetic |
| Inverted write | bitwise complement in reference model | arithmetic negation |
| DAC saturation | `[-4096, 4095]` at capture | `[-2047, 2047]` clamp on `ai` each step |
| Resampling/filtering | external/test harness dependent | two input SVFs, sample drop, duplicated output |

Not every difference is necessarily audible or wrong. The table identifies where equivalence should be demonstrated rather than assumed.

## Recommended next engineering work

1. Add automated interpreter test vectors from `MIDIVerb_RE/vec_midiverb.c`.
2. Fix odd-block and buffer-lifetime hazards before feature work.
3. Add exact and simplified arithmetic modes, then quantify their null residual.
4. Implement a fixed 23.4375 kHz internal clock with quality resampling.
5. Model input and output analog filters from the supplied SPICE networks.
6. Add wet/dry and output level while retaining a strict hardware mode.
7. Create impulse-response tests for representative short, long, gated, reverse, and Defeat programs.
8. Build a ROM disassembler/graph exporter so program topology can be compared visually.

## Repository/build notes

- The repository is ISC licensed, but `plugin/rom.h` is acknowledged by the author as a transformed copyrighted Alesis/Keith Barr ROM image. Redistribution implications should be reviewed before shipping a derived product.
- DPF is a git submodule and has been initialized in the local clone.
- The repository history is short and largely concentrated in July-August 2024, with build/metadata fixes through January 2025.
- No test suite is present.
- Pluginval was disabled in the build workflow “until further investigation,” so format/host validation should be revisited.
