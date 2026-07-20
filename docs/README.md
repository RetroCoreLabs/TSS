# `docs/` — analysis documents

Written while reverse-engineering the archive. Facts are marked
**[VERIFIED]** when read directly from a file, binary or manual, and
**[ASSUMPTION]** when they are interpretation.

The repository overview, with the folder map and quick answers, is the
[main README](../README.md) at the root.

| document | contents |
|---|---|
| `PROJECT-DESCRIPTION.md` | **start here** — what the archive is, who wrote what, the NORD-10 port, the bootstrap chain, rebuild status |
| `TSS-Analysis.md` | what each file is, how the build works, the boot paths, the drum driver, the configuration marks |
| `TSS-Style-Authorship.md` | who wrote what, by coding-style fingerprint, plus a full inventory of the NORD-10-specific code (`.pdf` is the same document) |
| `MAC-BPUN-Analysis.md` | reverse engineering of the real MAC binary: cold-start entry, permanent symbol table, command table, MON-call inventory, BPUN tape layout |
| `BUILD-TSS.md` | how to build TSS — under SINTRAN, stand-alone, or with the C assembler |

For the architecture of TSS itself, read `../ppt/Intro to TSS.pdf` first —
it is a guided tour from the kernel down to the drum's register bits.
