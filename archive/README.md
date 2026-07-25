# `archive/` — the original files, untouched

**Do not edit anything in this folder.** These are the files as they came off
the tape: 8-bit bytes with the ND **parity bit still set** in bit 7. Every
other folder in the repository is derived from these.

## Two versions of the same program

The archive contains the TSS source **twice**, and they are *not* the same
text — this is the single most important thing to know about this folder:

| files | what they are |
|---|---|
| `TSS1.ORG` … `TSS5.ORG` | the **original 1973 source** as Bo Lewendal wrote it |
| `TSS1.SYMB` … `TSS5.SYMB` | the same program **after "CVS" patched it** for reassembly |

### What CVS changed — 153 lines, all symbol renames

`STR` and `LSS` are **built-in symbols in MAC's permanent table**
(`STR` = 030000 = `STF`, `LSS` = 002400), so TSS's own labels of those names
collided with the assembler and had to be renamed. `ASSYSA` says so directly:

```
% STR and LSS are defined in the MAC/FMAC symbol table
% CVS:  STR has been substituted by XTR
% CVS:  LSS has been substituted by XSS
```

Measured differences (whitespace ignored, parity stripped):

| file | changed lines |
|---|---|
| TSS1 | 0 (whitespace only) |
| TSS2 | 16 |
| TSS3 | 43 |
| TSS4 | 76 |
| TSS5 | 18 |
| **total** | **153** |

Every one of them is a rename:

| replacement | count |
|---|---|
| `STR` → `XTR` | 77 |
| `STRX` → `XTRX` | 8 |
| `LSS` → `XSS` | 7 |
| `STR2` → `XTR2` | 2 |
| `STR1` → `XTR1` | 2 |
| `LSS` → `LSSX` | 2 |
| `STR0` → `XTR0` | 1 |

### The rename is incomplete

Note `LSS` went to `XSS` in seven places but to `LSSX` in two, and several
references were missed entirely. In the patched sources these names are
**referenced but never defined**:

```
STR   STR0   STR1   STR2   STR1X   STR2X
```

In the 1973 originals all of them are consistently defined and used
(`STR` alone appears 99 times). So six of the twenty symbols that remain
undefined after a full rebuild are artifacts of this incomplete patch, not
of the original program or of the modern assembler.

Regenerate this analysis with `mac-c/scripts/attic/diff_cvs_vs_original.sh` and
`mac-c/scripts/attic/check_rename_gaps.sh`.

## Contents

| file(s) | contents |
|---|---|
| `TSS1.ORG` … `TSS5.ORG` | original 1973 source, 8-bit |
| `TSS1.SYMB` … `TSS5.SYMB` | CVS-patched source, 8-bit — the version that assembles, copied to `../src/` |
| `ASSYSA.SYMB`, `ASSYSB.SYMB` | the MAC build scripts (version A, and B with the `DEBUG` mark) |
| `MINIT.SYMB` | standalone mass-storage initialisation program |
| `TDUMP.SYMB` | the dumper that punches bootable TSS tapes |
| `LIST.SYMB`, `LIST1`–`LIST5` | list-stream output of the original assembly |
| `ASYMB.SYMB`, `BSYMB.SYMB` | **symbol-table dumps of the two original builds** — the golden oracle |
| `original-text/` | `TSS1.ORG`–`TSS5.ORG` with the parity bit cleared, for reading and diffing |

## Deriving the readable copies

Everything in `../src/` and `../reference/` is this folder with bit 7
cleared:

```bash
perl -pe 's/(.)/chr(ord($1)&0x7f)/ge' archive/TSS1.SYMB > src/TSS1.SYMB
```

`ASYMB.SYMB` and `BSYMB.SYMB` additionally carry a **128-byte NUL leader**
(paper-tape leader) that the copies in `../reference/` drop.

One caveat: inside `'…'` string constants bit 7 is *not* necessarily parity.
`LIST.SYMB` contains 245 bytes that are `0x9D` here and `0x1D` after
stripping, inside register-dump message text. Use these originals for any
byte-level work.
