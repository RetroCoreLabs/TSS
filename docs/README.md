# `docs/` — analysis documents

Written while reverse-engineering the archive. Facts are marked
**[VERIFIED]** when read directly from a file, binary or manual, and
**[ASSUMPTION]** when they are interpretation.

The repository overview, with the folder map and quick answers, is the
[main README](../README.md) at the root.

| document | contents |
|---|---|
| `TSS.md` | **the documentation hub** — describes and links every document below, plus the build commands and the one-paragraph bring-up. Start here to navigate. |
| `PROJECT-DESCRIPTION.md` | **start here** — what the archive is, who wrote what, the NORD-10 port, the bootstrap chain, rebuild status |
| `TSS-Analysis.md` | what each file is, how the build works, the boot paths, the drum driver, the configuration marks |
| `TSS-ARCHITECTURE.md` | **the OS architecture** — the five-part structure, the 8×2K virtual-memory/paging model, the process/context model, the PIL interrupt levels and scheduler, the 81-entry `MCTBL` monitor-call interface, and the overlay subsystem |
| `TSS-DRIVERS.md` | **every device driver** — teletypes/terminals, CDC disc, swapping drum, card reader, line printer, paper tape, Diablo — with device numbers/IOX channels and the NORD-1 (`IOT`) vs NORD-10 (`IOX`) I/O split |
| `TSS-FILESYSTEM.md` | **on-disc file system** — NCR vs CDC disc addressing, the MIB free-track bitmap, track allocation (`GTRK`/`RTRK`), the `USTBL`/`USRDK` user tables, the per-user index blocks and file descriptors, and the full disc map |
| `TSS-USERS.md` | **users, login, accounting** — the user-number model, create chains (`SINIT`/`CRUSE`/`CRUSR`), `LOGON` authentication, password management (`PASWD`/`CPASW`), and the CPU+connect-time accounting subsystem (`ACCTD`) |
| `TSS-COMMANDS-AND-PROGRAMS.md` | **using the OS** — the command loop and full 60-command table, the monitor-call calling convention, the `PLACE` binary loader and how user programs are run, and the finding that **no editor/compiler ships in this source** |
| `TSS-BRINGUP-RUNBOOK.md` | **the verified end-to-end bring-up** — bare disc → build → pad the CDC image → MINIT initialise → cold-start at **address 7** with `--opr=131313` → **SYSTEM created** (disc-verified) → `@ENTER`. Reproducible, with the address-7 gotcha and the one open login item |
| `TSS-BRINGUP-AND-MINIT.md` | **MINIT internals + the exact bring-up runbook** — the disc formatter line by line (INITIALIZE/UPDATE/REGENERATE), the MIB bit layout and polarity, NCR addressing, `TDUMP`, and the cold→login sequence with each step marked verified or open |
| `TSS-Style-Authorship.md` | who wrote what, by coding-style fingerprint, plus a full inventory of the NORD-10-specific code (`.pdf` is the same document) |
| `MAC-BPUN-Analysis.md` | reverse engineering of the real MAC binary: cold-start entry, permanent symbol table, command table, MON-call inventory, BPUN tape layout |
| `BUILD-TSS.md` | how to build TSS — under SINTRAN, stand-alone, or with the C assembler |
| `DISK-INIT-USERS-AND-BOOT.md` | **from a bare disc to a login prompt** — the operator's-panel switches (`OPR=131313`), formatting the disc with `MINIT`, how `SINIT` creates user SYSTEM, the two on-disc user tables (`USTBL` names / `USRDK` passwords), and how `LOGON` authenticates |
| `HANDOFF-LOGIN-BRINGUP.md` | handoff for the login bring-up — the mac-c `9377` tokenizer bug (fixed + validated), the verified create/login mechanism, and the remaining `MINIT` run procedure |

For the architecture of TSS itself, read `../ppt/Intro to TSS.pdf` first —
it is a guided tour from the kernel down to the drum's register bits.
