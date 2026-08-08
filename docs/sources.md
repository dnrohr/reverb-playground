# Source map

Last researched: 2026-08-08.

## Tier 1: direct Barr material and primary technical artifacts

### Sean Costello, “RIP Keith Barr”

<https://valhalladsp.com/2010/08/25/rip-keith-barr/>

The most important narrative source. It reproduces Barr's account of designing the original MIDIVerb and a second note about his preferred later “2AP, delay” loop topology. Key claims include relative DRAM addressing, four operations, 128 operations per sample, coefficient-0.5 allpasses, the later gate-array engine, controller-generated LFO updates, and multiple injection/sparse output tapping.

### Keith Barr posts on the Spin Semiconductor forum

- Allpass-loop history: <https://www.spinsemi.com/forum/viewtopic.php?t=3>
- Room/plate/hall discussion: <https://www.spinsemi.com/forum/viewtopic.php?t=92>
- Spin effects/allpass notes: <https://spinsemi.com/knowledge_base/effects.html>

Barr explains his progression from Schroeder/MXR structures through allpasses embedded in comb loops to a single large loop. He discusses spectral peaking from feedback around tapped sums, the psychoacoustic basis of good compact reverbs, all-allpass designs, delay “space,” and sound-based rather than physically literal algorithm names.

The forum is intermittently slow and some old knowledge-base links have moved. Search-indexed copies were cross-checked, but a future archival pass should capture every post by the `Keith` account.

### MIDIVerb_RE

- Upstream: <https://github.com/emeb/MIDIVerb_RE>
- Local: `../research/sources/MIDIVerb_RE/`

Eric Brombaugh and Paul Schreiber's reverse engineering is the strongest hardware source. It includes full schematics, a 20-page slide deck, C emulator/disassembler/compiler, Verilog model, and SPICE models of analog filters. The ROM itself is intentionally absent.

Particularly important local artifacts:

- `docs/MV_Slides.pdf` - top-level system, timing, opcodes, address calculation, example gain/allpass/lowpass sequences, typical program graph.
- `schematics/MIDIVerb_Schematic.pdf` - traced digital and analog hardware.
- `code/emulator/midiverb.c` - concise reference execution semantics.
- `verilog/mvop.v` - gate/timing-oriented model of the discrete DSP.

### BarrVerb

- Upstream: <https://github.com/ErroneousBosh/BarrVerb>
- Local: `../BarrVerb/`

Gordon Pearce's DPF interpreter and transformed ROM. Its README documents intended compromises and credits Brombaugh, Schreiber, and `thement`.

### Spin FV-1 documentation

- FV-1 data sheet: <https://spinsemi.com/Products/datasheets/spn1001/FV-1.pdf>
- SPINAsm manual: <https://www.spinsemi.com/Products/datasheets/spn1001-dev/SPINAsmUserManual.pdf>
- Effects notes: <https://spinsemi.com/knowledge_base/effects.html>
- Guitar amp demo notes credited to Barr: <https://www.spinsemi.com/app_download/GA_DEMO_notes.pdf>

These document the later processor Barr designed and the unusually efficient delay/allpass instruction vocabulary that supported his reverb practice.

## Tier 2: technically informed interpretation

### Sean Costello on Alesis architectures, KVR

<https://www.kvraudio.com/forum/viewtopic.php?p=5583525>

Costello summarizes correspondence with Barr: Quadraverb's four sections, later conversion into a single interconnected loop, confirmed single-loop use by MIDIVerb IV/Wedge, and extensive preset-to-preset topology variation. He also discusses fixed-point character and long chains of coefficient-0.5-to-0.618 allpasses in Bloom-like effects.

### ValhallaShimmer Bloom note

<https://valhalladsp.wordpress.com/tag/keith-barr/>

Connects MIDIVerb II Bloom to a deliberately slow density attack from series allpasses and quotes the original manual's description. Useful for perceptual interpretation, but its proposed reconstruction settings are Costello's emulation, not original Alesis code.

### AES Pacific Northwest meeting recap

<https://www.aes-media.org/sections/pnw/pnwrecaps/2015/rlang_oct2015/>

Panel discussion with Costello and experienced hardware designers. Useful observations: hardware constraints determine viable algorithms; the first 50 ms shape spatial impression; limited bandwidth can benefit reverb; MIDIVerb II Bloom's slow rise came from series allpasses; noise/bit depth became part of the sound.

### Gearspace “Reverb Subculture” discussion

<https://gearspace.com/board/geekzone/380233-reverb-subculture-31.html>

Contains program-level analysis of MIDIVerb/Bloom arithmetic and topology by technically engaged users. Treat claims as leads unless confirmed against a ROM disassembly.

### Synth-DIY and DIYStompboxes threads

- <https://synth-diy.org/pipermail/synth-diy/2016-October/153727.html>
- <https://www.diystompboxes.com/smfforum/index.php?topic=133471.0>

These establish the reverse-engineering project's provenance and point to people with direct Alesis or hardware-analysis experience. They are less complete than the published `MIDIVerb_RE` archive.

## Tier 3: historical/product context

- Sound on Sound interview with Barr: <https://www.soundonsound.com/people/alesis-adat-future-technology>
- MIDIVerb IV product page: <https://www.alesis.com/products/view/midiverb4.html>
- MIDIVerb IV service manual: <https://www.vintagesynthparts.com/wp-content/uploads/2018/01/almid4sm.pdf>
- CDM remembrance and links: <https://cdm.link/remembering-keith-barr-founder-of-alesis-lost-last-week/>

These help establish product history and hardware context but are not enough on their own to reconstruct algorithms.

## Related implementations to inspect next

- `thement/midiverb_emulator`: <https://github.com/thement/midiverb_emulator>
- MAME Alesis driver: <https://github.com/mamedev/mame/tree/master/src/mame/alesis>
- Brombaugh's Eurorack implementation: <https://github.com/emeb/g081_audio>
- Recent decompiled/embeddable work discussed here: <https://www.reddit.com/r/synthdiy/comments/1qeveed/midiverb_algorithms_for_diy_use/>

## Source cautions

- “Keith Barr architecture” is often used online as if it names one fixed block diagram. The direct record contradicts that simplification.
- Costello carefully labels some chronology as a guess based on correspondence and examples. These notes preserve that uncertainty.
- Forum diagrams often use “parallel loops” for a network whose sections are serially interconnected around a ring. Trace feedback edges, not page layout.
- A transformed ROM or decompilation may carry copyright obligations separate from an emulator's source-code license.
- Product manuals describe sound and controls, not necessarily internal connectivity.
