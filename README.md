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
| **24** | teletypes maximum (the shipped build is assembled for **10**: the console plus TTY5-TTY10 = nd100x TERMINAL 5-10; TTY2-TTY4 cannot be used on nd100x because its terminals at IOX 0310-0330 answer idents 0121-0123, not the 5-7 TSS dispatches on) |
| **8 × 2K** | pages per process, at `40000₈` |

The NORD-10 device drivers — including the swapping drum — were written by
**Nils Jakob Langeland** in spring 1973.

## TSS and SINTRAN III

TSS 3.0 is dated 1973 by its own source (the NORD-10 drivers are signed
17/4/73 and 28/5/73). SINTRAN III, Norsk Data's later operating system,
carries much of the same design. Compared against the *SINTRAN III Users
Guide* (ND-60.050.06, June 1976) and *SINTRAN III Monitor Calls*
(ND-860228):

- **Monitor calls.** Both use `MON n` (`161000` plus an 8-bit call number)
  dispatched through a table. 18 of the 81 TSS call-table entries have the
  same number and the same name in SINTRAN III: octal `0`-`10` (`LEAVE`,
  `INBT`, `OUTBT`, `ECHOM`, `BRKM`, `RDISK`, `WDISK`, `RPAG`/`RPAGE`,
  `WPAG`/`WPAGE`), `13`-`14` (`CIBUF`, `COBUF`), `32` `MSG`, `35` `IOUT`,
  `64`-`67` (`ERMSG`, `QERMS`, `ISIZE`, `OSIZE`) and `76` `SETBS`. Where the
  arguments were checked, they sit in the same registers. TSS 3.0 even
  implements two SINTRAN III calls at SINTRAN III's numbers, under SINTRAN III
  names in its source: `OPEN` at 50 and `CLOCK` at 113.
- **Commands.** 24 of the 60 TSS commands exist under the same name in
  SINTRAN III, for example `WHO-IS-ON`, `CREATE-FRIEND`, `PLACE-BINARY` and
  `INIT-ACCOUNTING`. About 13 more are renamed equivalents: `ALLOCATE`
  became `ALLOCATE-FILE` with the same arguments, counted in pages instead
  of tracks.
- **Files and users.** The same `(USER)NAME:TYPE` file names, the same rule
  that a name in quotation marks creates a new file, six of the seven TSS
  default file types, friends, owner/friend/public access, and the same
  login sequence: ESC, `ENTER`, password, `PROJECT NUMBER`.

CERN documents from the time confirm it from the users' side. In February
1975 a CERN paper on choosing the service computer's operating system says
"all subsystems made for NORD-TSS are directly transferable to Sintran III as
background programs, because the monitor calls are similar. The command
monitor is also very similar to the one in NORD-TSS", and that Norsk Data
itself no longer used TSS but had adopted SINTRAN III. CERN had run TSS since
1973, by 1975 with eleven terminals. Tor Olav Steine, a former Norsk Data employee, says in his 2020 history of
the company that SINTRAN III was built from SI's virtual memory project, "Bo
Lewendal's TSS", floating-point operations and SINTRAN II, mainly by Trygve
Matre. No source says part by part what came from TSS. The comparison,
with a citation for every match, is in
[`docs/TSS-AND-SINTRAN.md`](docs/TSS-AND-SINTRAN.md), together with the history of
the two systems and a source grade for every statement.

> **New here?** Read [`ppt/Intro to TSS.pdf`](ppt/) for the architecture,
> then [`docs/PROJECT-DESCRIPTION.md`](docs/PROJECT-DESCRIPTION.md) for the
> provenance, authorship and rebuild status.

The documentation:

