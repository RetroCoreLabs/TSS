# TSS Source Symbol Changes (to make the recovered system RUN)

**Full path:** `E:\Dev\Ronny\TSS\docs\SYMBOL-CHANGES.md`

This document records every change made to the recovered TSS **symbol/source**
set beyond the original 1973/1978 archive, **why** each was necessary, and
**why the chosen value/placement is the correct one**. It is deliberately
exhaustive: these edits alter the historical source, so each must be justified
from a primary source and validated against the golden reconciliation.

Scope note: this covers **source-symbol** changes only. Emulator (nd100x) and
assembler (mac-c) changes are recorded in their own docs
(`docs/CDC-DISC-DEVICE.md`, `docs/OVERLAY-DISC-SPEC.md`,
`docs/LOGON-PATH.md`, and the nd100x MMS-type config work). The one supporting
tooling change made alongside a symbol change is noted in §4.

Every claim below is tagged **[VERIFIED]** (primary source `file:line`, or a
live nd100x observation) or **[INFERRED]** (reasoned, not directly proven).
Numbers are **OCTAL** unless marked "dec".

---

## Why any change at all — the invisible gap

The golden builds (`ASSYSA` → `reference/ASYMB.SYMB`, `ASSYSB` →
`reference/BSYMB.SYMB`) only ever produced **symbol-table dumps**; they were
**never executed**. This project is the first time the recovered TSS is
actually *run* on a CPU (nd100x). Running exposes bugs that a symbol dump
cannot: specifically, a **variable that is referenced but never defined** has a
literal that resolves to **0**, and any executed store *through* that literal
writes to **address 0**. That never mattered for a dump; it is fatal for a run.

The corpus already documents "20 symbols that remain undefined after a full
build, and that is correct" (see `CLAUDE.md`). That statement is correct **for
the golden dumps**. It is **not** safe for a running system if any of those
undefined symbols is used as the target of an executed store. Exactly one is:
`EXRGP` (§1).

---

## 1. `EXRGP` — restored resident pointer word

### 1.1 The change

**File:** `E:\Dev\Ronny\TSS\src\TSS2.SYMB`, in the `%LEV14 VARIABLES` block,
immediately after `EXRP` and **before** `SAVEX`:

```
MCODE, 0 %MONITOR CALL CODE
EXRP, 0  %EXR ADDRESS
"N10
EXRGP, 0 %EXR REGISTER-SAVE POINTER (RECONSTRUCTED - SEE NOTE ABOVE)
"
SAVEX, 0 %SAVED EXR + NEXT 2 INSTRUCTIONS
```

(The source carries a full multi-line `%` comment above it; reproduced here in
condensed form.)

### 1.2 Why it was needed — root cause, from primary evidence

`EXRGP` is **referenced but never defined** in the recovered source.
**[VERIFIED]** `grep EXRGP src/` returns exactly three lines, all *references*,
zero *definitions*:

| ref | context | build that executes it |
|---|---|---|
| `TSS1.SYMB:3181` | `S10`: `... STZ I (EXRGP; ...` | **N10 (runs)** — unconditional context-init |
| `TSS1.SYMB:4101` | `LEVX`: `LDX I (EXRGP; JXZ LEVX1; STZ I (EXRGP` | `"NN10` only (software instruction emulation) |
| `TSS1.SYMB:4231` | `EXRG`: `AAX -1; STX I (EXRGP` | `"NN10` only (software instruction emulation) |

Because `EXRGP` is undefined, the literal `(EXRGP` assembles to **0**.
**[VERIFIED]** in the flat drum image the S10 literal pool word for `(EXRGP`
is `0`:

```
[007445]=015243  [007446]=000000  <- the (EXRGP literal
[007447]=015212  [007450]=015211  (every neighbour is a valid address)
```

At `S10` (`TSS1.SYMB:3181`, executed for **every** fresh user in the N10 build),
`STZ I (EXRGP` therefore stores 0 through the null literal **into physical
address 0**. **[VERIFIED live, nd100x + physical write watchpoint on address 0]**
the *only* write to address 0 in the whole boot is `STZ I 72` at PC `007355`
(= this `STZ I (EXRGP`), which changes physical word 0 from `0125003` to
`0000000`.

