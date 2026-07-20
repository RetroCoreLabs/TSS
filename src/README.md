# `src/` — the MAC source that assembles

The TSS source, parity bit cleared, ready to feed to an assembler. This is
the **single** clean copy in the repository; nothing here is duplicated
elsewhere.

Derived from `../archive/TSS*.SYMB` by clearing bit 7 — i.e. this is the
**CVS-patched** version (`XTR`/`XSS`), not the 1973 original. That is
deliberate: it is the version that assembles without colliding with MAC's
built-in symbols, and the version the golden dumps in `../reference/` were
produced from, so the oracle comparison stays valid. The pristine 1973
source is `../archive/TSS*.ORG`, with the differences catalogued in
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
./build_tss_assysa.sh     # runs ASSYSA and ASSYSB; output -> ../Build/
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

`../docs/TSS-Analysis.md` explains the file layout and the configuration
marks; `../ppt/Intro to TSS.pdf` is a guided tour of the architecture.