| document | contents |
|---|---|
| [`docs/TSS-SOURCE-FILES.md`](docs/TSS-SOURCE-FILES.md) | **per-file reference** — every `.SYMB` file: what it is, what is in it, whether it is built, and where it is documented in depth |
| [`docs/TSS-PSEUDOCODE.md`](docs/TSS-PSEUDOCODE.md) | **the source in pseudo-C** — every routine, what it does, with the line range to check it against |
| [`docs/TSS-ARCHITECTURE.md`](docs/TSS-ARCHITECTURE.md) | **the OS, top to bottom** — memory/paging, scheduler, interrupt levels, cold start, the overlay subsystem, terminal I/O and login, file system, users/accounting, the command processor, every device driver, and the emulated CDC/drum devices |
| [`docs/TSS-BRINGUP.md`](docs/TSS-BRINGUP.md) | **from clean checkout to a logged-in `@` prompt** — build, emulator, MINIT disc format, cold-start, login, validation and debugging (scripts in [`bringup/`](bringup/README.md)) |
| [`docs/PROJECT-DESCRIPTION.md`](docs/PROJECT-DESCRIPTION.md) | **history and provenance** — what this is, who wrote it, the `.ORG`-vs-`.SYMB` restoration, how it was originally built and booted |
| [`docs/MAC-ASSEMBLER.md`](docs/MAC-ASSEMBLER.md) | **the assembler** — the MAC/FMAC family, the MAC.BPUN reverse engineering, and the full record of silent-miscompile defects found and fixed in `mac-c` |
| [`docs/TSS-USER-MANUAL.md`](docs/TSS-USER-MANUAL.md) | **user manual** — logging in, the command processor, and all 60 `@`-prompt commands, with the error-message table and device list |
| [`docs/TSS-COMMAND-VALIDATION.md`](docs/TSS-COMMAND-VALIDATION.md) | **what actually happens when you run it** — every one of the 60 commands exercised against the emulator, verbatim transcripts, root causes, and the corrections they forced on the manual |
| [`docs/OPEN-QUESTIONS.md`](docs/OPEN-QUESTIONS.md) | **the live list** of what is still unanswered, and what would settle each item |
| [`docs/TSS-FLOAT-FORMAT.md`](docs/TSS-FLOAT-FORMAT.md) | why the clock cannot advance: TSS is a 48-bit floating-point program whose `TBANG` constants were assembled in the 32-bit format — a defect in the 1978 original, not in this rebuild |

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
| [`mac-c/`](mac-c/README.md) | a MAC assembler in C — 732 unit tests, runs the 1973 build scripts |
| [`Build/`](Build/README.md) | **the built binaries, tracked** — the runnable TSS tape and disc (`drum/`), the MINIT and bootstrap tapes, the golden symbol dumps; regenerated by `make build` |
| [`bringup/`](bringup/README.md) | bring-up validation tools (the bring-up itself is `make help` at the root) |
| [`docs/`](docs/README.md) | the documents above |
| [`ppt/`](ppt/README.md) | *Intro to TSS* — the guided tour |

## Getting started

Two ways in: **download a ready-made disc set**, or **build everything from
source**.

### A. Download the release kit