Physical word 0 is the **GOVER dispatch vector** — `0/ JMP I *+3`
(**[VERIFIED]** `TSS1.SYMB:61-70`; assembled image word 0 = `0125003`). Once it
is zeroed, every `GOVER OVn` (which transfers to address 0 to reach `GOVX`)
falls through `0000000` instead of dispatching, the overlay dispatcher spins,
and **LOGON is never reached**. This is the exact, long-standing "TSS won't
reach LOGON" blocker.

### 1.3 Why `EXRGP` is a *pointer variable* and why the value is `0`

`EXRGP`'s three uses fix its type and correct initial value:

* `S10` (`TSS1.SYMB:3181`) **clears** it: `STZ I (EXRGP` → `EXRGP := 0` at each
  user-context init. **[VERIFIED]**
* `LEVX` (`TSS1.SYMB:4101`, `"NN10`) reads it as a pointer: `LDX I (EXRGP;
  JXZ LEVX1` (if the pointer is 0, skip), else `STZ I (EXRGP; LDF I (SAVEX;
  STF 0,X` (clear it, then deposit the saved float registers at the address it
  held). **[VERIFIED]**
* `EXRG` (`TSS1.SYMB:4231`, `"NN10`) writes it: `AAX -1; STX I (EXRGP`
  (`EXRGP := RBLOK-1`, a pointer into the user register block). **[VERIFIED]**

So `EXRGP` is a **single resident word holding a pointer** into the user
register-save block, used by the NORD-1 (`"NN10`) software instruction
emulation, and **initialised to 0** (S10 clears it; a 0 pointer is the "no
pending save" state that `LEVX`'s `JXZ` explicitly tests). Therefore:

* **The word's value is `0`** — matches the S10 clear and the `LEVX` "0 = none"
  convention. **[VERIFIED from the three uses]**
* **The literal `(EXRGP` must be a valid non-zero resident address** (the
  address of this word). That is the whole point of defining it: the store then
  clears `EXRGP`, not address 0.

The identity is reinforced by the family it sits in: `MCODE` (monitor-call
code), `EXRP` ("EXR ADDRESS"), `SAVEX` ("SAVED EXR") — all `%LEV14 VARIABLES`
in `TSS2.SYMB:104-108`. `EXRGP` ("EXR reGister Pointer") is the missing fourth
member. **[INFERRED, strong]** the original 1973 source defined it right here.

### 1.4 Why it is placed **before** `SAVEX`

`SAVEX` heads a **3-word** area, not one word: `LEVX` does `LDF I (SAVEX`
(**[VERIFIED]** `TSS1.SYMB:4102`), and a MAC 32-bit float `LDF` reads
**3 consecutive words** ("SAVED EXR + NEXT 2 INSTRUCTIONS", the comment on
`TSS2.SYMB:108`). Inserting `EXRGP` *after* `SAVEX` would land inside that float
and corrupt it. So `EXRGP` is inserted after `EXRP`, before `SAVEX`.

### 1.5 Why it is gated `"N10` (and not unconditional)

**Constraint:** the golden dumps are the project's oracle; a change is only
acceptable if it keeps the golden reconciliation byte-identical
(`CLAUDE.md`, "the validation methodology").

* **[VERIFIED]** both golden builds are **NN10** (neither sets `N10`):
  `ASSYSA.SYMB:4` = `CDC MACF DIAB K14 TEL4`; `ASSYSB.SYMB:4` =
  `CDC MACF DEBUG DIAB K14 TEL4`.
* `EXRGP` is **absent from `reference/ASYMB.SYMB`/`BSYMB.SYMB`** — so the 1978
  source that produced the golden dumps *also* left it undefined. Adding a
  dedicated resident **word unconditionally** would occupy one location and
  **shift every symbol defined after it** in TSS2/3/4/5, destroying the
  679/693 (A) and 675/689 (B) match. **[VERIFIED reasoning; confirmed by the
  score below]**
* Gating the restored word `"N10` means: in the **NN10 golden** builds the word
  is **not** assembled → **no shift → dumps byte-identical**; in the **N10
  runnable** build the word **is** assembled → `EXRGP` gets a real address →
  the S10 store clears the variable instead of address 0.

This is the *most correct* choice that satisfies both "make it run" and "do not
perturb the oracle." An NN10 build that is one day actually *executed* would
need `EXRGP` **unconditional** (its emulation path uses it) and a golden
**re-baseline** — noted in the source comment as the future path. **[INFERRED]**

### 1.6 Validation — real evidence

* **Golden reconciliation UNCHANGED.** After the edit,
  `./build_tss_assysa.sh` + `./compare_asymb.sh`:
  `exact matches : 679` (A), 675 (B) — the only unmatched entries remain the
  pre-existing **13 macro names** + the pre-existing **`QOV1C` off-by-one**
  (the separate `mac_permsym.c` issue). **Byte-identical to baseline.**
  **[VERIFIED]**
* **`EXRGP` now defined:** drum `DSYMB.SYMB` shows `EXRGP=015246`. The S10
  literal is no longer 0. **[VERIFIED]**
* **The clobber is gone / GOVER now dispatches.** Before: `GOVX` executed **0**
  times (word 0 destroyed). After: `GOVX` executes **1194** times, address 0 is
  entered and correctly dispatches, and the CDC controller is issued the reads
  for OV15's own physical sectors **`027040` and `027150`** (= `72·0244`,
  `72·0245`). **[VERIFIED live]** The overlay machinery works end-to-end through
  the disc read — none of which happened before the fix.

