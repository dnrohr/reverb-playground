# Continuous parameter editing

The M2.3 reference editor sends every slider input event to the already-prepared native Barr runtime. The value is audible while the pointer is still down; pointer release only closes the undo transaction. One drag is therefore one history entry, regardless of how many intermediate values it produced.

## Editable parameters

The fixed reference exposes its input-sum gain, input-filter cutoff, and the delay time and coefficient of each of its six allpasses. The native runtime descriptor owns every default, range, step, and unit. The runtime snapshot supplies that metadata to the inspector, so the UI does not keep a second parameter table.

## Control-to-audio transfer

The web view calls the native `setRuntimeParameter` bridge with stable node and parameter IDs. The UI/native callback resolves that identity off the audio thread and writes one of 14 fixed `std::atomic<double>` lanes. These atomics are required at compile time to be always lock-free.

At the start of each audio block, the live harness performs exactly 14 atomic loads and applies the targets to the existing primitives. No command queue, allocation, lock, JSON parsing, or graph compilation occurs in `process`. Topology is unchanged.

## Transition policy

- Gain ramps linearly to its target over 20 milliseconds.
- Low-pass coefficients and allpass coefficients exponentially approach their target over 20 milliseconds.
- An allpass delay edit crossfades between the old and new read taps over 20 milliseconds. Its ring buffer is allocated once for the declared 100-millisecond maximum; editing does not resize it.

The delay policy is deliberately clean rather than tape-like: it avoids uncontrolled discontinuities and pitch sweeps while inspecting an architecture. A selectable tape-style mode remains possible later.

## Undo and redo

Pointer-down captures the exact starting value, intermediate events audition continuously, and pointer-up records the exact final value. Undo writes the captured start value through the same native path; redo writes the captured final value. A new edit clears the redo branch. `Ctrl/Cmd+Z`, `Ctrl/Cmd+Shift+Z`, `Ctrl/Cmd+Y`, and the inspector buttons share this history.

The history is currently editor-session state. Patch persistence arrives in M2.4.

## Verification

Native tests check the defined gain/filter smoothing signal, finite bounded output through a large allpass delay edit, and lock-free harness-to-DSP parameter application on the next audio block. Web tests check exact-value history, no-op handling, and redo invalidation. UI evidence under `artifacts/ui/m2-3-continuous-parameter-editing` demonstrates intermediate live values and undo/redo; the audio-side test is the deterministic repeated-signal evidence.
