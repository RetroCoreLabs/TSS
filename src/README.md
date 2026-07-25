# `src/` — the corrected MAC source that assembles cleanly

The TSS source in the form that assembles **with zero errors**. This is the
working source the build actually uses, and the version the golden dumps in
`../reference/` are scored against.

## Why this folder exists (it is *not* just a copy of `archive/`)

`src/` is derived from `../archive/TSS*.SYMB` (CVS's patched 1978 source) in
**two** steps — and the second step is the whole reason the folder exists:

1. **Parity bit cleared** — bit 7 stripped from every byte. Historically this
   was required just to feed the files to an assembler. As of 2026-07-25 mac-c
   strips parity on the fly (`mac_assemble_file`) and can assemble the raw
   `../archive/` originals directly, so this step *alone* no longer justifies a
   separate copy.

2. **The `STR`→`XTR` rename COMPLETED.** `STR`/`LSS` collide with MAC's
   permanent symbol table, so CVS renamed them — but left the job unfinished:
   `STRX`, `STR0`–`STR2`, `STR1X`, `STR2X` are still referenced in *value*
   contexts (`SAT STRX`, `STSET PTR,XTR,STRX,…`) with no definition. `src/`
   finishes the rename (`STRX`→`XTRX`, `STR0`→`XTR0`, …; ~137 lines across the
   five parts) and adds the `EXRGP`/`CLD` fixes.

**The payoff:** `src/` assembles with **0 errors**, whereas the raw `archive/`
originals assemble with **6 undefined-operand errors** (`STR0 STR1 STR2 STR1X
STR2X STRX` — the incomplete-CVS-rename casualties). Both builds produce the
**byte-identical** golden symbol dump (679 A / 675 B — those six are undefined
in the 1978 reference too), but `src/` is the clean, warning-free source.

So this is a **genuinely distinct artifact, not a duplicate.** Do **not**
regenerate it from `archive/` with a plain parity strip — that would throw away
the completed rename and reintroduce the 6 errors. The pristine 1973 *pre-CVS*
original is `../archive/TSS*.ORG`; every difference is catalogued in
`../archive/README.md`.

## Contents

| file | lines | contents |
|---|---|---|
| `TSS1.SYMB` | 4,709 | kernel — interrupts, all device drivers, scheduler, swapper, page tables |
| `TSS2.SYMB` | 3,512 | context block, monitor-call implementations, command processor |
| `TSS3.SYMB` | 1,755 | overlay machinery (`GOVER`/`OVERL`/`OVERX`) and system utilities |
| `TSS4.SYMB` | 2,040 | utility overlays — DUMP, RECV and friends |
| `TSS5.SYMB` | 1,995 | user-management overlays — create/delete user, accounts |
| `ASSYSA.SYMB` | 25 | build script, version A |
| `ASSYSB.SYMB` | 25 | build script, version B (adds the `DEBUG` mark) |
| `MINIT.SYMB` | 810 | standalone mass-storage initialisation program |
| `TDUMP.SYMB` | 383 | dumper — punches a bootable TSS paper tape |

≈14,000 lines of MAC assembler in total.

## Assembling it

```bash
cd ../mac-c
./scripts/build/build_tss_assysa.sh     # runs ASSYSA and ASSYSB; output -> ../Build/
```

The build scripts are MAC command streams, not shell scripts. `ASSYSA`
sets the library marks (`CDC MACF DIAB K14 TEL4`), defines the symbols MAC
lacks, then assembles the five parts with `)9ASSM` and dumps the symbol
table with `)LIST`.

## Reading it

Notation that trips people up:

- `%` starts a comment; `;` separates statements on one line.
- Numbers are **octal** by default.
- `"MARK` opens a conditional region, a bare `"` closes it. A mark is true
  when the symbol is *referenced but never defined* — which is why bare
  symbol lines like `CDC` appear in the build script.
- `'TEXT'` is a string, two characters per word; `#ab` is one word holding
  two characters; `(EXPR` is a literal; `[3.2E2` is a floating constant.
- `*` is the current location counter, `*N` means `*+N`, and `EXPR/` sets
  the counter.
- Names are significant to **five characters, counting from the end**
  (`TIMOUT` and `IMOUT` are the same symbol).

`../docs/PROJECT-DESCRIPTION.md` explains the file layout and the configuration
marks; `../ppt/Intro to TSS.pdf` is a guided tour of the architecture.
