# Barr reference implementation

The development reference is a deterministic Barr-inspired signal path assembled entirely from the public mono primitives. Its immediate purpose is to establish the product's stereo channel plan, stable graph identity, and render/test seam before the editor or general compiler exists.

```text
Input L --\
           Sum * 0.5 -> Low-pass -> AP 4.31 -> AP 7.13 -> AP 13.73 -> AP 19.91 --+-> AP 29.71 -> Output L
Input R --/                                                                        +-> AP 37.11 -> Output R
```

All times are milliseconds. The first four allpasses provide temporal diffusion in a shared mono path. The two differently sized terminal allpasses expose decorrelated wet channels through explicit mono connections. The serialized graph uses stable descriptive node and connection IDs and keeps left/right ports explicit.

## Deliberate departures from original MIDIVerb arithmetic

This is a development reference, not yet a cycle-accurate MIDIVerb-I preset:

- It uses floating-point Schroeder allpasses instead of Barr's 16-bit four-operation microcoded machine.
- The delay values are representative fixed design values, not decoded from proprietary program ROM.
- It currently has a feed-forward shared diffusion chain rather than a program-specific large feedback loop; internal allpass delays provide state, but there is no outer recirculating tank yet.
- It uses a conventional one-pole 7 kHz input filter rather than the hardware's exact converter, fixed-point, and filtering behavior.
- It runs at the host sample rate rather than the MIDIVerb's approximately 23.4 kHz internal rate.

These differences are intentional and test-visible. M1.2 establishes the architectural seam. Later reference refinement must use legally redistributable measurements or user-supplied ROM-derived data and document every changed node/parameter. The existing BarrVerb review explains why its bundled arithmetic should not silently become the new product's canonical truth.

## Runtime contract

`BarrReference::prepare` configures every public primitive from the same immutable runtime descriptors that generate the visible graph. `process` performs no allocation and uses caller-owned mono input/output spans. Stereo inputs are summed and normalized at `0.5` gain as the explicit **Mono Sum** block's documented behavior; output spans are always written or zero-filled. `reset` clears all filter and allpass state.

The audition impulse is injected at the normalized mono-sum boundary as a test stimulus. It is not a hidden processor applied to live input. Master audition gain and the two numerical safety guards operate after the reference patch and are exposed in the native audition strip; they are also listed in the runtime snapshot's `outsidePatch` metadata.
