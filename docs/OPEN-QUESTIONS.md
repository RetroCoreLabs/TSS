# Open questions — NORD TSS 3.0

**This file:** `docs/OPEN-QUESTIONS.md`
**Read first:** `CLAUDE.md` — the standing rules override
everything here.

The live list. This file replaces the dated session handoffs: when an item is
answered it moves into the document that owns the subject, and it is deleted
from here rather than accumulating a status history. Answered items are not
kept here — the evidence lives in the owning document.

Current as of 2026-07-28. Repository state: `mac-c` 718 assertions / 0
failures, golden oracle 679/693 (A) and 675/689 (B), 0 assembly errors, all 60
commands exercised with 87 of 89 invocations returning.

---

## 1. Which floating-point format was TSS built for?

**Own document:** `docs/TSS-FLOAT-FORMAT.md`

`TBANG`'s constants are 2-word 32-bit; `FPDAT` and `NORM` require 48-bit. The
consequence is that the time of day cannot advance. Confirmed by patching `K1`
in memory; **not** a `mac-c` defect, and not to be "fixed" there.

**ANSWERED 2026-07-28 — no longer an open question.** The NORD-10/S Reference
Manual §2.5.2.6 (`ND-06.008.01`, TSS's own machine generation) lists the
instructions the 32-bit FPP option affects: **`FAD`, `FSB`, `FMU`, `FDV`,
`NLZ`, `DNZ`**. `LDF`/`STF` are not among them, and ND-110 RASK / ND-120
DELILAH-L microcode agree. TSS uses `STF`/`LDF` throughout, so it is a 48-bit
program and its `TBANG` constants were **wrong in the 1978 build too** — the
archived system's clock could not have advanced on real hardware either.

Remaining is historical curiosity only: how the wrong constants got there.
Kept in this list solely as a pointer to the document.

## 2. `QOV1C` — a one-word oracle mismatch

Golden `000642`, produced `000641`. An `OVERX` overlay-size symbol, and one of
the two "extra" entries in the version-A score. The `-L` listing makes the
`)9MOVE`/`VOR` arithmetic inspectable for the first time; that is the route in.

Not started.

## 3. `TDUMP` has never executed

`Build/tdump/tdump.bpun` assembles with zero diagnostics
and was mounted as paper tape for the `PLACE-BINARY`, `RBLOAD` and
`LOAD-BINARY` phases of the command sweep. All three returned cleanly; none
actually loaded and entered TDUMP.

What is known: the tape-reader route is reachable and does not hang. What is
missing: the correct operator sequence to load a BPUN from tape and transfer
control to it. `PLACE-BINARY` ignores its command-line argument and prompts,
and the prompt path returns `BAD FILENAME`.

Related and unexplained: **`PLACE-BINARY` and `LOAD-BINARY` reject
`TAPE-READER`, which `RBLOAD` accepts** — two commands calling the same `OPEN`
disagree. Worth a source dive; likely the same root cause as the above.

## 4. `SET-REGISTER` targets the wrong register

`SET-REGISTER X 777` leaves `X = 0`; `SET-REGISTER A 1234` left `A = 0` and set
`STS = 234`. No error reported. Reproduced again on the post-fix sweep.

An earlier root cause for this was **retracted** — see
`docs/TSS-COMMAND-VALIDATION.md` PART III §4. Not
re-investigated since. Treat the retracted analysis as untrusted.

## 5. `LOAD-SYSTEM` does not return — mechanism confirmed, fix identified

The only command that never comes back, and the reason is now read from
source rather than guessed. `LOADV` (`src/TSS5.SYMB:604-626`,
`"CDC`-only):

```
L2,  SAX 0; SAT 0; SAA 1; JPL I (SDISK     % read disc page 0 -> core 0
     IOF; RCLR DP                          % P := 0, execute what was loaded
```

`RCLR DP` zeroes the program counter, so the next instruction executed is
`core[0]` — which `SDISK` has just loaded from **disc page 0**. On a working
system that page holds `DKRST` (`JMP *+2; CORLD`), which pulls page 2,
`DBOOT` reads the coreload address out of `core[1]`, reloads core and jumps
through `0301`. No front-panel intervention is involved.

**Our MINIT-formatted disc has never had a bootstrap written to page 0**, so
`RCLR DP` transfers control to whatever is there. That is the whole defect:
the command is correct and the disc is unprepared.

**Investigated 2026-07-30. The mechanism is proven; the archived bootstrap
cannot drive this machine.** What was established, each by measurement:

1. The producer exists: `src/TSS3.SYMB:284-292`, under `"CDBIN`, emits
   `8CADR/DKRST 8DKA/0` and `8CADR/RDKOP 8DKA/2` - the boot sector and its
   helper. It is guarded by `"TSBIN`, FALSE in both archived builds
   (`src/TSS1.SYMB:47`).
2. `SYSSV` (`src/TSS1.SYMB:4108`) already writes 64 pages of core to disc from
   `CORLD=60` via `XDISK`. The system image is on the disc; `DBOOT` reads back
   exactly that range. Only the boot sector was missing.
3. Writing `DKRST` (54 words) to logical disc 0 and `RDKOP`/`DBOOT` (67 words)
   to logical disc 2, mapped through `cdc_dkadr` (physical sectors 0 and 4),
   **works**: after `LOAD-SYSTEM`, `core[0..7]` reads
   `171002 054003 134004 125002 030424 030504`, exactly `DKRST`, with
   `P=000006 T=000002 X=030424 L=000003` - `LOADV` read page 0 and `RCLR DP`
   executed it. Both sectors were zero beforehand.
4. It then **spins forever** at `RDKOP`'s `IOT SKA DISC; JMP *-1`. The
   bootstrap is `"CDC NMACF` code that talks NORD-1 `IOT` to disc channel
   `DISC=DCHN+44`. `DISC` is defined only in the `NN10` branch, so an `N10`
   build assembles `IOT SKA 0`; an `NN10` build assembles device `0144`.
   **There is no NORD-10 `IOX` variant of `DKRST`/`RDKOP` in the corpus.**
   Our running system is the DRUM+N10 build, whose disc lives at `IOX 500-507`.
5. nd100x's CDC controller supports only thumbwheel 0 -> address `0500`
   (`src/devices/cdc/deviceCDC.c:573-587`), so it cannot be moved to `0144`.

**Consequence - the goal splits in two:**

- **Making `LOAD-SYSTEM` work is achievable.** `LOADV` runs on the live
  machine and reads sector 0 itself, so it needs no emulator support - only a
  boot sector that drives the disc with `IOX`. That code does not exist in the
  archive and would have to be written, which makes it a `derived/` artifact,
  not a restoration.
- **A tape-free cold boot is NOT achievable in nd100x today.** Its boot types
  are `BP, BPUN, AOUT, PROG, FLOPPY, SMD, SCSI`; `boot_type_for_ctrl`
  (`src/frontend/nd100x/nd100x.c:102-110`) maps only SMD, FLOPPY and SCSI.
  There is no CDC boot path. On real hardware the LOAD button and microcode
  read sector 0; the emulator has no equivalent for this controller. Booting
  will need `--boot=bpun` until nd100x gains one.

See also `TSS-COMMAND-VALIDATION.md` PART III §2 and
`TSS-PSEUDOCODE.md` §3.9.

## 6. nd100x items

**~~FPP32 is internally inconsistent.~~ RETRACTED 2026-07-28 — nd100x is
correct.** `ndfunc_stf`/`ndfunc_ldf` move three words unconditionally, and
that is right: ND-110 RASK and ND-120 DELILAH-L microcode hardwire `LDF`/`STF`
to the 3-word `T/A/D` layout with no 2-word entry point. A 32-bit-FPP machine
uses `LDD`/`STD` for its `A,D` accumulator instead. **No emulator change is
wanted**, and the patch proposed here was withdrawn before it was applied.
Detail in `TSS-FLOAT-FORMAT.md` §4.

**Adopt in the harness (nd100x >= 0465922, `feat/prog-loader-and-shell-exec`),
per the nd100x team 2026-07-28:**

- the debugger sleep now only occurs while a DAP command holds the CPU, so
  debugger-attached runs execute at full speed;
- `--throttle` defaults to 0.5275 MHz, which makes the instruction-locked RTC
  exactly 50 Hz in real time;
- new `--rtc=wall` (or `[machine] rtc = wall`) fires the RTC on host
  wall-clock 20 ms regardless of emulation speed; default `--rtc=ticks` is
  unchanged;
- DAP disconnect no longer terminates the emulator — detach and re-attach now
  works, which the probe scripts currently do not exploit.

Their re-measurement on this exact setup: 47.1 Hz with ticks+throttle, 48.5 Hz
with `--rtc=wall`. **None of this affects the frozen-clock finding**, which
depends only on `KLOK` advancing monotonically, not on its rate.

## 7. `ASSYSA.SYMB` line 2 (`100`) and the `*:` line

Unknown. Probably documented command or console conventions rather than
source. Start with
`$NDINSIGHT/Reference-Manuals/ND-60.096.01 MAC Interactive Assembly and Debugging System User's Guide.md`.

## 8. `LIST-ACCOUNTS LINE-PRINTER` produced no file

Inconclusive rather than open-and-failing: `INIT-ACCOUNTING` had just cleared
the accounting file. Re-test in the other order.

---

## Method note — why this list is short

Several long-running items were closed by **single-variable experiments on a
running machine** after months of source reasoning failed. The recurring
failure mode, recorded in `TSS-FLOAT-FORMAT.md` §7 and
`MAC-ASSEMBLER.md` §6.9: a correct measurement gets dismissed because it
contradicts what the source says the code should do — while the *emitted code
does not match the source*. Two oracles (718 assertions, the golden dumps)
were fully satisfied by a binary whose clock arithmetic was inverted, because
the dumps validate word counts only, never encodings.

When an item here is picked up: change one variable, predict the outcome in
advance, and include a negative control.
