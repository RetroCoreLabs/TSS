# `archive/original-text/` — readable copies of the 1973 originals

These are the **pristine 1973 pre-CVS source** (`TSS1.ORG`–`TSS5.ORG`) with the
ND **parity bit (bit 7) cleared**, so they open as plain 7-bit ASCII text for
reading and diffing. They are a *convenience rendering* of `../TSS*.ORG` — same
bytes, high bit stripped — not a separate program.

Regenerate any of them from the 8-bit original with:

```bash
perl -pe 's/(.)/chr(ord($1)&0x7f)/ge' ../TSS1.ORG > TSS1.ORG
```

## Contents

| file | lines | what it is |
|---|---|---|
| `TSS1.ORG` | 4,708 | kernel — interrupts, device drivers, scheduler, swapper, page tables |
| `TSS2.ORG` | 3,511 | context block, monitor-call implementations, command processor |
| `TSS3.ORG` | 1,754 | overlay machinery (`GOVER`/`OVERL`/`OVERX`), system utilities |
| `TSS4.ORG` | 2,039 | utility overlays — DUMP, RECV and friends |
| `TSS5.ORG` | 1,994 | user-management overlays — create/delete user, accounts |

## How this differs from the neighbours

| folder | files | parity | CVS `STR`→`XTR` rename |
|---|---|---|---|
| `../` (archive root) | `TSS*.ORG` | **set** (8-bit) | no — the 1973 original |
| **here** (`original-text/`) | `TSS*.ORG` | **cleared** (readable) | no — the 1973 original |
| `../../src/` | `TSS*.SYMB` | cleared | **yes, and completed** — the version that assembles cleanly |

So `original-text/` is the readable form of the **pre-CVS** program. It is *not*
what the build uses — the build assembles the CVS-patched, rename-completed
`../../src/` (see `../../src/README.md`). Use these files to see what Bo
Lewendal actually wrote in 1973, and to diff the original against the patched
source. The full catalogue of what CVS changed is in `../README.md`.

> Note: these are kept purely for human reading. mac-c can now assemble the
> parity-set originals in `../` directly (it strips bit 7 on ingest), so this
> folder is not needed by any tool — only by people.
