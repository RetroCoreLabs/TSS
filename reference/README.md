# `reference/` — output of the original build, and the golden oracle

What the 1973/78 MAC assembler actually produced, parity bit cleared. These
files are **evidence**, not input: they are how any modern rebuild is
proved correct.

| file | contents |
|---|---|
| `ASYMB.SYMB` | **symbol-table dump of the version-A build** — 693 symbols |
| `BSYMB.SYMB` | symbol-table dump of the version-B (`DEBUG`) build — 689 symbols |
| `LIST1.SYMB` … `LIST5.SYMB` | list-stream output of assembling `TSS1`–`TSS5` |
| `LIST.SYMB` | one combined listing covering TSS3 + TSS4 + TSS5 |

Derived from `../archive/` by clearing bit 7; the two symbol dumps also drop
a 128-byte NUL paper-tape leader that the originals carry.

## Why the symbol dumps matter

`ASYMB`/`BSYMB` are produced by MAC's `)LIST` command at the end of
`ASSYSA`/`ASSYSB`. Each line is a symbol and its final octal value:

```
 KLOK=000016
DEVTB=002621
```

Because they record the **address of every label**, they pin down the exact
size of every routine, table and conditional region in the assembled image.
Any assembler that reproduces them has reproduced the build.

## Using them

```bash
cd ../mac-c
./scripts/build/build_tss_assysa.sh
./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
./scripts/verify/first_divergence.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
```

`compare_asymb.sh` scores exact matches; `first_divergence.sh` walks the
dump in definition order and reports the first symbol whose value differs,
which localises a missing or extra word to a handful of source lines.

Current score: **679 / 693** for version A, **675 / 689** for version B,
with zero assembly errors. Thirteen of the unmatched entries in each are
macro names — real MAC lists a macro with the address of its body *inside
MAC's own memory image*, which a host assembler cannot reproduce.

## A caveat on the listings

`LIST1.SYMB` is truncated mid-line (it ends `SMPR, STA SMPA; STX S`). That
is how the archive is; the corresponding source is complete.
