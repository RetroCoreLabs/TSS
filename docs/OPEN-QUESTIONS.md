# Open questions — NORD TSS 3.0

**This file:** `docs/OPEN-QUESTIONS.md`

The live list of what is still unanswered about TSS. When an item is
answered it moves into the document that owns the subject, and it is deleted
from here rather than accumulating a status history.

Current as of 2026-07-31. Repository state: `mac-c` 732 assertions / 0
failures, golden oracle 679/693 (A) and 675/689 (B), 0 assembly errors; the
system cold-boots from the disc alone (`make discboot`), with the bootstrap
installed by TSS's own code from the mac-punched CDBIN tape
(`TSS-BRINGUP.md` §6.7).

---

## 1. `QOV1C` — a one-word oracle mismatch

Golden `000642`, produced `000641`. An `OVERX` overlay-size symbol, and one of
the two "extra" entries in the version-A score. The `-L` listing makes the
`)9MOVE`/`VOR` arithmetic inspectable for the first time; that is the route in.

Not started.

## 2. `TDUMP` has never executed under TSS

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

(The CDBIN bootstrap tape that TDUMP's 8DUMP run would punch is produced by
`mac`'s own `)8DUMP` today — `mac-c/scripts/build/build_tdump.sh`. Running
TDUMP as a real user program under a live TSS remains the fully-original
route, and is blocked on exactly this item.)

## 4. `ASSYSA.SYMB` line 2 (`100`) and the `*:` line

Unknown. Probably documented command or console conventions rather than
source. Start with
`$NDINSIGHT/Reference-Manuals/ND-60.096.01 MAC Interactive Assembly and Debugging System User's Guide.md`.

## 5. `LIST-ACCOUNTS LINE-PRINTER` produced no file

Inconclusive rather than open-and-failing: `INIT-ACCOUNTING` had just cleared
the accounting file. Re-test in the other order.

## 6. How did the wrong `TBANG` constants get into the 1978 build?

TSS is a 48-bit floating-point program whose `TBANG` clock constants were
assembled in the 32-bit format, so the archived system's time of day could
not advance on real hardware either (`TSS-FLOAT-FORMAT.md` settles the
format question). What remains is historical: how the wrong constants got
there and whether anyone noticed in period. A verified fix exists since 2026-10-08
(the `CLKFX` mark, `TSS-FLOAT-FORMAT.md` §8); the historical question is
unchanged.

---

## Method note — why this list is short

Several long-running items were closed by **single-variable experiments on a
running machine** after months of source reasoning failed. The recurring
failure mode, recorded in `TSS-FLOAT-FORMAT.md` §7 and
`MAC-ASSEMBLER.md` §6.10: a correct measurement gets dismissed because it
contradicts what the source says the code should do — while the *emitted code
does not match the source*. Two oracles (the unit-test suite, the golden
dumps) were fully satisfied by a binary whose clock arithmetic was inverted,
because the dumps validate word counts only, never encodings.

When an item here is picked up: change one variable, predict the outcome in
advance, and include a negative control.
