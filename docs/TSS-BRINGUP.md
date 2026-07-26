# NORD TSS 3.0 — the complete bring-up reference

From a clean checkout to a logged-in `@` command prompt on the nd100x emulator:
building the toolchain, building the OS images, preparing and formatting the
disc, cold-starting, creating the first user, logging in — plus every
validation and debugging tool used along the way.

Evidence marking: **[VERIFIED]** = observed live on the emulator or read from
the cited source line(s); **[ASSUMPTION]** = interpretation, reasoning stated.
Citations are `file:line`. All addresses and disc values are **octal** unless
marked decimal.

For the *quick* script-driven walkthrough, see
[`../bringup/README.md`](../bringup/README.md) — this document is the full
reference behind those scripts.

---

## 1. Overview and verified end state

### 1.1 What this is

The recovered 1973 NORD TSS 3.0 source (`src/TSS1..TSS5.SYMB`) is rebuilt with
`mac-c/` (a C99 reimplementation of the ND MAC assembler) and executed on the
nd100x ND-100 emulator. Two facts drive everything:

1. **The 1978 golden builds were never executed** — they only produced
   symbol-table dumps (`reference/ASYMB.SYMB`, `BSYMB.SYMB`). Running TSS on
   nd100x is the **first execution ever**, so it exposes bugs a dump cannot:
   undefined variables written through null literals, a wrong paging model,
   device-encoding mistakes, and silent assembler miscompiles whose word
   counts are correct. "Correct for the dump" ≠ "correct for a run."
2. There are **two programs in `archive/`** and two hardware targets. nd100x
   runs the **NORD-10 / DRUM** build; conditional-assembly marks `"N10` /
   `"NN10` select NORD-10 vs NORD-1 code throughout.

Layout: `archive/` originals (never edit) → `src/` clean source → `reference/`
golden oracle → `Build/` output (disposable) → `mac-c/` the assembler →
top-level `Makefile` (the bring-up driver; `make help`) → `bringup/` the
validation tools.

### 1.2 The verified end state [VERIFIED, 2026-07-23]

The **full chain works**, end to end:

```
make build                                (mac-as + tests + all artifacts)
  →  make prepare                         (padded CDC image)
  →  MINIT format: 4470 / 4670 / I       (16 free tracks in the MIB)
  →  cold-start --start=7 --opr=131313    (SINIT creates SYSTEM, disc-verified)
  →  normal boot --start=7                (@ENTER)
  →  SYSTEM  →  PROJECT NUMBER 1
  →  AMBIGUOUS FILENAME                   (non-fatal: no ()SCRATCH on a fresh disc)
  →  TYPE IN DATE (DD,MM,YYYY,HH,MM,SS)   (first login only)
  →  @                                    (the command prompt)
```

> **[VERIFIED 2026-07-26] One more step is required before the file system is
> usable.** After `SINIT`, **every user's track quota is zero — including
> `SYSTEM`** (`SINIT` creates it through `CRUSE`, which passes literal `0`,
> `src/TSS5.SYMB:103`). Until quota exists, `CREATE-USER` reports a spurious
> `ALREADY EXISTS`, `TRANSFER` from `SYSTEM` fails, and no user can create a
> file. As `SYSTEM`, at the `@` prompt:
>
> ```
> @TRANSFER
> TO USER: SYSTEM
> FROM USER: SYSTEM          <-- same user: mints quota, skips the debit
> NUMBER OF TRACKS: 20
> @LIST-TRACKS
> USER NAME: SYSTEM
> 20 TRACKS LEFT
> ```
>
> `TRTRK` jumps straight to the credit when `TO` equals `FROM`
> (`src/TSS5.SYMB:1589`, `JAZ T4`), so this is the only way to create quota
> from nothing. `DISK-SPACE` is unaffected — quota and physical free space are
> separate accounts, and a quota larger than the disc can be issued. Details:
> `docs/TSS-COMMAND-VALIDATION.md` D3.

At the `@` prompt, `HELP` lists the full ~60-command catalogue and
`WHO-IS-ON` prints `1 SYSTEM`. **[VERIFIED]** live.

Quality gates at this state: mac-c tests **695 passed / 0 failed**; golden
oracle **679/693 exact matches (version A), 675/689 (version B)**, zero
assembly errors; `verify_repo.sh` fully green.

### 1.3 The two facts that make or break the bring-up

1. **Cold-start at ADDRESS 7, not the BPUN autostart.** The tape's built-in
   start (`ISTRT`) is the *normal-running* entry and skips the
   operator-switch (`OPR`) test that dispatches `SINIT`. Enter at
   **`--start=7`** (`make coldstart`/`make login` set it). See §6.
2. **Pre-size the CDC disc.** The emulated CDC device does not grow on
   writes; the user area (NCR `4470`) maps to a high physical sector, so the
   backing file must be padded (`make prepare` pads to 8192 sectors / 4 MB).
   See §5.1.

### 1.4 Bring-up — the automated path and the manual inputs

Everything is driven by the top-level `Makefile` (run from WSL,
`cd /mnt/e/Dev/Ronny/TSS`; `make help` lists every target).

**Recommended — one command, fully unattended:**

```bash
make build      # once: mac-as + 695 tests + the TSS/MINIT artifacts
make auto       # prepare + format + cold-start, driven over DAP; disc PERSISTS
make login      # interactive: log in and use TSS
```

`make auto` runs the format and cold-start under the DAP debugger, auto-answers
the MINIT prompts, and stops each phase with a **clean debugger-terminate** so the
CDC disc is reliably written back (see the persistence note below). Nothing is
typed. Afterwards `make verify` shows *15 free tracks + SYSTEM*, and you `make login`.

**Manual steps (interactive) — every input spelled out:**

| step | command | exactly what you type / wait for | verify |
|---|---|---|---|
| 1 | `make prepare` | nothing — makes a fresh padded disc in `Build/bringup/` | — |
| 2 | `make format` | `4470` ⏎ , then `4670` ⏎ , then `I` ⏎ ; wait for `FINISHED` | `make verify` → *16 free tracks* |
| 3 | `make coldstart` | nothing — wait for `NORD TSS VERSION 3.0A IS UP` then `@ENTER` | `make verify` → *15 free + SYSTEM* |
| 4 | `make login` | at `@ENTER`: `SYSTEM` ⏎ ; at `PROJECT NUMBER P-`: `1` ⏎ ; on the *first* login, at `TYPE IN DATE (DD,MM,YYYY,HH,MM,SS):` e.g. `24,07,2026,15,30,00` ⏎ | you reach the `@` prompt |

What every input means:
- **MINIT**: `4470` = first user-area disc address (FSYS start), `4670` = last
  (gives 16 free tracks), `I` = INITIALIZE (vs `U` update / `R` regenerate). Full
  dialogue in §5.6.
- **Cold-start**: driven by the operator switches **`--opr=131313`** and the boot
  entry **`--start=7`** (both set by `make coldstart`; see §6). SINIT creates user
  SYSTEM; **no console input is needed**.
