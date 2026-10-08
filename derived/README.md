# `derived/` — files produced by this project, not by Norsk Data

Nothing here came off the original tape. These are extractions and build
variants created while analysing the archive. They are inputs (hand-written
or machine-extracted), which is why they are kept under version control
rather than in the disposable `../Build/` folder.

| file | what it is |
|---|---|
| `DRUM-DRIVER.SYMB` | `XDRUM` + `TRSFR` lifted **verbatim** out of `TSS1.SYMB` (lines 3695–3877) so the swapping-drum driver can be read on its own |
| `ASSYSA-MAC-INPUT.SYMB` | `ASSYSA` reduced to a pure MAC command stream |
| `ASSYSB-MAC-INPUT.SYMB` | the same for version B (`DEBUG`) |
| `ASSYS-DRUM-N10-MAC-INPUT.SYMB` | **experimental** NORD-10 + drum build — compiles `XDRUM` in |

## `DRUM-DRIVER.SYMB`

The deepest layer of TSS: the NORD-10 swapping drum, written by
**Nils Jakob Langeland, 17 April 1973**, and described in its own header as
a *"1. approximation to a NORD-10 version"*.

- Device `IOX 540`; registers at +0 read core address, +1 load core address,
  +3 load block address, +4 read status, +5 load control, +7 word count.
- Function codes: 0 read, 1 write, 2 read-test, 3 compare, 20 read-status.
- Three exits — error / busy / finished — with the busy exit designed to be
  re-called, so it drives interrupt-driven transfers.
- 32 sectors per track, with multi-block transfers split at track
  boundaries; word count = blocks × 64.
- `TRSFR` is the swap front end: pages below 4 × `DRMSZ` go to the drum,
  everything above spills to the CDC disk. Without the `DRUM` mark
  `TRSFR = TRXX`, i.e. disk-only.

It needs `DRMSZ`, `DKA1`, the `DKTR` CDC disk driver, and the `CDC`, `DRUM`
and `N10` library marks. **Both archived builds compile it out** — neither
sets `DRUM` or `N10`.

## The `*-MAC-INPUT.SYMB` variants

The archived `ASSYSA`/`ASSYSB` begin with three lines that are *not* MAC
input — `FMAC`, `100`, `SYSA` — which start the assembler and answer its
prompts, and end with operator lines (`*:`, `?`, `)9TSS`, `@cc`). These
copies strip that wrapper, quote the output file names so MAC creates them,
and end with `)9EXIT`, so they can be pasted straight into a MAC session
under SINTRAN.

`ASSYS-DRUM-N10-MAC-INPUT.SYMB` additionally sets the `DRUM` and `N10`
marks and drops the `INTDS`/`INTEN` pre-definitions, because under `N10`
the source defines those itself as `IOF`/`ION`. Since 2026-10-08 it also sets
the `CLKFX` mark, which assembles `TBANG`'s clock constants as 48-bit floats
(`../docs/TSS-FLOAT-FORMAT.md` §8); the golden inputs must not set it. It is
**untested against
hardware** — the drum driver is a first approximation, and the Diablo probe
in the `LEV6` scanner assembles NORD-1 `IOT` opcodes even in an N10 build.

`../docs/TSS-BRINGUP.md` documents both build routes in full.

> Note: `../mac-c/scripts/build/build_tss_assysa.sh` does **not** use these files — it
> feeds the real `../src/ASSYSA.SYMB` to the C assembler and strips the
> wrapper itself. These exist for running under a genuine MAC.
