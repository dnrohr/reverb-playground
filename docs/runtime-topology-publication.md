# Runtime topology publication

M5.4 turns prepared graph compilation into a bounded asynchronous publication pipeline. The editable document remains a UI/control-thread value. The compiler worker owns validation, scheduling, control-plan construction, delay allocation, and runtime construction. The audio callback sees only prepared runtime envelopes.

## Lifecycle

1. `requestCompilation` assigns a monotonically increasing revision and moves the document into a one-entry newest-request-wins queue.
2. A dedicated worker takes the latest request and compiles it away from the audio thread.
3. If another edit has arrived before compilation finishes, the obsolete result is discarded on the worker and the newer request wins.
4. A successful result replaces the one prepared pending envelope. An older pending envelope is destroyed by the publishing thread, never by audio.
5. At the beginning of `process`, audio performs a bounded pending-to-active exchange. The selected runtime is fixed for the complete host block.
6. The previous active envelope enters a fixed 16-slot single-producer/single-consumer retirement ring.
7. The compiler worker drains and destroys retired runtimes. If the ring temporarily has no capacity, audio defers the pending swap and continues with the current active runtime.

The audio-side publication action is constant time: lock-free pointer loads/exchanges, scalar revision stores, and at most one fixed-ring pointer write. It never compiles, allocates, deletes, locks, waits, logs, or touches files/network. Windows x64 builds enforce always-lock-free pointer and index atomics at compile time.

## Bounded queue behavior

The request queue contains at most one document and the prepared side contains at most one pending runtime. A burst therefore cannot grow memory linearly with edit count. Replaced requests and prepared results increment `supersededRequests`; the last revision is never silently replaced by an older result.

The retirement ring cannot overflow into deletion on the audio thread. A full ring delays publication rather than weakening the real-time contract. The worker polls at a short bounded interval even when no new compilation request exists, so reclamation does not depend on another edit.

## Failure behavior

Compilation errors update the failed revision and diagnostic text but never touch the active or pending runtime. The last valid graph continues rendering. A later valid request can still become pending and active normally.

## Revision diagnostics

`TopologyPublicationSnapshot` exposes:

- latest requested revision;
- prepared pending revision, or zero;
- block-boundary active revision, or zero;
- most recent failed revision and diagnostic;
- superseded request/result count;
- completed compilation count;
- off-thread reclaimed runtime count.

Snapshot construction and failure-text copying are message/control-thread operations. The audio callback only updates lock-free scalar identities.

## Verification

Native tests explicitly observe pending, active, and failed states. They prove a failed compile leaves the preceding output audible. A rapid-edit stress test submits 1,000 changing graphs while another thread continuously processes audio; it requires a finite output, forward progress, newest-revision activation, request coalescing, bounded compilation count, and worker-side reclamation.

## Milestone boundary

M5.4 supplies the publication engine for the editable graph. [M5.5](topology-change-crossfades.md) wires editor documents into this host and adds the short old/new output crossfade that suppresses the remaining topology-switch click without retaining abandoned tails indefinitely. The standalone presentation did not change in M5.4, so that earlier task required no screenshot or video.
