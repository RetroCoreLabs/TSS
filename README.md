# NORD Time Sharing System 3.0

**A complete, standalone timesharing operating system for the Norsk Data
NORD-1 and NORD-10, written by Bo Lewendal in 1973.** It owns the machine —
it is not a program running under SINTRAN.

Every terminal is a process with its own 8-page virtual memory, swapped
between core and mass storage — a CDC disk, or a drum. Users get a command
processor, a file system, mail and accounting; programs get a monitor-call
interface. Roughly **14,000 lines of MAC assembler** in five parts, with the
algorithms documented in structured pseudo-code beside the code.

| | |
|---|---|
| **5** | source parts, TSS1–TSS5 |
| **81** | monitor calls in `MCTBL` |
| **24** | teletypes maximum |
| **8 × 2K** | pages per process, at `40000₈` |

The NORD-10 device drivers — including the swapping drum — were written by
**Nils Jakob Langeland** in spring 1973.

> **New here?** Read [`ppt/Intro to TSS.pdf`](ppt/) for the architecture,
> then [`docs/PROJECT-DESCRIPTION.md`](docs/PROJECT-DESCRIPTION.md) for the
> provenance, authorship and rebuild status.
>
> **Want the whole system explained?** [`docs/TSS.md`](docs/TSS.md) is the
> documentation hub — it describes and links every document: the OS
> architecture, all drivers and subsystems, the file system, users and login,
> how to use the OS, and the exact bring-up procedure (including the standalone
> MINIT disc formatter, now verified running on the emulator).

This repository holds the recovered source, the original build outputs, and
a C reimplementation of the MAC assembler that rebuilds the system. The
original 1973 build script runs end to end and reproduces **679 of the 693
symbols** of the archived 1978 symbol dump with **zero assembly errors**.

---

## Folders

Each has its own README with the detail.

| folder | contents |
|---|---|
| [`archive/`](archive/README.md) | **the originals, untouched** — 8-bit, parity bit set. Holds the source *twice*: the 1973 original (`.ORG`) and CVS's patched version (`.SYMB`), plus the original build outputs. Everything else is derived from here. |
| [`src/`](src/README.md) | the one clean, assemblable copy of the source |
| [`reference/`](reference/README.md) | **the golden oracle** — the symbol dumps and listings the original assembler produced, used to prove any rebuild correct |
| [`derived/`](derived/README.md) | files this project made: the extracted drum driver, and build variants including a NORD-10 + drum configuration |
| [`mac-c/`](mac-c/README.md) | a MAC assembler in C — 677 unit tests, runs the 1973 build scripts |
| [`Build/`](Build/README.md) | **output only**, disposable — everything the assembler produces |
| [`docs/`](docs/README.md) | analysis documents |
| [`ppt/`](ppt/README.md) | *Intro to TSS* — the guided tour |

## Rebuilding it

```bash
cd mac-c
make test                 # 677 unit tests
./build_tss_assysa.sh     # run the real ASSYSA and ASSYSB scripts
./compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
```

Needs a POSIX toolchain; on Windows use WSL.

## Quick answers

- **Is this NPL?** No — MAC assembler throughout.
- **Are the top-level files build logs?** No. `TSSn:SYMB` is real source,
  `LISTn` is the list output, `ASYMB`/`BSYMB` are symbol dumps.
- **Is there drum driver code?** Yes — `XDRUM`, NORD-10 only, device
  `IOX 540`, extracted to [`derived/DRUM-DRIVER.SYMB`](derived/README.md).
  It is compiled *out* of both archived builds.
- **What is the difference between `.ORG` and `.SYMB`?** 153 lines, all
  symbol renames (`STR`→`XTR`, `LSS`→`XSS`) made because those names collide
  with MAC's built-in symbols — and the rename is incomplete. See
  [`archive/README.md`](archive/README.md).
- **Can it be assembled today?** Yes, with `mac-c/`. The real MAC binary
  cannot run without SINTRAN — see
  [`docs/MAC-BPUN-Analysis.md`](docs/MAC-BPUN-Analysis.md).
- **How was TSS started?** Assemble into core, start at address 7 — that
  saves the image to disk and boots it.
- **How do you get to a login prompt from a bare disc?** Format the disc with
  the standalone `MINIT` program, cold-boot with the operator switches at
  `131313₈` (runs `SINIT`, which creates user **SYSTEM**, passwordless), then
  boot normally — every terminal prints `@ENTER`. Full procedure, the operator
  switch values, and how the two on-disc user tables (`USTBL` names / `USRDK`
  passwords) drive login: [`docs/DISK-INIT-USERS-AND-BOOT.md`](docs/DISK-INIT-USERS-AND-BOOT.md).