- **Login**: SYSTEM is **passwordless** (no PASSWORD prompt). `PROJECT NUMBER`
  must be a **positive** integer. The `AMBIGUOUS FILENAME` on the first login is
  **harmless** — it's the missing per-user scratch file `SCRATCH:DATA` (see the
  User Manual, `TSS-USER-MANUAL.md` §5). The date is asked on the first login only.

> **Disc persistence [FIXED 2026-07-24].** The CDC and drum devices in nd100x are
> now **write-through**: every disc sector is written to the image file the instant
> the guest writes it and flushed (patch to `deviceCDC.c` / `deviceDrum.c`,
> `CDC_OP_WRITE` / `DRUM_FUNC_WRITE`). So the manual steps above **persist
> correctly when stopped with Ctrl-C.** *Verified:* a SIGINT-stopped MINIT format
> keeps its 16 free tracks and a SIGINT-stopped cold-start keeps SYSTEM.
>
> *Background (why this mattered):* CDC/drum previously buffered the whole disc
> surface in RAM and wrote it back only on a clean shutdown
> (`cleanup_machine → *_Destroy`, reached at `CPU_SHUTDOWN`). A plain Ctrl-C/SIGINT
> calls `exit(0)` and skips that path, so it *lost* format/cold-start writes —
> while SMD (SINTRAN) and floppy, which write straight through to their image
> `FILE*`, were never affected. The write-through patch removes the difference.
> **This requires the patched nd100x.** On an unpatched build, use **`make auto`**
> (DAP-driven, clean terminate) for the steps that must persist. `make login`
> needs no persistence either way — it only reads the disc.

---

## 2. Building the toolchain and the images

All commands run from WSL. **PowerShell→WSL quoting mangles `$vars`/awk in
inline one-liners — always write a script file and run it.**

### 2.1 mac-c and the TSS artifacts

```bash
# The assembler + its test suite (must be 0 failures)
cd /mnt/e/Dev/Ronny/TSS/mac-c && make test

# The runnable NORD-10/DRUM TSS: bootable BPUN + drum image + CDC overlay disc
./scripts/build/build_tss_drum.sh    # -> Build/drum/{tss-drum.bpun, tss-drum.img, tss-cdc.img, DSYMB.SYMB}

# The standalone disc formatter
./scripts/build/build_minit.sh       # -> Build/minit/minit.bpun   (entry MINIT = 000206)
```

Both TSS and MINIT build with **0 assembly errors**.

| File | Role |
|------|------|
| `Build/drum/tss-drum.bpun` | bootable TSS image (BPUN); tape autostart = `ISTRT` |
| `Build/drum/tss-drum.img`  | swapping-drum backing image (IOX 540) |
| `Build/drum/tss-cdc.img`   | CDC system-disc image (IOX 500–507) — **overlays only** (§5.5) |
| `Build/drum/DSYMB.SYMB`    | symbol→address table; **always resolve addresses from here** |
| `Build/minit/minit.bpun`   | standalone MINIT formatter, base `000000`, entry `MINIT` |

> **Address caveat:** any `TSS2+` source edit shifts every later symbol by +1
> word. Never hard-code octal addresses across a rebuild — read them from
> `DSYMB.SYMB`. The `)PCL`-killed local labels (`S10`, `SYY`, `S99`) are
> absent from the dump; they were recovered by decoding `tss-drum.img` for
> the `TRA OPR` opcode (`150002`).

### 2.2 CRITICAL build gotcha — `make test` does not relink `mac-as`

`make test` rebuilds `test_mac` but does **not** relink `mac-as`. After any
edit under `mac-c/src/`, run plain **`make`** in `mac-c/` *before* any
`build_tss_*.sh`, or the images are built with the **stale assembler** — a
failure mode that cost a full debug cycle (the fixed code tested green while
the emulator ran an image built by the old binary). Verify an artifact with:

```bash
python3 bringup/check-robj-encoding.py Build/bringup/tss.bpun   # must print FIXED
```

which byte-checks the built image for the fixed-vs-broken `ROBJ`
forward-reference pattern (§7.4).

### 2.3 Golden-oracle validation — run after ANY `src/` or assembler change

`reference/ASYMB.SYMB` and `reference/BSYMB.SYMB` are the symbol-table dumps
of the original 1978 builds — the oracle. Any change is judged by re-running
the build and re-scoring:

```bash
cd mac-c && ./scripts/build/build_tss_assysa.sh \
  && ./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB | grep 'exact matches'
# Baseline: 679/693 (A), 675/689 (B). Must not regress.
```

The unmatched entries are 13 macro names (real MAC lists them with the macro
body's address inside MAC's own memory image — not reproducible, not to be
faked) plus the known `QOV1C` off-by-1.

