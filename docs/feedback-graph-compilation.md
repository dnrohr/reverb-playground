# Feedback graph compilation

M3.4 extends the prepared graph runtime to legal feedback networks containing explicit Delay state. It retains the M3.3 node contracts, resource limits, deterministic ID ordering, fixed-capacity preparation, and last-valid publication behavior.

## Strongly connected components and legality

The compiler first finds strongly connected components in the complete directed graph. Cyclic components are reported in deterministic node-ID order. It then builds the current-sample dependency graph by cutting every edge entering a Delay node: a Delay's output for sample `n` comes from stored state, so its current input is not needed to calculate that output.

If the remaining dependency graph is acyclic, every original loop is causal. If a cycle remains, it is a zero-delay algebraic loop. Compilation fails and returns a concrete deterministic path such as:

```text
zero-delay algebraic loop: gain-a -> gain-b -> gain-a
```

Merely sharing an SCC with a Delay is insufficient. A nested Gain/Sum sub-loop that bypasses every Delay remains in the current-sample graph and is rejected. Allpass has state but also immediate current-input feed-through, so it does not break an algebraic dependency; a feedback path must cross an explicit Delay.

## Per-sample execution order

For each sample, the prepared runtime performs four bounded phases:

1. Publish host inputs and read every Delay output without advancing state.
2. Evaluate current-sample dependencies in the deterministic topological schedule.
3. Write each completed Delay input and advance each Delay exactly once.
4. Copy the two explicit output-port values to the host output.

This rule is invariant to host block partitioning. A one-sample Delay in a gain-`0.5` loop excited by an impulse produces `0, 0.5, 0.25, 0.125, ...` whether rendered as one block or several.

## Publication and budgets

Feedback compilation and Delay allocation occur on the control thread. Processing uses the same atomic raw-pointer publication protocol as the acyclic runtime; an invalid edit does not replace or reset the active graph.

The automated compiler budget fixture uses 256 nodes, 64 independent feedback loops, and a 1,024-sample prepared block. Its contract is compilation below one second and prepared routing plus short-delay storage below 8 MiB on CI. The separate [delay-memory plan](delay-memory-planning.md) now reports exact requested/allocated totals and rejects graphs above the 64 MiB project budget before publication.

Tests cover direct recurrence, block-partition invariance, exact algebraic-loop paths, nested and independent components, deterministic rendering, invalid-publication continuity, and the maximum-size budget fixture.

UI unchanged; no screenshot or video was required.
