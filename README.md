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

The documentation is four documents:

| document | contents |
|---|---|
| [`docs/TSS-ARCHITECTURE.md`](docs/TSS-ARCHITECTURE.md) | **the OS, top to bottom** — memory/paging, scheduler, interrupt levels, cold start, the overlay subsystem, terminal I/O and login, file system, users/accounting, the command processor, every device driver, and the emulated CDC/drum devices |
| [`docs/TSS-BRINGUP.md`](docs/TSS-BRINGUP.md) | **from clean checkout to a logged-in `@` prompt** — build, emulator, MINIT disc format, cold-start, login, validation and debugging (scripts in [`bringup/`](bringup/README.md)) |
| [`docs/PROJECT-DESCRIPTION.md`](docs/PROJECT-DESCRIPTION.md) | **history and provenance** — what this is, who wrote it, the `.ORG`-vs-`.SYMB` restoration, how it was originally built and booted |
| [`docs/MAC-ASSEMBLER.md`](docs/MAC-ASSEMBLER.md) | **the assembler** — the MAC/FMAC family, the MAC.BPUN reverse engineering, and the full record of silent-miscompile defects found and fixed in `mac-c` |

This repository holds the recovered source, the original build outputs, and
a C reimplementation of the MAC assembler that rebuilds the system. The
original 1973 build script runs end to end and reproduces **679 of the 693
symbols** of the archived 1978 symbol dump with **zero assembly errors** —
and the rebuilt system **boots to an interactive login** on the nd100x
emulator (likely the first TSS session since the 1970s).

---

## Folders

Each has its own README with the detail.

| folder | contents |
|---|---|
| [`archive/`](archive/README.md) | **the originals, untouched** — 8-bit, parity bit set. Holds the source *twice*: the 1973 original (`.ORG`) and CVS's patched version (`.SYMB`), plus the original build outputs. Everything else is derived from here. |
| [`src/`](src/README.md) | the one clean, assemblable copy of the source |
| [`reference/`](reference/README.md) | **the golden oracle** — the symbol dumps and listings the original assembler produced, used to prove any rebuild correct |
| [`derived/`](derived/README.md) | files this project made: the extracted drum driver, and build variants including a NORD-10 + drum configuration |
| [`mac-c/`](mac-c/README.md) | a MAC assembler in C — 695 unit tests, runs the 1973 build scripts |
| [`Build/`](Build/README.md) | **output only**, disposable — everything the assembler produces |
| [`bringup/`](bringup/README.md) | scripts that take a bare disc to a TSS login, plus validation tools |
| [`docs/`](docs/TSS-ARCHITECTURE.md) | the four documents above |
| [`ppt/`](ppt/README.md) | *Intro to TSS* — the guided tour |

## Rebuilding it

```bash
cd mac-c
make test                 # 695 unit tests
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
  [`docs/MAC-ASSEMBLER.md`](docs/MAC-ASSEMBLER.md).
- **How was TSS started?** Assemble into core, start at address 7 — that
  saves the image to disk and boots it.
- **How do you get to a login prompt from a bare disc?** Format the disc with
  the standalone `MINIT` program, cold-boot with the operator switches at
  `131313₈` (runs `SINIT`, which creates user **SYSTEM**, passwordless), then
  boot normally — every terminal prints `@ENTER`; log in as `SYSTEM` and you
  reach the `@` command prompt. Full procedure:
  [`docs/TSS-BRINGUP.md`](docs/TSS-BRINGUP.md).
