# TSS — documentation hub

This is the map of everything written about **NORD TSS 3.0** (the 1973 Norsk
Data NORD-1 / NORD-10 timesharing OS) in this repository: the OS architecture,
every driver and subsystem, the file system, how users and login work, how you
actually use the OS, and the exact procedure to bring it up from a bare disc.

Every analysis document tags its facts **[VERIFIED]** (read from source, binary,
or a live trace), **[ASSUMPTION]** (interpretation), or **NOT DETERMINED FROM
SOURCE** — nothing here is guessed. Citations are to `src/FILE.SYMB:line`.

For the repository itself (folder map, how to rebuild), see the
[root README](../README.md). For the historical provenance, start with
[`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md).

---

## The OS, top to bottom

| # | document | what it covers |
|---|---|---|
| 1 | [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) | **the big picture** — the five-part TSS1–TSS5 structure, the 8×2K-page virtual-memory / swapping model (`MSTRT=40000₈`), the one-process-per-terminal context model, the LEV0–14 interrupt levels and the scheduler, the 81-entry `MCTBL` monitor-call interface, and the overlay subsystem |
| 2 | [`TSS-DRIVERS.md`](TSS-DRIVERS.md) | **every device driver** — teletypes/terminals (rings, `WBUF`/`RBUF`), the CDC disc (`DKOP`/`DKTR`/`DKADR`), the swapping drum (`XDRUM` @ IOX 540), card reader, line printer, paper tape, Diablo — with device numbers/IOX channels and the NORD-1 (`IOT`) vs NORD-10 (`IOX`) I/O split selected by the `N10` mark |
| 3 | [`TSS-FILESYSTEM.md`](TSS-FILESYSTEM.md) | **the on-disc file system** — NCR vs CDC disc addressing, the MIB free-track bitmap, track allocation (`GTRK`/`RTRK`), the `USTBL`/`USRDK` user tables, the per-user index blocks and 32-word file descriptors, and the full octal disc map |
| 4 | [`TSS-USERS.md`](TSS-USERS.md) | **users, login, accounting** — the user-number model, the create chains (`SINIT`/`CRUSE`/`CRUSR`), `LOGON` authentication, password management (`PASWD`/`CPASW`), and the real CPU+connect-time accounting subsystem (`ACCTD`) |
| 5 | [`TSS-COMMANDS-AND-PROGRAMS.md`](TSS-COMMANDS-AND-PROGRAMS.md) | **using the OS** — the command loop and the full **60-command** table, the monitor-call calling convention, the `PLACE` binary loader and how user programs are loaded and run — and the finding that **no editor or compiler ships in this source** (development was external ND MAC/BPUN) |

## Bringing it up

| # | document | what it covers |
|---|---|---|
| 6 | [`TSS-BRINGUP-RUNBOOK.md`](TSS-BRINGUP-RUNBOOK.md) | **the verified end-to-end bring-up** — the reproducible bare-disc→SYSTEM procedure: build, pad the CDC image, MINIT initialise, cold-start at **address 7** with `--opr=131313`, SYSTEM created (disc-verified), `@ENTER`. Start here to actually bring the system up |
| 7 | [`TSS-BRINGUP-AND-MINIT.md`](TSS-BRINGUP-AND-MINIT.md) | **MINIT internals** — the disc formatter line by line (INITIALIZE / the UPDATE stub / REGENERATE), the MIB bit layout and polarity, NCR addressing, the second standalone tool `TDUMP`, and the cold→login sequence with the **verified live MINIT run** |
| 8 | [`DISK-INIT-USERS-AND-BOOT.md`](DISK-INIT-USERS-AND-BOOT.md) | **the operator how-to** — the `OPR=131313₈` cold-start switch, formatting with MINIT, how `SINIT` creates user SYSTEM, the two on-disc user tables, and how `LOGON` authenticates |
| 9 | [`HANDOFF-LOGIN-BRINGUP.md`](HANDOFF-LOGIN-BRINGUP.md) | **the login bring-up handoff** — the mac-c `9377` tokenizer bug (fixed + validated), the verified create/login mechanism, and the remaining integration steps |

## Background & reference

| document | what it covers |
|---|---|
| [`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md) | **start here for provenance** — what the archive is, who wrote what, the NORD-10 port, the bootstrap chain, rebuild status |
| [`TSS-Analysis.md`](TSS-Analysis.md) | file-by-file analysis, how the build works, the boot paths, the configuration marks |
| [`TSS-Style-Authorship.md`](TSS-Style-Authorship.md) | authorship by coding-style fingerprint; inventory of the NORD-10-specific code |
| [`MAC-BPUN-Analysis.md`](MAC-BPUN-Analysis.md) | reverse-engineering of the real MAC binary (cold start, permanent symbol table, MON-call inventory, BPUN layout) |
| [`BUILD-TSS.md`](BUILD-TSS.md) | how to build TSS — under SINTRAN, stand-alone, or with the C assembler |
| [`CDC-DISC-DEVICE.md`](CDC-DISC-DEVICE.md) / [`OVERLAY-DISC-SPEC.md`](OVERLAY-DISC-SPEC.md) | the CDC disc device and the overlay-on-disc format |

---

## How to build (from `mac-c/`)

One Makefile drives the whole pipeline; `mac-as` is a prerequisite of every
artifact, so editing the assembler rebuilds it before any TSS build.

| command | builds |
|---|---|
| `make` | `mac-as` (the C reimplementation of MAC) + `test_mac` |
| `make test` | the unit-test suite (677 assertions, must be 0 failures) |
| `make tss` | the golden ASSYSA/ASSYSB build, scored against `reference/` |
| `make drum` | the NORD-10 + swapping-drum bootable image |
| `make minit` | the standalone MINIT disc formatter BPUN |
| `make all` | tools + tests + every artifact |

Requires a POSIX toolchain; on Windows build under WSL.

## The one-paragraph bring-up

Assemble TSS and MINIT with the fixed `mac-as` → run **MINIT** to lay down the
MIB free-track bitmap on the CDC disc (verified working on nd100x, see doc #6) →
cold-boot with the operator switches at `131313₈` so **`SINIT`** creates user
**SYSTEM** (passwordless) → boot normally; every terminal prints `@ENTER` and
you log in as `SYSTEM`. The full step-by-step with what is verified vs still
open is in [`TSS-BRINGUP-AND-MINIT.md`](TSS-BRINGUP-AND-MINIT.md) and
[`DISK-INIT-USERS-AND-BOOT.md`](DISK-INIT-USERS-AND-BOOT.md).