Every tagged release ships the built system, the format and bootstrap
tapes, and the disc media — see
[Releases](https://github.com/RetroCoreLabs/TSS/releases). Unpack it, then
either **jump start** with the ready-made `cdc-jumpstart.img` (copy it over
`cdc.img`) or follow the archive's README through the one-time bring-up
(bootstrap tape → MINIT format → cold start, about two minutes — the same
steps a 1973 operator performed). Then:

```bash
tar xzf nord-tss-3.0-*.tar.gz && cd nord-tss-3.0-*
./run-tss.sh /path/to/nd100x
```

Log in as **SYSTEM**, project **1**, no password. `HELP` lists all 60
commands. The bring-up and usage guide is
[`dist/QUICKSTART.md`](dist/QUICKSTART.md) in this repo.

You still need the emulator — it is a separate project:

```bash
git clone https://github.com/RetroCoreLabs/nd100x.git
cd nd100x
sudo apt install build-essential cmake libcjson-dev    # Debian/Ubuntu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

> **`--mms=1` is required and cannot go in the config file.** TSS is 1973 code
> that programs the MMU the Paging-System-I way; under nd100x's default MMS2
> the machine starts and then hangs in the overlay loader. `run-tss.sh` passes
> the flag for you.

### B. Build it from source

Needs a POSIX toolchain; on Windows use WSL. Every helper script is POSIX
shell or awk: the repository is python-free by decision.

```bash
git clone https://github.com/RetroCoreLabs/TSS.git && cd TSS
make build            # assembler + 732 tests + TSS, DRUM and MINIT artifacts
make prepare          # fresh disc set; the CDBIN tape installs the bootstrap
make format           # MINIT format (type 4470, 4670, I)
make coldstart        # cold start -> a disc with user SYSTEM
make verify           # free tracks + SYSTEM present
make login            # boot and log in
```

`make help` lists every target; the emulator is found via `ND=/path/to/nd100x`
or the `ND100X` environment variable. The whole procedure, including doing it
by hand at the operator panel, is in [`docs/TSS-BRINGUP.md`](docs/TSS-BRINGUP.md).

To check the rebuild against the 1978 original:

```bash
./mac-c/scripts/verify/verify_repo.sh     # build, tests, oracle, links
```

## Reproducing the 1973 build exactly

`make build` above is the convenient path. To run the *original* `ASSYSA` and
`ASSYSB` command streams — the scripts the operators typed in 1978 — and score
the result against the archived symbol dump:

```bash
cd mac-c
./scripts/build/build_tss_assysa.sh
./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
```

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
- **Is there really a backdoor?** Yes. `LOGON` bypasses the password check
  when what you type hashes to `0636` — which is `..`, two periods. It is in
  Lewendal's 1973 original *and* in the 1978 assembly listing, and it has been
  verified by logging in with it. See
  [`docs/PROJECT-DESCRIPTION.md` §9](docs/PROJECT-DESCRIPTION.md).
- **How was TSS started?** Assemble into core, start at address 7 — that
  saves the image to disk and boots it.
- **How do you get to a login prompt from a bare disc?** Format the disc with
  the standalone `MINIT` program, cold-boot with the operator switches at
  `131313₈` (runs `SINIT`, which creates user **SYSTEM**, passwordless), then
  boot normally — the console prints `@ENTER`; log in as `SYSTEM` and you
  reach the `@` command prompt. The other teletypes start **dead**: the
  level-6 scanner wakes one when **ESC** (033) is typed on it, and only then
  it prints `@ENTER` (`src/TSS1.SYMB` `LEV6`: `AND (177; SUB (33; JAF L2`).
  On nd100x, start with `--telnet=PORT`, connect, pick `Terminal 39`
  (= TERMINAL 8 = TSS TTY8), press ESC. Full procedure:
  [`docs/TSS-BRINGUP.md`](docs/TSS-BRINGUP.md).

## External resources — set these, never hard-code them

Nothing in this repository contains an absolute path. Every path in a script,
Makefile, config file, source comment or document is **relative to the
repository root**, so a clone works on any machine, drive or OS.

A few inputs genuinely live outside the repo — the original ND binaries,
reference manuals, microcode and the emulator. Those are reached through
environment variables. Set only the ones a given task needs:

| variable | points at | used by |
|---|---|---|
| `ND100X` or `ND` | the `nd100x` emulator **binary** | `Makefile` (bring-up targets), the probe scripts |
| `ND100X_SRC` | the `nd100x` **source tree** | device-analysis docs |
| `ND_BPUN_DIR` | directory holding `MAC.BPUN`, `MACM-*.BPUN`, `FMAC-*.BPUN` | `docs/MAC-ASSEMBLER.md` |
| `NDINSIGHT` | the NDInsight reference-manual collection | the manual citations throughout `docs/` |
| `ND110COMPILE` | the ND110Compile tree holding `uCode/` | the microcode citations in `docs/TSS-FLOAT-FORMAT.md` |

Example:

```bash
export ND_BPUN_DIR=/path/to/ND/BPUN
export NDINSIGHT=/path/to/NDInsight
```

Scripts that need one of these fail immediately with a message naming the
variable, rather than silently searching a path that only exists on one
machine.
