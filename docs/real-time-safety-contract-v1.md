# Real-time and safety contract v1

Status: accepted for engine version 0.1. This contract is versioned independently from the patch schema. A behavior change that weakens or changes an observable guarantee requires a new contract version and migration note.

## Graph legality

A directed feedback cycle is legal only when every route around that cycle passes through an explicit stateful `delay` node. Removing all delay nodes and their incident edges must leave an acyclic graph. Self-connections and multi-node algebraic loops without a delay are therefore illegal, even if gain or filtering might make them numerically stable.

Delay time is expressed in milliseconds. The compiler converts it to at least one sample for a node participating in feedback; zero samples never satisfies the cycle rule. Validation happens before compilation and before a graph can be published to the audio thread.

## Audio-thread rules

The processing callback may perform only bounded computation over already-owned memory. It must not:

- allocate, free, resize a container, or acquire ownership that may destroy on the callback;
- wait on a mutex, condition variable, future, thread, process, or device;
- access a filesystem, network, logger, dialog, clipboard, or operating-system service;
- parse JSON, compile a graph, discover topology, or perform work whose bound depends on user interaction;
- throw an exception.

DSP state, scratch buffers, schedules, and parameter lanes are prepared off-thread to fixed capacities. The initial limits are 256 nodes, 512 connections, 64 delay lines, 10 seconds per delay at 192 kHz, 64 control updates per audio block, and the host's prepared maximum block size. A document outside those limits is rejected with a diagnostic; it is not partially compiled.

## Numerical safety

Every externally audible block passes through a constant-space, allocation-free output guard. The default runaway threshold is absolute sample value `16.0` (about +24 dBFS). The first NaN, positive or negative infinity, or sample above that threshold:

1. zeros the complete output block, including samples before the violation;
2. latches emergency mute for subsequent blocks;
3. records a bounded violation code and sample index for off-thread observation.

Muted processing emits silence. Recovery is explicit: the control thread prepares or resets the DSP state and then clears the latch. It never resumes automatically into unknown feedback state. Denormals are handled with the platform's real-time-safe flush-to-zero facility and are not treated as an emergency.

The current `NumericalSafetyGuard` makes non-finite and runaway behavior directly testable. Later graph runtimes must place the guard after wet/dry output assembly so no public output bypasses it.

## Parameters versus topology

Parameter changes and topology changes use different publication paths.

- A parameter change writes a bounded, lock-free command to an existing compiled graph. Audible continuous parameters use per-sample or documented per-block smoothing; milliseconds remain the display and serialization unit. Discrete parameters change at a block boundary without interpolation.
- A topology change is validated and compiled entirely off-thread into an immutable runtime snapshot. The audio thread adopts a ready snapshot only at a block boundary through an atomic publication mechanism. Reclamation occurs off-thread after the previous snapshot is no longer observable.

The initial topology transition emits one safety-muted block. A later click-safe crossfade may replace that policy only when both graphs and all temporary memory are prepared off-thread and CPU cost remains bounded. Parameter smoothing must not be used to disguise a topology swap.

The fixed Barr runtime implements the first parameter path as 14 always-lock-free atomic value lanes. Each block performs a fixed 14 loads. Gain and filter/coefficient targets smooth over 20 milliseconds; allpass delay targets crossfade preallocated read taps over 20 milliseconds. The detailed policy and editor transaction semantics are documented in [Continuous parameter editing](continuous-parameter-editing.md).

## Verification obligations

- Graph tests cover a rejected delay-free cycle and an accepted feedback loop containing `delay`.
- Numerical tests inject NaN and excessive finite level, observe the violation, verify immediate full-block silence, verify the mute latch, and verify explicit reset.
- Each processing primitive documents and tests that preparation owns allocation while `process` is `noexcept` and allocation-free.
- Stress tests added with the graph runtime exercise supported sample rates, maximum block size, silence, impulses, sustained full-scale input, and long feedback renders.

This document is the normative foundation contract. Implementation-specific thresholds may become patch-independent engine settings, but may not exceed a safe finite range or disable non-finite detection.