**What the oracle cannot see:** matching symbol *addresses* only proves the
word **count** is right. Three real bugs survived it — `COPY SA DT` emitting
`000000`, the `9377` octal-parse bug, and the forward-reference addend bug
(§7.4) — because none changed a word count. There is also **no
instruction-bit oracle in the archive**: the `reference/LIST*.SYMB` listings
are verbatim source echoes with no generated-code column **[VERIFIED]**.
Encodings are therefore pinned by `mac-c/tests/test_mac.c`, which asserts every
instruction's **encoding**, not just corpus totals. See
[`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md).

### 2.4 The build inputs and how they differ from the originals

| File | Purpose |
|---|---|
| `derived/ASSYSA-MAC-INPUT.SYMB` | Version A (SYSA) build — pure MAC command stream |
| `derived/ASSYSB-MAC-INPUT.SYMB` | Version B (SYSB, DEBUG) build — pure MAC command stream |
| `derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB` | NORD-10 + swapping-drum build (enables `XDRUM`) — **the runnable one** |
| `src/TSS1.SYMB` … `TSS5.SYMB` | the five TSS source parts |
| `src/ASSYSA.SYMB` / `ASSYSB.SYMB` | the original command streams, untouched (reference) |
| `src/MINIT.SYMB` / `TDUMP.SYMB` | standalone utilities: mass-storage init, TSS dumper |

**[VERIFIED diffs]** vs the original `ASSYSA`/`ASSYSB` streams:

- Removed from the front (OS-level startup, not MAC input): `FMAC`, `100`,
  `SYSA`/`SYSB` — **[ASSUMPTION]** the command that starts the FMAC
  assembler plus replies to its startup/name prompts.
- Removed from the end: `*:`, `?`, `)9TSS` (A) / `)9TSS`, `@cc` (B) —
  `)9TSS` is not in ND-60.096.01; **[ASSUMPTION]** a site-specific command
  that started TSS.
- Added at the end: `)9EXIT` (documented MAC exit command, for unattended
  runs). Added in A: double quotes around output file names — per the
  manual, `"FILE"` makes MAC *create* the file; the unquoted originals
  required pre-existing files.
- Everything between is verbatim, including the `% Added by CVS` symbol
  patches.

**Route via real MAC (historical/oracle):** the streams can also be fed to
MAC as a SINTRAN III subsystem — `@MAC`, then the `*-MAC-INPUT.SYMB` content
(or a mode file ending `)9EXIT`, run with `@MODE`). Each `)9ASSM` prints a
diagnostics count per part (`**000000 DIAGNOSTICS` = clean **[VERIFIED
manual]**), and the final `)LIST` dumps the symbol table — exactly how the
golden dumps were made.

### 2.5 Drum-build specifics (`ASSYS-DRUM-N10-MAC-INPUT.SYMB`)

- Marks **`DRUM N10`** added: compiles `TRSFR`+`XDRUM` (TSS1 lines
  3711–3874), swapping to a drum at device `IOX 540`, **spilling to the CDC
  disk above 4×`DRMSZ` pages**. `DRMSZ` defaults to `2000` octal 256-word
  pages.
- `INTDS`/`INTEN` pre-definitions removed — under `N10` the source defines
  them itself as `IOF`/`ION` (TSS1:103–106) **[VERIFIED]**.
- Known wart **[VERIFIED]**: `TSS1.SYMB:2658` (the `LEV6` scanner Diablo
  probe) assembles NORD-1 `IOT` words unguarded, even in the N10 build. No
  runtime impact observed on nd100x to date, but it is a latent NORD-1 relic.
- The drum driver's own header calls it a "1. APPROXIMATION" (NJL, 17/4/73).

### 2.6 The memory-image model — why location 7 matters

TSS assembles into memory at fixed addresses (page-zero vectors at 0–7,
overlays staged via `ROVER`) and **saves itself**: location 7 holds
`LDT *+3; JMP I *+1; SYSSV; CORLD` (`TSS1.SYMB:70`) **[VERIFIED]** — the
"binary" is the running memory image saved to disc by `SYSSV`, not a linker
output file. This is also why address 7 is the authentic disk-boot /
cold-start vector (§6.1).

---

## 3. The emulator (nd100x)

### 3.1 Which copy, and how to build it

**The authoritative nd100x checkout is the WSL-native one:**
`~/repos/nd100x` (= `/home/ronny/repos/nd100x`). A second copy at
`E:\Dev\Emulators\ND\nd100x` **diverges** (`config.c`, `nd100x.c` differ) —
build and use the WSL copy, which carries the CDC/OPR/MMS1 work. Do not
modify nd100x without explicit approval (project rule).

```bash
cd ~/repos/nd100x/build && cmake --build . -j4 && ctest
```

**mkptypes gotcha [VERIFIED]:** `tools/mkptypes/mkptypes` is a committed
Windows PE; a WSL build fails with "Exec format error", and CMake does not
rebuild it because the PE is newer than the source. Fix:

```bash
cd ~/repos/nd100x/src/cpu/tools/mkptypes && cc -O2 -o mkptypes mkptypes.c
```

### 3.2 Features added to nd100x for TSS

| flag | purpose |
|---|---|
| `--mms1` | NORD-10 Paging System I (TSS requires it; SINTRAN keeps MMS2) — §4.2 |
| `--drum=FILE` | TSS swapping drum at IOX 540 |
| `--cdc=FILE` | CDC system disc at IOX 500–507 |
| `--opr=OCTAL` | preset the operator's-panel switch register read by `TRA OPR`; e.g. `--opr=131313`. F12 → **[6] Control Panel Switches** edits it live |
| `--start=OCTAL` | override the BPUN autostart entry (e.g. `--start=7`) |
| `--pipe` | keyboard from stdin (expect-style console automation) |
| `--trace` | per-instruction disassembly to stderr (register values are the state at instruction **fetch**) |
| `--debugger --port=N` | DAP debug server (project convention: port **1777**) |

The TSS console is terminal 1 (IOX 300–307) = **DAP terminal 192** = the
local console. It is **not** exposed via `--telnet` (telnet registers
terminals 5–11 only) — drive it locally, via `--pipe`, or over DAP.

### 3.3 The boot-entry contract [VERIFIED]

How nd100x picks the start address: `LoadBPUNStream`
(`src/ndlib/load_bpun.c`) parses the BPUN's octal-ASCII preamble;
`header->start` = the last `/`-opened location, and on `!` it computes
`boot = (loadAddress == start) ? lastValue : loadAddress`. The archived
loader ends `164316!` where `164316` also equals `start`, so an unmodified
tape boots at `lastValue = 0` — for TSS a trap vector (`JMP I 3` → the
monitor-return path) executed with uninitialised state, running off into
zero memory. **The fix is in the tape:** `mac-as -e ENTRY` (octal address or
symbol) replaces the final `164316!` token with `<entry>!`; since
`entry != start`, nd100x reads `boot = entry`. `build_tss_drum.sh` passes
**`-e ISTRT`** (`ISTRT = 025076`, `src/TSS2.SYMB` — `SAA 1; STA I (IDEV;
STA I (ODEV`, falling into `START = 025101` → `LEV2` → `LOGON`). With
`entry == 0` the tape stays byte-identical to the archived format.

`ISTRT` is the *normal-running* re-entry. **Both bring-up boots override it
with `--start=7`** — the real cold-start vector (§6.1). And **[VERIFIED]**
`--start=301` (`INIT` directly) does **not** work — it self-loops at INIT's
`PON; JMP I 1` paging handoff because it skips the page-zero vector setup
the address-7 path performs. 7 works, ISTRT skips SINIT, 301 hangs.

### 3.4 The WSL/DAP rig

```bash
nd100x --mms1 --boot=bpun --image=tss.bpun --cdc=cdc.img --drum=drum.img \
       [--opr=131313] --start=7 --debugger --port=1777
```

- Connect a DAP client to `127.0.0.1:1777` (port 1777 is the project
  convention, chosen to avoid clashing with other tooling on 4711).
- Enable console capture on **terminal 192**, attach, set breakpoints,
  continue.
- Send console input as **raw hex bytes** — a carriage return must be the
  byte `0x0D` (a literal `\r` is echoed as backslash-r):
  `SYSTEM`+CR = `hex 53 59 53 54 45 4D 0D`; `4470`+CR = `hex 34 34 37 30 0D`;
  `I`+CR = `hex 49 0D`.
- Always run on **copies** of the disc images.
- Stopping: with the write-through nd100x (§1.4) any stop — SIGINT (`make stop`),
  DAP terminate, or a crash — persists CDC/drum writes, because each sector is
  written to the image immediately. (On an *unpatched* nd100x only a clean DAP
  `disconnect{terminateDebuggee}` flushes CDC/drum; SIGINT would lose them.)

`make dap-format` / `make dap-coldstart` / `make dap-login` launches each stage
pre-configured on port 1777. For a fully-scripted, self-terminating run of the
whole format + cold-start (the reliable persist path), use **`make auto`**, which
drives `bringup/dap_bringup.py` (a minimal DAP client that auto-answers the
prompts and terminates cleanly).

---

## 4. The three fixes that made TSS *run* (not just assemble)

Before these, TSS never reached user code. All three were needed
**together**. They are history now — already applied — but each is a class
of bug the golden dumps could never catch, so they are kept on record.

### 4.1 `EXRGP` — a resident variable missing from the recovered source

`EXRGP` (an EXR register-save pointer, LEV14 family) is referenced but
**never defined** in `src/`, so its literal `(EXRGP` resolved to **0**. The
unconditional `STZ I (EXRGP` at `S10` (the `TSS1.SYMB:3181` region) then
stored 0 **to address 0**, destroying the GOVER page-0 dispatch vector — so
no overlay ever loaded. **Fix:** restore `EXRGP, 0` in the `%LEV14
VARIABLES` block of `src/TSS2.SYMB:106-124`, **before `SAVEX`**, gated
`"N10` so the NN10 golden dumps stay byte-identical. Value 0 (a pointer,
cleared per context at `S10`). This is an incompleteness in the recovered
source, the same class as CVS's unfinished `STR→XTR` rename.

### 4.2 MMS1 paging, not MMS2

TSS is NORD-10 code: it programs **Paging System I** — single 16-bit
page-table words written to `0177400`, then `PON` (`TSS1.SYMB:306-318`).
nd100x defaulted to MMS2 (16 page tables, 32-bit entries), under which TSS's
page-table writes are meaningless. **Fix:** nd100x gained a per-machine
`mms` setting — use **`--mms1`** for TSS. SINTRAN keeps MMS2.

### 4.3 CDC control-word operation decode — the subtle one

The overlay read is issued by `DKOP`/`DKTR` (`TSS1.SYMB:3458`, "CDC N10").
With the mac-c SHR shift bug fixed, `DKTR` builds the *manual-correct* read
control word **`000004`**: the device **operation is a plain 2-bit field at
control-word bits 11–12** (00 read / 01 write / 10 read-parity / 11
compare), per the ND manuals (ND-11.008.01 / ND-06.016.01). nd100x's CDC
device (`src/devices/cdc/deviceCDC.{c,h}`) decodes exactly that:
`op = control.bits.deviceOperation` (bits 11–12); a read (`000004`) → op 0 →
disc→core DMA into `ROVER`.

> **History / trap:** an earlier device build decoded the op as an
> `(op+1)<<12` field at bits 12–14 (read word `010004`). That model was an
> **artifact of the mac-c SHR compiler bug**, which had made `DKTR` emit
> `010004`. With SHR fixed, `DKTR` emits `000004`, and the `(op+1)<<12`
> model wrongly decodes it as op 3 (compare) — the device sets
> `compareError`, never DMAs, and the boot hangs in `DWAIT` (status
> `062024`). The correct, current model is **bits 11–12**; the full
> derivation lives in the `deviceCDC.h` NOTE.

---

## 5. Disc preparation and MINIT

### 5.1 Pre-sizing the CDC image [VERIFIED]

The emulated CDC device **does not grow on writes** — it bounds every
transfer against the backing-file size (`deviceCDC.c`, the
`wordOff > surfaceWords` check). The TSS user area (`FSYS`, NCR `4470`) maps
to a high physical sector (≈ 6288 decimal), while the raw `tss-cdc.img` is
only ~459 sectors — far too small, and MINIT gets `DISK ERROR` on every user
track. **Pre-size the backing file:**

```bash
cp Build/drum/tss-cdc.img  <work>/cdc.img
truncate -s 4M             <work>/cdc.img     # 8192 sectors — covers the user area
head -c 1048576 /dev/zero > <work>/drum.img   # blank swap drum
cp Build/drum/tss-drum.bpun <work>/tss.bpun
```

Padding only appends zeros; the overlays (physical sector ~296+) are
untouched. **[VERIFIED]** an 8192-sector image lets MINIT format NCR
`4470`–`4670` cleanly. `make prepare` does all of this.

### 5.2 What MINIT is

`src/MINIT.SYMB` — the **"MASS STORAGE INITIALIZATION PROGRAM"** (line 1) —
is a **standalone operator tool**, not part of the TSS OS image. It lays down
the disc-resident free-track bitmap (**MIB**) that the kernel's track
allocator `GTRK` later consumes, so that the first cold boot's `SINIT` can
create user SYSTEM. It is self-contained: its own TTY I/O, octal I/O, message
printer, and a full copy of the CDC disc driver (`DKTR`/`DKOP`/`DKADR`/
`DWAIT`); NORD-1 vs NORD-10 code selected by library marks. It builds via
`mac-c/scripts/build/build_minit.sh` to `Build/minit/minit.bpun`, image base `000000`,
entry `MINIT = 000206`, 0 errors. **[VERIFIED]**

Source structure map **[VERIFIED]** (`src/MINIT.SYMB`, 810 lines):

| Lines | Content |
|---|---|
| 1–2 | Title; CVS-added equates `IOT/ACT/SKA/PIN/SNI` |
| 8 | Bare `CDC` mark — selects the CDC-disc code paths |
| 11–15 | `NN10` / `"N10 )KILL NN10 "` — "not-NORD-10" mark, killed when `N10` is set |
| 24–45 | `TCI` (TTY char in), `TCO` (char out) |
| 49–71 | `CRLF`, `TCF` (I/O error → `I/O ERROR`, `WAIT`, restart) |
| 75–94 | `MSG` (print packed-string message) |
| 98–132 | `OCTIN` / `OCTOT` (octal read/print) |
| 134–221 | **`MINIT` main + branches `M1..M5`, `MSS`, fault handlers `MF1..MF3`, `MUP`, `ER1`, `REG..REG6`** |
| 224–225 | `MASKS` table — the 16 single-bit masks `1,2,4,…,100000` |
| 228–243 | Prompts `MS1..MS6` and error strings `ERM1..ERM8` |
| 246–265 | `SBIT` — set/clear one bit in the MIB |
| 267–304 | Device equates; `DCHN`, `DISC`, `LCA/LBA/LMR/RST/…` |
| 307–327 | `DWAIT` — disc ready/timeout |
| 333–360 | `DKOP` — one 256-word disc operation |
| 364–519 | `DKTR` — the full transfer driver |
| 526–587 | `DKADR` — NCR→CDC address conversion, `DKLIM=626` |
| 590–796 | NORD-10 (`"N10`) I/O library: `INBT`, `OUTBT`, `IOINI` |
| 798–805 | NORD-1 (`"NN10`) I/O: `IOT` polling |
| 808–810 | `MIB=*`, `BUFF=MIB+4000`, `)LINE` |

### 5.3 The MIB bitmap — geometry and polarity

**Placement [VERIFIED]** `MINIT.SYMB:808-809`: `MIB=*` (immediately after
the code) and `BUFF=MIB+4000` — two `4000`-word (2048-decimal) regions. The
`M1` loop zeroes `MIB[0..4000)` (`:143-144`); `M3` zeroes `BUFF` likewise.
**[ASSUMPTION]** 2048 words × 16 bits = 32768 track bits, indexed by NCR
address.

**MASKS [VERIFIED]** `:224-225`: `MASKS[k] = 1<<k` for bit `k` of a 16-bit
word.

**Bit addressing [VERIFIED]** `:165-167` (INITIALIZE success path):

```
LDA ADRI; SAD SHR 7; ADD (MIB; STA MIBX      % word index = ADRI >> 7, + MIB base
SAA 0; SAD 4; ADD (MASKS; COPY DX SA         % bit index  = (ADRI >> 4) & 17
LDA 0,X; ORA I MIBX; STA I MIBX              % set that bit
```

and `:168`: `LDA ADRI; AAA 10; STA ADRI` — **NCR addresses advance by `10`
(8 decimal) per track**; one track = 8 NCR units. **[ASSUMPTION]** the low 3
bits of an NCR address are the within-track sector. The same math is
centralised in `SBIT` (`:252-259`), which takes `T = disc address`,
`A = 1 (set) / 0 (clear)`.

**Polarity [VERIFIED, both from source and live]: a SET bit means the track
is GOOD and FREE; a clear bit means BAD or in use.** The MIB starts all-zero
(`M1`); the *only* place INITIALIZE sets a bit is the success path
`:165-167`; both disc-fault paths (`MF1`/`MF2` → `M5`, `:173-182`) jump past
it, leaving a bad track's bit 0. This agrees exactly with the kernel's
allocator: `GTRK` (`src/TSS2.SYMB:2663-2682`) scans for a word `≠0` (`:2671`
`JAZ G4` skips fully-allocated all-zero words) and **claims a track by
CLEARING its bit**. Finally `M5` (`:170-171`) writes the MIB itself to disc:
`LDX (MIB; SAT 50; SAA 7; … JPL I (DKTR` — at NCR address **`50`**
(`DKBIT`).

### 5.4 DKADR — NCR → CDC address conversion [VERIFIED]

`MINIT.SYMB:526-587`. Input `A` = NCR disc address, output `A` = CDC
address; failure return if illegal. Key constant (`:534-535`):

```
DKLIM=626    % NUMBER OF CYLINDERS (313 FOR CDC 9425 AND 626 FOR CDC 9427)
```

— this build targets a **CDC 9427** (626 cylinders). The conversion:

- masks NCR to 14 bits (`AND (17777`, `:541`),
- **divides by 12 decimal** for the cylinder field (NORD-10 path
  `RDIV ST` with `SAT 14`, `:550-551`; the NORD-1 path multiplies by the
  reciprocal `52525`/952, `:546-548`),
- multiplies the remainder by 14 (`MPY (14`, `:561`) for the sector count,
- adds a per-unit displacement `52600` if a unit-select bit is set (`:565`),
- range-checks against `DKLIM` (`SUB (DKLIM; JAP DKADF`, `:571`).

**[ASSUMPTION]** ÷12 = cylinder, ×14 = sector, consistent with CDC 9427
geometry; the exact heads/cylinder and sectors/track are not named constants
in this file.

The TSS kernel's own driver uses the equivalent repack — as built (SHR
fixed): **`DKADR(L) = 32·⌊L/12⌋ + 2·(L mod 12)`** (`mac-c/mac.c
cdc_dkadr`, verified live) — a base-12 "track·32 + 2·sector" map. That
formula is how logical structures land at physical sectors: MIB `50` →
physical `0150` (104 dec), overlays `0160+` → physical ~`0450+` (296+),
`USRDK 1460` → physical `4200` (2176), `FSYS 4470` → ≈ 6288. The nd100x CDC
device is a dumb linear-by-physical-sector store.

Supporting routines: **`DWAIT`** (`:312-327`) polls the ready bit
(`IOX RST; BSKP ONE 20 DA`) with a timeout; **`DKOP`** (`:333-360`) does one
256-word operation; **`DKTR`** (`:364-519`) is the full transfer — wait
ready, load `LCA`/`LBA`, convert via `DKADR`, load modus/word-count, start,
wait, check status; up to 3K-word multi-block transfers. It is the same
driver the OS uses; MINIT carries its own copy so it can run bare.

### 5.5 The overlay-on-disc subsystem (what is already on the disc)

- Overlays live on the **CDC system disc** (IOX 500–507), not the drum
  (540). Registers: `500` RCA, `501` LCA (core addr), `502` RSECT, `503` LBA
  (block), `504` RST (status), `505` LMR/LCW (control), `506` SEEK, `507`
  LWC (count). IDENT 1, interrupt level 11.
- Logical sector `= OVDK + 2·OVLAY` (`OVDK = 0160` non-DEBUG); each overlay
  is two 256-word sectors, read into `ROVER` (`031400`) and
  `ROV4 = ROVER+400`.
- The mac-c overlay writer (`mac_write_cdc_disc`, `src/mac_bpun.c`) writes
  **overlays only**, each at its `DKADR` physical sector. Everything else on
  the disc — MIB, user tables, per-user tracks, the saved system image — is
  written by MINIT and by TSS itself at runtime (§6.3).

### 5.6 The MINIT operator dialogue [VERIFIED]

Prompts and errors (`MINIT.SYMB:228-243`; `$` = CR/LF in the packed
strings):

| Sym | Line | Text |
|---|---|---|
| `MS1` | 228 | `MASS STORAGE INIT` / `FIRST DISK ADDRESS (NCR): ` |
| `MS2` | 229 | `LAST DISK ADDRESS (NCR): ` |
| `MS3` | 230 | `INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): ` |
| `MS4`/`MS5`/`MS5A` | 231–233 | echo tails `NITIALIZE` / `PDATE` / `EGENERATE` |
| `MS6` | 234 | `FINISHED` |
| `ERM1` | 236 | `ILLEGAL ADDRESSES` (LAST ≤ FIRST) |
| `ERM2`–`ERM5` | 237–240 | `DISK ERROR ` ` AT NCR(` `) CDC(` `)` |
| `ERM6` | 241 | `TRANSFER ERROR` |
| `ERM7` | 242 | `UNABLE TO WRITE MIB` |
| `ERM8` | 243 | `NOT IMPLEMENTED` |
| `TCFS` | 69 | `I/O ERROR ` |

Entry flow (`:137-148`): print banner + FIRST prompt, read octal, **`AND
(77770`** (round down to an 8-unit track boundary); same for LAST;
`LAST ≤ FIRST` → `ILLEGAL ADDRESSES` and restart; zero the MIB; prompt
I/U/R (any other character re-prompts).

The three branches:

- **I(nitialize) → `MIT`** (`:149-172`) — destructive surface test: for each
  track in FIRST..LAST, write zeroed test data, read it back, verify all-zero
  (`M4` `:163-164`, any non-zero word → `MF2`). Success sets the track's MIB
  bit; disc faults print `DISK ERROR … AT NCR(n) CDC(n)` and leave the bit
  clear. Then write the MIB to NCR `50` and print `FINISHED`.
- **U(pdate) → `MUP`** (`:184`) — **a stub**: prints `NOT IMPLEMENTED` and
  restarts. **[VERIFIED]** Any real procedure must use `I` (or `R`).
- **R(egenerate) → `REG`** (`:192-214`) — non-destructive rebuild: marks the
  whole range allocated, then walks the user table (`340` user slots, base
  `51`, `70` files per user — loop bounds read from `:197-212`, deeper
  meaning **[ASSUMPTION]**) and each user's UIB / file / track chains,
  re-setting the bit of every free track, then writes the reconstructed MIB
  back. Use to repair a corrupted bitmap without data loss.

### 5.7 The production range: `4470`–`4670` [VERIFIED]

The free-track pool must start at **`FSYS = 4470`** — the start of user
storage. Everything below is reserved and must stay marked "used":

| structure | NCR addr |
|---|---|
| MIB / `DKBIT` | `50` |
| overlays | `160` / `360` |
| password table `USRDK` | `1460` (`= MAILD+MAILS+400`) |
| the 12 swap areas | `1470`–`4270` |
| user storage `FSYS` | `4470`+ |

`4470`–`4670` stepping `10`/track = **16 free tracks** (a functional range;
widen LAST for more users). **[VERIFIED]** after `FINISHED`, the MIB at disc
`50` (physical sector 104) holds word 18 = `177600`, word 19 = `177` — 16
set bits = 16 free tracks. Track math: MIB bit index = NCR/8, so FIRST
`4470` marks track `4470/8 = 447` (295 dec) onward — exactly what `GTRK`
allocates from.

### 5.8 The verified live MINIT run

MINIT booted on nd100x under DAP (`--boot=bpun --image=minit.bpun
--cdc=cdc.img --debugger --port=1777`) **[VERIFIED]**:

1. The BPUN autostart landed the CPU exactly at `P = 000206` (the MINIT
   entry) — the base-0 image and `-e MINIT` autostart work.
2. The console printed the full dialogue and completed with `FINISHED` — the
   nd100x `--cdc` device at `DCHN = 500` answers MINIT's `DKTR`/`DWAIT`
   correctly; no `UNABLE TO WRITE MIB` / `I/O ERROR`.
3. A functional-test format of `0`–`2000` produced **exactly one non-zero
   sector**, at physical sector `0150` (104 dec), containing 128 set bits —
   `1024/8 = 128` tracks. Exact match; this empirically pinned both the
   **set = free** polarity and the `DKADR` mapping of NCR `50` → physical
   `0150`.

---

## 6. Cold-start and SYSTEM creation

### 6.1 Enter at address 7 — the authentic cold-start vector

The BPUN's own autostart is `ISTRT` (`src/TSS2.SYMB:1863`), the
*normal-running* re-entry (`START` → `LEV2` → `LOGON`); it never runs the
operator-switch test. **Address 7 runs the real cold-start** — the
`7/ LDT *+3; JMP I *+1; SYSSV; CORLD` disk-boot vector (`TSS1.SYMB:70`).
See §3.3 for why `--start=301` also fails. [VERIFIED]

```bash
nd100x --mms1 --boot=bpun --image=<work>/tss.bpun --cdc=<work>/cdc.img \
       --drum=<work>/drum.img --opr=131313 --start=7 --debugger --port=1777
```

### 6.2 The operator-switch decode [VERIFIED]

`src/TSS1.SYMB:3180-3182`:

```
S10, ...
     TRA OPR; SUB (131313; JAF *+3; LDA (SINIT; JMP S99   % OPR = 131313 -> run SINIT
     LDA I (AVAIL; JAP *+3; LDA (NOTUP; JMP S99           % system not available -> NOTUP
     LDA (LEV2                                            % otherwise: normal boot
S99, ...   (start the chosen routine as the first process)
```

| `OPR` (octal) | effect |
|---|---|
| **`131313`** | Cold init: dispatch `SINIT`, which **creates user SYSTEM**. Use once, on a freshly MINIT-formatted disc. |
| anything else | Normal boot: dispatch `LEV2`; every terminal ends at `@ENTER`. (If `AVAIL` is negative, `NOTUP` instead.) |

On nd100x: `--opr=131313` presets the `TRA OPR` value; omit it for normal
boots. **[VERIFIED]** live: with `--opr=131313`, `TRA OPR` returns `131313`
at the `S10` site, the `SUB (131313` yields 0, and the branch falls through
to the SINIT dispatch.

### 6.3 What writes the CDC disc during bring-up [VERIFIED]

The disc's *system* structures are written by TSS itself at runtime, by four
distinct routines writing four distinct regions:

| Routine | Source | Trigger | Disc region written |
|---------|--------|---------|---------------------|
| `SYSSV` (save system) | `TSS1.SYMB:4108` | the address-7 boot vector | `0100` blocks of the resident system image (`XDISK` op 3), from disc addr `SYST` |
| `FCRUS` (create user) | `TSS5.SYMB:74` (overlay OV10) | `OPR=131313` → `SINIT` → `CRUSE` | the **user table** at `DKBIT+index` (`RUTBL`/`WUTBL`, `TSS2.SYMB:3048/3070`) |
| `IUSER` (init user) | `TSS5.SYMB:127` | called by `FCRUS` | the new user's private directory track (`RBLOC`/`WBLOK`, addr from `UNDK`) |
| `GTRK` (get track) | `TSS2.SYMB:2665` | called by `FCRUS` | the free-space bit table (MIB) at `DKBIT` |

`FINIT` (`TSS1.SYMB:4714`), called unconditionally at every cold start,
initialises the **in-core** file-system tables only (`BITBL`, `USTBL`,
`PCTBL`, `VCTBL`, …) via `SETB` — no disc I/O.

### 6.4 The two on-disc user tables — and why SYSTEM needs no password

| table | disc address | holds | written by |
|---|---|---|---|
| **`USTBL`** | `DKBIT+idx` (`DKBIT=50`, so ≈ disc `51`) | user **names** + track ownership | `CRUSE`/`FCRUS` — i.e. **`SINIT`** and the `CRUSR` command |
| **`USRDK`** | `1460` | the **password** word per user (entry offset 7) | `CRUSR`, `PASWD`, `CPASW` only |

**A brand-new disc's `USRDK` is empty, and that is correct** — an empty
password word means **passwordless**, exactly what the primordial SYSTEM
needs. There is no "seed USRDK" step; the login *name* comes from `USTBL`,
which `SINIT` writes. `SINIT` **[VERIFIED]** `src/TSS2.SYMB:858-862`:

```
SINIT, LDX (SINQ; RCLR DD; JPL I (CRUSE; JMP *+1
       JMP I (LEV2
SINQ,  #SY; #ST; #EM; 0; 0; 0; 0        % the name "SYSTEM", packed 2 chars/word
```

`CRUSE`/`FCRUS` then: (1) `RUTBL` — read the user/track table from disc,
(2) find a free slot and `GTRK` a free track (needs MINIT's MIB), (3) store
the name + track and `WUTBL` it back, (4) `IUSER` the new user's tracks.
Result: SYSTEM exists in `USTBL`, owns a track, user number 1, no password.

**Why the order MINIT → SINIT → LOGON is mandatory:** on a disc that never
saw MINIT the MIB is all zeros (no free tracks); `GTRK` fails, `CRUSE`
returns "no more tracks", `SINIT` falls through to `LEV2` having created
**nothing**, and `LOGON` silently re-prompts forever — exactly the failure
observed when cold-booting `--opr=131313` on the raw overlay-only image.

### 6.5 The verified cold-start chain

| step | addr | source | action |
|---|---|---|---|
| disk-boot vector | **7** | `TSS1.SYMB:70` | `LDT *+3; JMP I *+1; SYSSV; CORLD` |
| `SYSSV` | 010603 | `TSS1.SYMB:4108` | saves the core image to disc, → INIT |
| `INIT` | 000301 | `TSS1.SYMB:252` | vectors, tables, N10 paging, `ION` |
| `LEV5` → `SWAPR` → `SYY` | 006443 / 07270 | `TSS1.SYMB:2794/3173` | schedules the first process |
| `S10` → OPR test | 07271 / **07307** | `TSS1.SYMB:3175/3180` | `TRA OPR; SUB (131313; JAF *+3; LDA (SINIT` |
| `SINIT` | 022057 | `TSS2.SYMB:858` | create SYSTEM (calls `CRUSE`) |
| `CRUSE`/`GTRK`/`WUTBL` | 031272 | `TSS5.SYMB:74` | allocate a track, write the name into `USTBL` |
| `LEV2` → `LOGON` | — | `TSS2.SYMB:1865` | every terminal prints `@ENTER` |

**[VERIFIED]** live: the `CRUSE` breakpoint (031272) fires and `@ENTER`
prints on the console.

### 6.6 Verifying SYSTEM on disc [VERIFIED]

Stop the emulator with SIGINT (flushes the surface), then inspect the image
— `bringup/verify-disc.py` automates this; by hand:

```python
d = open("cdc.img","rb").read(); b = 104*512
mib = d[b:b+512]
print("MIB set bits:", sum(bin(x).count("1") for x in mib))                 # 15  (was 16)
print("SYSTEM on disc:", b"\x53\x59\x53\x54\x45\x4D" in d[106*512:107*512]) # True
```

**[VERIFIED] result:** MIB set bits **16 → 15** (`GTRK` claimed one track by
clearing its free bit) and the name `SYSTEM` appears in the user-table
region at physical sector 106. That is the account, on disc.

---

## 7. Login

### 7.1 The verified sequence

Normal boot (omit `--opr`, keep `--start=7`); each terminal prints the login
prompt. **[VERIFIED]** end to end:

```
@ENTER SYSTEM                       <- MS1 (TSS4.SYMB:539: '$$@ENTER \ '); type the name
PROJECT NUMBER P-1                  <- needs a POSITIVE number; 0/blank re-prompts
AMBIGUOUS FILENAME                  <- non-fatal on a fresh disc (no ()SCRATCH yet)
TYPE IN DATE (DD,MM,YYYY,HH,MM,SS)  <- first login only; e.g. 23,07,2026,15,30,00
@                                   <- the command processor (XSTAR)
```

At `@`: `HELP` lists ~60 commands; `WHO-IS-ON` prints `1 SYSTEM`.

Prompt notes:
- `@ENTER` is message `MS1` in the LOGON overlay (`src/TSS4.SYMB:539`;
  `$` = CR/LF, `\` = terminator).
- SYSTEM is **passwordless**, so there is no `PASSWORD` prompt (§7.2).
- `AMBIGUOUS FILENAME` comes from LOGON's attempt to open the `()SCRATCH`
  file, which does not exist yet on a fresh disc; login continues.

### 7.2 How authentication works [VERIFIED]

`LOGON` (`src/TSS4.SYMB:557-562`):

```
L4, LDX (USRTB; LDT (USRDK; SAA 1; JPL I (XDISK; JMP *-4          % read USRDK -> USRTB
    LDX CPTR,B; LDT (USARR; SAA 1; COPY DD SA; JPL I (ABLKP; JMP L2  % match the name
    JMP L2; STA UNO,B; AAA -1                                     % UNO = user number
    RCLR DD; SAD SHR 5; COPY DX SA; SAA 0
    SAD 10; ADD (USRTB; RSUB DA SX; STA OBJ,B                     % entry = USRTB + UNO*8
    SAX 7; LDA I OBJ,B,X; JAZ LOGS                                % offset 7 = password; 0 -> LOGS
```

The crucial detail: the **name match** (`ABLKP`) uses `USARR` as its
string-array accessor, and `USARR` reads each name from **`USTBL`**, not
`USRDK` (**[VERIFIED]** `src/TSS3.SYMB:660-661`: `ADD (USTBL … LDT (USTRG;
SAA 7; JPL I (BCOPY`). So: (1) LOGON reads `USRDK` (passwords) into
`USRTB`; (2) `ABLKP`→`USARR` finds the typed name in `USTBL` (written by
`SINIT`) and returns the user number; (3) the password word at
`USRTB[user][7]` is **0** for a fresh SYSTEM → `JAZ LOGS` → passwordless
login succeeds. Passwords come later via the `PASSWORD` command (`PASWD`,
writes `USRDK`); SYSTEM creates more users with `CRUSR` (writes both
tables) and clears passwords with `CPASW`.

### 7.3 Overlay nesting during login (why it works)

The login path is a nested overlay call — `LOGON` (OV6) → `ABLKP` (OV1A) —
and this is by design: `STK` saves the caller's overlay per frame in `XOVL`,
`USTK` restores and reloads it. Overlays execute in place in `ROVER`
(031400); the `40000` "VOR window" is build-time staging only. **[VERIFIED]**
live: GOVX raises level 5, `S5` reads OV1A (SDISK T=`0162`/`0163` →
`ROVER`/`ROV4`), and the process resumes into the new overlay.

### 7.4 The login-hang history (one paragraph)

Two mac-c silent-miscompile bugs each once made login impossible, and both
were invisible to the golden dumps because word counts were unchanged: the
`9377` octal-parse bug (all-digit symbols containing 8/9 parsed as octal →
`AND 9377` became `AND 0`, zeroing every console character), and the
**forward-reference addend bug** — fixups discarded the constant part of
`JMP RFN+2`-style operands, which broke `ROBJ`'s error exits, turned "index
out of range" into "empty object", and sent LOGON's `()SCRATCH` directory
enumeration into an endless busy-loop after the project number. Both are
fixed and regression-pinned (tests [7] and [15]);
`bringup/check-robj-encoding.py` byte-checks any built image for the fixed
pattern. Full defect records: [`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md).

---

## 8. Validation and debugging

### 8.1 The tool inventory

| tool | where | what it does |
|---|---|---|
| `verify-disc.py` | `bringup/` | reads `Build/bringup/cdc.img` (or a given path); reports the MIB free-track count and whether SYSTEM is in the user table, and says which bring-up step to run next |
| `check-robj-encoding.py` | `bringup/` | byte-checks a built BPUN/image for the fixed vs broken ROBJ forward-reference pattern — **must print FIXED** (stale-assembler detector, §2.2) |
| `make stop` | top-level `Makefile` | SIGINTs the bring-up emulator — the safe stop that flushes the CDC surface |
| `make dap-format` / `dap-coldstart` / `dap-login` | top-level `Makefile` | launches a bring-up stage under the DAP debugger on port 1777 |
| `verify_repo.sh` | `mac-c/` | everything at once: layout, build, tests, coverage, both TSS builds, oracle scores, markdown links |
| `compare_asymb.sh` / `first_divergence.sh` | `mac-c/` | score a symbol dump against the golden oracle / find the first diverging symbol |
| `MACTRACE=1` | mac-c env | the assembler prints which line ended each source stream and the conditional state on exit — the tool for "the build silently stopped early" |
| `--trace` | nd100x | per-instruction disassembly to stderr |

### 8.2 Trace recipes (`--trace 2>trace.txt`)

Diagnostic greps for the CDC/boot path:

```
IOX 505     control word (000004 = read)      IOX 503    disc block address
IOX 501     DMA core address (031400 = ROVER)  IOX 504    CDC status
IOX 305     console character out              P=032736   LOGON reached
```

A healthy boot: `INIT` → scheduler `LEV5` → `S10` → dispatches `LEV2` →
`ISTK; IBUF; SXBRK`, start-up banner via `ERMSG` (= `GOVER OV15`), then
`LOGON`. Overlays load from CDC into `ROVER`: control word `IOX 505 =
000004`, OV15's body (`031400 005003`) executes, multiple distinct `IOX 503`
sectors as more overlays load. **[VERIFIED]** live.

Debugger caveat: idle-loop instructions can read as zeros under the DAP
default page view — inspect with the executing PIL context.

### 8.3 DAP driving

See §3.4 for the rig. Useful breakpoints: `CRUSE` (031272) to confirm
account creation, `GTRK` to confirm track allocation succeeds after MINIT,
`LOGON` (032736) to watch authentication.

### 8.4 Key addresses (octal) — quick reference

Re-read from `Build/drum/DSYMB.SYMB` after any rebuild (§2.1); these are the
values of the verified build:

| symbol | addr | role |
|---|---|---|
| boot vector | `7` | `LDT *+3; JMP I *+1; SYSSV; CORLD` — the cold-start entry (`--start=7`) |
| `MINIT` | `000206` | MINIT formatter entry (own BPUN) |
| `INIT` | `000301` | cold-start init (do **not** enter here directly — hangs) |
| `WBUF`/`RBUF` | `000610`/`000702` | teletype ring in/out (where the `9377` bug bit) |
| `LEV0` | `006104` | idle/background wait loop |
| `LEV5` | `006443` | scheduler/swapper |
| `S10` / OPR test | `07271` / `07307` | cold-start dispatcher / `TRA OPR` compare |
| `DKTR` | `007651` | CDC transfer builder (control word `000004`) |
| `DKADR` | `010006` | logical→physical disc address |
| `DWAIT` | `010313` | disc-wait poll |
| `SYSSV` | `010603` | save-system-to-disc |
| `GOVX` | `016457` | overlay dispatcher |
| `XDISK` | `021137` | kernel disc I/O entry |
| `SINIT` | `022057` | create-SYSTEM cold-start routine |
| `ISTRT` | `025076` | BPUN tape autostart — normal-running entry (skips the OPR test) |
| `START` | `025101` | normal system entry (`LEV2` → `LOGON`) |
| `ROVER` | `031400` | overlay load/execute target |
| `CRUSE` | `031272` | create user (OV10) |
| `USARR` | `032430` | name accessor — reads `USTBL` (OV1A) |
| `ABLKP` | `032444` | abbreviation/name matcher (OV1A) |
| `LOGON` | `032736` | login procedure (prints `@ENTER`) |

Disc addresses (NCR): `MIB/DKBIT` `50` · `USTBL` ≈ `51` · overlays `160`+ ·
`USRDK` `1460` · swap areas `1470`–`4270` · `FSYS` `4470`.

### 8.5 Troubleshooting

| symptom | cause | fix |
|---|---|---|
| MINIT prints `DISK ERROR` on every track | CDC image too small (device does not grow) | re-run `make prepare` (pads to 8192 sectors) |
| cold-start never shows `@ENTER` | started at ISTRT, not 7 | ensure `--start=7` (the scripts set it) |
| no console echo at all | wrong console/terminal | the TSS console is terminal 192 / the local console; check `--start=7`; not reachable via `--telnet` |
| `verify-disc.py` shows 16 free after step 3 | SINIT never ran | you booted without `--opr=131313` or not at addr 7 |
| login loops silently after the project number | stale `mac-as` (`make test` does NOT relink it) | run plain `make` in `mac-c/`, rebuild images, re-run steps 1–3; `check-robj-encoding.py` must say FIXED |
| boot hangs in `DWAIT`, CDC status `062024` | wrong CDC op-decode model (compare instead of read) | the device must decode the op at control-word bits 11–12 (§4.3) |
| WSL nd100x build fails "Exec format error" | stale Windows-PE `mkptypes` | rebuild it: `cc -O2 -o mkptypes mkptypes.c` (§3.1) |
| disc changes lost after a run | emulator not stopped with SIGINT | always Ctrl-C / `make stop` — that flushes the CDC surface |
| build "silently stopped early" | a conditional swallowed the rest of a stream | `MACTRACE=1` shows which line ended each stream |

### 8.6 Config

The bringup scripts accept environment overrides:

```bash
make coldstart ND=/path/to/nd100x DAP_PORT=1780
```

Working images go to `Build/bringup/` (disposable; re-created by step 1).

---

## 9. Known issues

Exactly **one** open bug:

- **DATE garble (cosmetic, untriaged).** After the first-login date entry,
  the `DATE` command prints garbled fields — observed
  `DATE IS 139 JULY 2026   159210:48`. Date conversion/formatting in the
  DATE path; does not affect login, commands, or the file system. Not yet
  investigated.

Not bugs (by design / correct for this corpus):

- `AMBIGUOUS FILENAME` at first login on a fresh disc (§7.1) — non-fatal,
  disappears once `()SCRATCH` exists.
- MINIT's `U`(pdate) branch is a stub — `NOT IMPLEMENTED` (§5.6).
- 20 symbols undefined after a full build, 13/14 oracle mismatches being
  macro names — see the corpus notes in the project docs.
- `TSS1.SYMB:2658` assembles NORD-1 `IOT` words unguarded in the N10 build
  (§2.5) — latent wart, no observed runtime impact.

---

## 10. Cross-references

| doc | what it holds |
|---|---|
| [`../bringup/README.md`](../bringup/README.md) | the quick script-side guide: the four steps, prompt meanings, DAP driving, troubleshooting |
| [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) | how TSS itself works: scheduler, levels, overlays, drivers, file system |
| [`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md) | the historical findings and the recovery story |
| [`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md) | the mac-c assembler: semantics, defect records (9377, SHR, forward-ref addend, CLD, STR→XTR), validation methodology |
| [`../mac-c/README.md`](../mac-c/README.md) | building and testing the assembler; the script inventory |