### 1.7 Status of the boot after this fix

This fix resolves the **word-0 clobber** completely and is **necessary**. It is
**not yet sufficient** to reach LOGON: a **separate, downstream** blocker
remains — after OV15 is read, the overlay body at `ROVER=031400` faults
(`TRAP lvl=14 sub=8` at `031411`) and the dispatch loops. **This is under
investigation and is unrelated to the `EXRGP` symbol change** (it concerns the
overlay read-delivery / body execution, not an undefined symbol). It is tracked
in `docs/LOGON-PATH.md`. **[VERIFIED live; root cause OPEN]**

---

## 2. Symbols examined and deliberately **NOT** changed

To be explicit about scope, these were analysed and left alone:

* **`REA=400`, `RKE=0`** (`TSS1.SYMB:1096,1098`, inside `"N10`) — **defined**
  constants in the N10 build; not used in any executed store-through; harmless.
  **[VERIFIED]** They are "undefined" only in the NN10 golden build, which is
  correct there.
* **`STR`, `STR0`–`STR2`, `STR1X`, `STR2X`** — the six casualties of CVS's
  incomplete `STR`→`XTR` rename. Referenced but undefined; **not** used as
  executed store targets, so they do not clobber. Left as-is (changing them
  would diverge from the archived patched source). **[VERIFIED per CLAUDE.md
  corpus notes]**
* **`&L`, `PROGM&M`** — expressions by design, not variables. **[VERIFIED]**
* The remaining undefined symbols are **library marks** (true when referenced-
  but-undefined; that is their intended mechanism). **[VERIFIED]**

Only `EXRGP` met the dangerous pattern *undefined **and** executed store-through
in the N10 build*, which is why it is the only symbol restored.

---

## 3. Reproducing / re-verifying these claims

| step | command (from `mac-c/`) | expected |
|---|---|---|
| golden A/B unchanged | `./build_tss_assysa.sh` then `./compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB` | `exact matches : 679` |
| `EXRGP` defined | `grep -i '^EXRGP=' ../Build/drum/DSYMB.SYMB` | `EXRGP=015246` |
| GOVER dispatches / OV15 reads | `./logon_trace_probe.sh` and the CDC-op trace | `GOVX>0`, LBA `027040`/`027150` |

---

## 4. Supporting tooling change (not a symbol change, recorded for completeness)

**File:** `E:\Dev\Ronny\TSS\mac-c\logon_trace_probe.sh`. The probe previously
hard-coded the login-path addresses (LEV2, ERMSG, LOGON, XSTAR, …). Restoring
`EXRGP` shifted every TSS2+ symbol by **+1 word**, which silently made those
hard-coded addresses point at the wrong instructions (they read as 0 hits). The
probe now **resolves the shiftable addresses from the build's own
`DSYMB.SYMB`** at run time, so it can never again go stale from an address
shift. TSS1-resident addresses that MAC `)KILL`s out of the dump
(`INIT`/`LEV5`, and `LEV2`) keep documented fallbacks. **[VERIFIED]**

---

## Change log

| date | symbol | file | change | golden impact |
|---|---|---|---|---|
| 2026-07-21 | `EXRGP` | `src/TSS2.SYMB` | restored `EXRGP, 0` gated `"N10` (LEV14-variable family, before `SAVEX`) | none — A=679, B=675 unchanged |
