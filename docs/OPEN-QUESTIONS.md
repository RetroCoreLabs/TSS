# Open questions — NORD TSS 3.0

**This file:** `/mnt/e/Dev/Ronny/TSS/docs/OPEN-QUESTIONS.md`
**Repository root:** `/mnt/e/Dev/Ronny/TSS`
**Read first:** `/mnt/e/Dev/Ronny/TSS/CLAUDE.md` — the standing rules override
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

**Own document:** `/mnt/e/Dev/Ronny/TSS/docs/TSS-FLOAT-FORMAT.md`

`TBANG`'s constants are 2-word 32-bit; `FPDAT` and `NORM` require 48-bit. The
consequence is that the time of day cannot advance. Confirmed by patching `K1`
in memory; **not** a `mac-c` defect, and not to be "fixed" there.

Settling it needs a primary source on the ND-100 `LDF`/`STF` word count per FPP
option — something to read, not to reason about. No further emulator
experiment can help until nd100x's FPP32 mode is self-consistent (see §6).

## 2. `QOV1C` — a one-word oracle mismatch

Golden `000642`, produced `000641`. An `OVERX` overlay-size symbol, and one of
the two "extra" entries in the version-A score. The `-L` listing makes the
`)9MOVE`/`VOR` arithmetic inspectable for the first time; that is the route in.

Not started.

## 3. `TDUMP` has never executed

`/mnt/e/Dev/Ronny/TSS/Build/tdump/tdump.bpun` assembles with zero diagnostics
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
`/mnt/e/Dev/Ronny/TSS/docs/TSS-COMMAND-VALIDATION.md` PART III §4. Not
re-investigated since. Treat the retracted analysis as untrusted.

## 5. `LOAD-SYSTEM` does not return

The only command that never comes back. The command itself is correct; the
working disc carries no bootstrap for it to reload. This needs a **different
build variant**, not a code fix. See `TSS-COMMAND-VALIDATION.md` PART III §2.

## 6. nd100x items

**FPP32 is internally inconsistent.** `ndfunc_fad/fsb/fmu/fdv/nlz/dnz` branch
on `CurrentFPPType`, but `ndfunc_stf` (`src/cpu/cpu_instr.c:637`) and
`ndfunc_ldf` (`:681`) move three words unconditionally. In 32-bit mode the
accumulator is `A,D`, the arithmetic reads its operand from `EA+0,EA+1`, and
`STF` stores that accumulator to `EA+1,EA+2` — **a one-word disagreement
between the arithmetic and load/store**, with `EA+0` receiving a stale `gT`.
Reported upstream. Until this changes, `--fpp=32` results are not evidence
about TSS.

This is a claim about internal consistency only. What real 32-bit-FPP hardware
did with `LDF`/`STF` is §1's open question and is **not** asserted here.

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
`E:\Dev\Ronny\NDInsight\Reference-Manuals\ND-60.096.01 MAC Interactive Assembly and Debugging System User's Guide.md`.

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
