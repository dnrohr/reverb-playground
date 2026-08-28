# Factory patch catalog and compatibility

Factory patches are versioned product assets, not informal examples. The
authoritative shipped set is
[`factory-patches/catalog.json`](../factory-patches/catalog.json). Catalog
version 1 contains exactly nine complete families:

| Factory ID | Family | Document source | Schema | Engine |
|---|---|---|---:|---:|
| `barr-reference` | Barr reference | Native runtime graph | 2 | 0.1 |
| `causal-reverse-envelope` | Reverse-style | Checked-in generated JSON | 2 | 0.1 |
| `level-gated-room` | Gated | Checked-in generated JSON | 2 | 0.1 |
| `modulated-cosmic-reverse` | Modulated reverse-style | Checked-in generated JSON | 2 | 0.1 |
| `gravity-diffusion` | Gravity Diffusion | Native-builder generated JSON | 2 | 0.1 |
| `dense-figure-eight` | Dense figure-eight | Native-builder generated JSON | 2 | 0.1 |
| `safe-parallel-shimmer` | Parallel shimmer | Native-builder generated JSON | 2 | 0.1 |
| `split-feedback-shimmer` | Feedback shimmer | Native-builder generated JSON | 2 | 0.1 |
| `reverse-cosmic-shimmer` | Reverse cosmic shimmer | Native-builder generated JSON | 2 | 0.1 |

The Barr graph is generated from the same native definitions that execute it,
so it cannot drift from a duplicated JSON asset. Gravity Diffusion, Safe
Parallel Shimmer, Split-Feedback Shimmer, and Reverse Cosmic Shimmer are exported from their native
graph builders and admitted by exact SHA-256. Dense Figure Eight follows the
same native-builder and exact-hash path. The reverse/gated graphs and catalog are deterministic output
from `scripts/generate_factory_patches.mjs`; `--check` compares every byte in CI.

Each catalog entry declares `status: complete`, its family, document kind/path,
schema and engine versions, SPDX license expression and license path, plus a
provenance kind, source path, and description. Tests require every referenced
path to exist. All nine are project-authored AGPL-3.0-only work and contain no
ROM-derived data, imported presets, or captured impulse response.

## Admission rule

A factory family may enter the catalog only when all of the following are true:

1. Its topology uses public visible primitives and has exactly one stereo input
   and output boundary.
2. Its document validates and compiles through the same path as a user patch.
3. A deterministic impulse render is finite and bounded, and bounded-noise
   stress is finite at 44.1, 48, and 96 kHz.
4. Serialize/parse/serialize produces a stable current-schema document.
5. Audible family claims have deterministic measurement fixtures.
6. Schema/engine compatibility, license, and provenance are declared.

Short-room/baseline designs remain outside the catalog because their complete
product topologies and acceptance fixtures have not been approved. Absence is
deliberate: the roadmap does not permit a family name to stand in for unfinished
behavior.

## Schema compatibility

The released readable range is schema v1 through v2. Writers always emit v2.

| Source | Reader behavior | Compatibility evidence |
|---|---|---|
| v1 | Adds the current explicit modulation socket/mapping for legacy modulatable parameters, preserves values/layout/connections, then writes v2 | Shared native and browser migration fixture |
| v2 | Reads current explicit modulation data and writes byte-stable v2 | Every factory and persistence round-trip test |
| Future/unknown | Rejects before replacing the current graph | Native/browser rejection tests |

The v1 fixture specifically proves a legacy Gain value of `0.375`, layout and
viewport survive while `gain-mod` receives the defined bipolar `0.5` amount and
`-1..+1` clamp. The migrated document reparses identically and its second write
is byte-for-byte stable. Schema files remain at
[`schemas/patch-v1.schema.json`](../schemas/patch-v1.schema.json) and
[`schemas/patch-v2.schema.json`](../schemas/patch-v2.schema.json).

## CI contract

Native tests derive their patch loop from the catalog and require every entry
to load, graph-validate, compile, render finite audio, serialize at its declared
versions, and round-trip. The same loop applies bounded stereo noise at each
supported sample rate. Browser tests independently require the catalog IDs to
match the factory menu, validate every metadata field, parse every checked-in
document through the editor contract, and exercise v1-to-v2 migration.

Safe Parallel Shimmer adds saved descriptive names to ordinary blocks; names
remain optional schema-v2 presentation metadata and never change DSP identity.
Split-Feedback Shimmer uses the same metadata rule and adds no private runtime
type: its two returns, filters, pitch shift, recombination, and shared tank are
ordinary public graph blocks.
Reverse Cosmic Shimmer likewise adds no private processor: its paired Pitch
Shift phases and reverse-grain directions are ordinary saved schema-v2
parameters, and its 45-block graph restores exactly through editor and host
state.
