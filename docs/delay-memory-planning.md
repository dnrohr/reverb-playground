# Delay-memory planning

M3.5 gives every compiled patch one explicit delay-memory plan. Planning happens after graph and node-contract validation but before any runtime or delay storage is allocated. Both `compileAcyclicGraph` and `compileFeedbackGraph` return the plan, including on rejection, and publication returns the same inspection data.

## Project budget

The MVP budget is **64 MiB (67,108,864 bytes) per prepared graph** for Delay and Allpass sample storage. This is a project limit, not an estimate of total plugin memory. Signal-routing buffers are reported separately by `PreparedAcyclicRuntime::preparedStorageBytes()`.

Each plan reports:

- delay-bearing line count;
- requested samples and bytes;
- allocated samples and bytes;
- the project budget and whether the allocation fits.

Requested memory is the logical integer delay after converting milliseconds at the target sample rate. Allocated memory is the actual arena capacity. They are equal for Delay. Allpass allocation can be larger because continuous delay editing needs a fixed read-tap range: it reserves at least 100 milliseconds, or its declared delay when that is longer, plus the guard sample used by its circular buffer.

## Boundaries and conversion

Saved and edited values remain milliseconds. At compilation:

- zero and negative delays are rejected;
- positive values round to the nearest integer sample with a one-sample minimum;
- exactly 10,000 milliseconds is accepted;
- values above 10,000 milliseconds are rejected;
- sample rates must be positive and no higher than 192 kHz;
- no more than 64 delay-bearing nodes may be compiled.

The compiler recalculates the complete plan whenever the sample rate changes. A graph can therefore fit at one rate and exceed the budget at another.

## Arena ownership and publication safety

After a plan passes the budget check, the prepared runtime allocates one contiguous `float` arena with exactly the planned sample count. Delay and Allpass processors receive non-owning slices during control-thread preparation. Their process, reset, split-phase feedback read/write, tap crossfade, and parameter smoothing paths never resize or allocate storage.

An over-budget graph has no runtime object and cannot be published. `AcyclicRuntimeHost` returns its exact requested/allocated totals and error while retaining the preceding runtime and its delay state. The same rule applies when a host sample-rate change makes an otherwise unchanged patch too large.

## Verification

Native tests cover no-delay, sub-sample minimum, exact maximum, over-limit, Allpass requested-versus-allocated, and aggregate over-budget cases. A sample-rate publication test prepares the same nine-delay patch successfully at 44.1 kHz, rejects its 192 kHz plan, and proves the 44.1 kHz runtime remains active. Repeated processing also verifies that the arena and total prepared-storage accounting stay fixed.

The editable UI is not yet connected to the general native graph compiler, so M3.5 exposes inspection through the compiler/publication contract without adding a misleading UI estimate. The later runtime/editor binding should render these exact returned totals rather than recomputing memory in TypeScript.
