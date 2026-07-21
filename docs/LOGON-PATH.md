# NORD TSS -- how the cold system starts a login session, and the final blocker

Why the LIVE-booting DRUM/N10 build idles in the level-5 scheduler and does not
show a LOGON prompt, and the exact trigger that makes it reach `LOGON`.

Derived **strictly** from the TSS source (`E:\Dev\Ronny\TSS\src\TSS*.SYMB`) and
verified live on nd100x under DAP (2026-07-21). Every claim is tagged
**[VERIFIED]** (source `file:line` + quote, or live trace/DAP observation) or
**[INFERRED]**. Things I could not settle are in section 6. Numbers are **OCTAL**
unless marked "dec" (that is how MAC prints). DRUM-build addresses are from
`Build/drum/DSYMB.SYMB`.

## TL;DR -- corrected picture

The earlier notes (`docs/BOOT-PROCESS.md`, `docs/COLD-BOOT-INIT.md`) chased
`XSTAR = GOVER OV19` as the login target and recommended forcing `OVLAY=-1`.
**That is the wrong target for a cold first login.** A freshly-created console
process is dispatched by the swapper straight to **`LEV2` (025104)** with
**`RMODE:=-1`**, and `LEV2` calls **`LOGON` (032735)** directly -- it never goes
near `XSTAR`. Confirmed live: `XSTAR` is executed **0** times; `LEV2` is entered
and 154 259 instructions run at PIL 2.

The real, verified blocker is **one instruction inside `LEV2`**: it prints a
start-up message via `ERMSG`, and `ERMSG = JMP I (FERMS`, `FERMS = GOVER OV15`
-- an **overlay call to OV15** that our disc cannot satisfy runnably. It loops
forever in the overlay dispatcher and never falls through to `JPL I (LOGON`.
Neutralising that single call makes TSS reach `LOGON` (proven live: the LOGON
breakpoint fires). **No console keypress is needed to reach LOGON** -- the
console/operator terminal is auto-started at INIT.

---

## 1. The terminal-activation path

There are **two** ways a terminal becomes an active session. The process table
`PRT` drives both. Each `PRT` entry is 10 (octal) words; word 5 is "TTY USAGE"
**[VERIFIED]** `TSS1.SYMB:2630-2641`:

```
0	%STATUS	BIT15 = BLOCK BIT ...          (word 4)
0	%TTY USAGE:                             (word 5)
	%NEGATIVE - TELETYPE SCANNER WILL SCAN THIS TELETYPE (PROCESS IS DEAD)
	%ZERO - SCHEDULER WILL INITIALIZE THIS PROCESS
	%POSITIVE - TELETYPE IN USE (PROCESS ALIVE)
```

### 1a. Console / operator terminal -- AUTO-STARTED at cold boot (no input)

`INIT` seeds `PRT1` (the first process = console) word 5 = **0**, and every other
`PRT` entry word 5 = **-1** **[VERIFIED]** `TSS1.SYMB:258,265-267`:

```
LDX (PRT1; STX I (PRT; STZ 0,X; STZ AVAIL         ; PRT := PRT1
...
SAA 0; STA 5,X;  LDX (PRT2; LDT (PRTXX             ; PRT1[5] := 0   (X was PRT1)
SAA -1; STA 5,X; AAX 10                            ; PRT2..PRTXX[5] := -1
SKP IF DT LST SX; JMP *-4
```

So at cold boot the console's word 5 is **0 = "SCHEDULER WILL INITIALIZE THIS
PROCESS"**, and the round-robin scheduler `LEV5` picks it up on the first pass
**[VERIFIED]** `TSS1.SYMB:2729-2736`:

```
L2, LDA 5,X; JAZ L5; JAN L0        ; word5==0 -> L5 (initialise)
...
L5, SAA -1; JMP SWAPR              ; A=-1 -> SWAPR "initialise" case
```

`SWAPR` routes the A<0 case to **`S10`** **[VERIFIED]** `TSS1.SYMB:2789`
(`LDA I 9FLAG; JAP SW1; JMP I (S10`), which builds a fresh context and dispatches
level 2 **[VERIFIED]** `TSS1.SYMB:3160-3173`:

```
S10, LDX I 9PRTX; STX I 9PRT
    SAA -1; STA I (RMODE               ; RMODE := -1  (this is a fresh cold user)
    STZ 2,X; STZ 3,X; STZ 4,X; SAA 4; STA 7,X; SAA 1; STA 5,X   ; word5 := 1 (alive)
    JPL I (AQCTX; ...; LDA (LEV2        ; A := LEV2   (025104)
S99, ...; IRW 20 DP                    ; level-2 P-register := LEV2
```

`S10` then falls into `S5` (`JMP I (S5`, `TSS1.SYMB:3192`); for a fresh user the
overlay check is skipped (see 3) and `S5A` triggers level 2 via the saved PID
**[VERIFIED]** `TSS1.SYMB:3086-3087` (`LDA 7,X; MST PID; ... WAIT; JMP LEV5`).
Level 2 then runs `LEV2 -> LOGON`.

### 1b. Any other terminal -- WOKEN by a typed character (LEV6 scanner)

Dead terminals (word 5 = -1) are watched by the level-6 **teletype scanner**,
"runs every 80 milliseconds" **[VERIFIED]** `TSS1.SYMB:2653-2694`. For a terminal
whose word 5 is `<= 0` it reads the input status/data register and, on a real
character, **sets word 5 := 0**, handing the terminal to the scheduler
**[VERIFIED]** `TSS1.SYMB:2664-2677`:

```
L1C, ... ADD (PRT1+5; COPY DB SA; COPY DD SA; LDA 0,B; JAP L10   ; word5>0 (alive)? skip
     ... ADD (TDEVI; COPY DB SA; LDA 0,B                          ; else read this tty's device
     ORA (IOX RDR; ...; BSKP ONE 30 DA; JMP L2                    ; N10: no char ready -> next tty
     LDA 0,B; ORA (IOX RDR; EXRG SA; AND (177; SUB (33; JAF L2    ; read char; ignore < 33 octal
     COPY DB SD; STZ 0,B                                          ; word5 := 0  -> scheduler inits
```

The scanner (level 6) and the scheduler (level 5) are both armed each clock tick
by the subsidiary clock `LEV9` **[VERIFIED]** `TSS1.SYMB:4011-4016`
(`%ENABLE TELETYPE SCANNER  SAA 100; MST PID` = level 6; `SAA 40; MST PID` =
level 5). Both levels are unmasked in `PIE = 77157` **[VERIFIED]** bit-expansion
of `ENABL, 77157` (`TSS1.SYMB:300`): levels 0,1,2,3,5,6,9,10,11,12,13,14 on.

**Live confirmation [VERIFIED, DAP].** Entering at `INIT=000301` and running:
`RMODE (015203) = 177777 (-1)`, `PRT1` word 5 (`006123`) = positive/alive, and
6 000 000 instructions distribute as PIL 5 = 5 728 285, **PIL 2 = 154 259**,
9 = 71 169, 6 = 21 995, 13 = 7 370, ... i.e. the console process really was
created and really runs at level 2.

---

## 2. The wait condition -- what TSS is actually polling

The `IOX 306` seen in the live trace is **not** an input wait. `306` is the
console teletype's **read-output-status** register (N10 map, `MINIT.SYMB`
IOXLIB: `300`=read-data, `302`=read-input-status, `303`=write-input-control,
`305`=write-data, `306`=read-output-status, `307`=write-output-control
**[VERIFIED]** `MINIT.SYMB:620-719`). The clock routine `LEV13` reads it every
tick to refresh the level-4 console register block **[VERIFIED]**
`TSS1.SYMB:3996-4004` (`%STATUS INFORMATION FOR LEVEL 4 REGISTER BLOCK ...
IOX 306; IRW 40 DA`). It is housekeeping, not a keypress wait.

The genuine "wait for a key" is the **level-6 scanner reading `IOX RDR`** on dead
terminals (1b). But the **console does not wait** -- it is auto-started (1a). So
at cold boot the system is *not* blocked on console input; it is blocked
elsewhere (section 3).

---

## 3. Terminal/user-table state at cold boot -- and the real blocker

State is correct and self-sufficient at cold boot; **no MINIT / generation /
operator command is required to have a console session**:

* `PRT1[5]=0` (console armed for auto-init), `PRT2..PRTXX[5]=-1` (dead, awaiting
  a key). **[VERIFIED]** `TSS1.SYMB:265-267` and live (`PRT1[5]` went 0 -> alive).
* `OVLAY (015205) = 036`, `SYSOV (011333) = 036` on the unpatched cold image
  **[VERIFIED live]** (both read `0x001E` = 036 = `OV19`). So `S5`'s overlay check
  `LDA I (SYSOV; SUB I 9OVL; JAZ S5X` **skips** cleanly for the fresh user
  **[VERIFIED]** `TSS1.SYMB:3070`. *The old `OVLAY=SYSOV=-1` patch is
  unnecessary and, if `SYSOV` is left at 036, actively harmful -- it makes `S5`
  attempt a bogus overlay read on the very path that creates the console.*

The console process is therefore created and dispatched to `LEV2` (025104). And
this is where it dies **[VERIFIED]** `TSS2.SYMB:1846-1855`:

```
LEV2, JPL I (ISTK
   LDT I (CINF; INTDS; JPL I (IBUF; INTEN
   SAT 1; JPL I (SXBRK
   LDA (CORLD; SUB (CORL1; JAF L2      ; CORLD==CORL1 here -> fall through to L1
L1,   SAA 52; JPL I (ERMSG; JMP L3     ; <-- prints startup message #52, THEN L3
L2,   SAA 61; JPL I (ERMSG
L3,   JPL I (LOGON                     ; the login routine (never reached)
LEV2A, JMP I (XSTAR
```

`ERMSG` is not resident code -- it is an overlay call **[VERIFIED]**:

```
ERMSG, JMP I (FERMS          TSS2.SYMB:3499
FERMS: GOVER OV15,E1,FERMS   TSS5.SYMB:1172   (OV15 = 032 octal = overlay 26)
```

So `LEV2` unconditionally does `GOVER OV15` to print its start-up banner. On our
disc `OV15` is not staged as a runnable overlay (the `)SOVER`/`)9MOVE`/ROVER
staging that mac-c does not reproduce -- see `docs/BOOT-PROCESS.md`), so the
overlay dispatcher spins. **Live [VERIFIED]:** over 6 000 000 instructions
`LEV2` is entered exactly **1**, `SXBRK` **1**, **`ERMSG` (031322) 2332 times**,
and **`LOGON` (032735) 0 times, `XSTAR` (033601) 0 times.** The PIL-2 hot spots
are the `STK`/`USTK` stack traps (020457/020535), page-zero vector 0 (`GOVX`
overlay dispatch), and `FERMS` (033477) -- i.e. the overlay dispatcher churning
on `GOVER OV15`. Some CDC disc I/O does occur (`SDISK` runs, `IOX 504` status
reads), so the read is *attempted*; the loaded OV15 is simply not runnable.

**This is the same overlay-staging gap as the OV19 story, reached earlier on the
path (OV15 for the banner, before OV19's `LOGON`).**

---

## 4. The minimal trigger to reach LOGON (ranked, actionable for the emulator)

Entry: `--start=000301` (INIT) with `--drum` + `--cdc` attached, as today.
**Do NOT patch `OVLAY`/`SYSOV`** (section 3). No console keypress is needed to
*reach* LOGON.

### Rank 1 -- faithful fix: stage OV15 (and the other overlays) loadably on the CDC disc
Make `GOVER OV15` load a runnable OV15 the way the original `)SOVER`/ROVER
machinery did. Then `ERMSG` prints its message, returns, and `LEV2` falls through
to `JPL I (LOGON`. This is the correct, historically-faithful fix and it fixes
every other `GOVER` (the command processor overlays too), not just the banner.
[VERIFIED this is the design; producing the staged disc is the open engineering
task, shared with the OV19 work.]

### Rank 2 -- diagnostic proof / cheap unblock: neutralise the `ERMSG` call in `LEV2`
Patch the single word at **025117** (phys `0x2a4f`, the `JPL I (ERMSG` on the
taken L1 path, `135016`) to `JMP *+1` (`124001`). `LEV2` then goes straight to
`L3 = JPL I (LOGON`. `LOGON` itself is inside the resident `OV19` body, so it
runs. **[VERIFIED LIVE]** -- see section 5. This skips the start-up banner; it is
a proof/bring-up shim, not a faithful boot.

### Rank 3 -- "type a character" (NOT needed for the console)
Feeding a console character wakes only a *dead* terminal via the level-6 scanner
(1b). The console is `PRT1` with word 5 = 0 (auto-init), not -1, so a keypress is
**not** what starts the console session. A keypress is needed later -- *after*
`LOGON` -- to actually log in and drive the command loop (`S1`/`CLOOP` reads the
line via `EDIT`/`GCMD`, `TSS5.SYMB:1829-1855`). [VERIFIED: console is word5=0 at
INIT; scanner only acts on word5<=0 dead ttys.]

### Not required
`22!` / `10,0$` operator commands, `MINIT`, or a generation step. The cold image
already contains an armed console `PRT` entry. [VERIFIED `TSS1.SYMB:265-267`.]

---

## 5. What I verified live (nd100x + DAP, 2026-07-21)

Boot: `nd100x --boot=bpun --image=Build/drum/tss-drum.bpun
--drum=... --cdc=... --start=000301 --debugger --port=1777`.

1. **Halted cleanly at entry `PC=000301` (INIT)** with `PIE=0`, then ran; after
   the run `PIE=077157`, paging on, `PID` cycling levels 2+5. **[VERIFIED]**
2. **Console process auto-created:** `RMODE(015203)=-1`, `PRT1` word 5 = alive,
   `OVLAY=SYSOV=036`. `LEV2` entered once; **154 259 instructions ran at PIL 2**.
   **[VERIFIED]**
3. **`LOGON`/`XSTAR` never reached unmodified:** address hit counts over 6M
   instr -- `LEV2=1, SXBRK=1, ERMSG=2332, LOGON=0, XSTAR=0`. The PIL-2 loop sits
   in the overlay dispatcher (`STK`/`GOVX`/`FERMS`). **[VERIFIED]**
4. **Root-cause proof:** wrote `025117 := 124001` (`JMP *+1`, neutralising
   `JPL I (ERMSG`) and reset `PRT1[5]:=0` to force a fresh dispatch. The
   instruction breakpoint at **`LOGON=032735` fired**; at the stop `P=032735`,
   `L=025124` (the return slot right after `LEV2`'s `JPL I (LOGON`), `A=052` (the
   leftover message code). Continuing, `LOGON` ran and returned to the level-5
   idle without hanging. **[VERIFIED]** -- the same breakpoint never fired in the
   unmodified run. This nails `ERMSG = GOVER OV15` as the sole blocker between the
   auto-started console and `LOGON`.

Reusable probe: **`E:\Dev\Ronny\TSS\mac-c\logon_trace_probe.sh`** -- runs nd100x
on *copies* of the images (read-only w.r.t. originals), tallies instructions per
PIL and the login-path address hit counts, and prints the verdict. Output on the
current build: `LEV2=1 ... ERMSG=2332 LOGON=0 XSTAR=0 -> console dispatched to
LEV2 but LOGON never reached (ERMSG=GOVER OV15 blocks).`

---

## 6. COULD NOT DETERMINE

1. **Whether `LOGON` then emits a visible prompt to the emulated console.** After
   the Rank-2 shim, `LOGON` executes and returns, but no banner appeared in
   nd100x's stdout/log. Whether that is because nd100x routes console-terminal
   output only to a telnet/interactive console (not the redirected stdout), or
   because `LOGON`'s output handshake (`IOX 306`/`305`) needs the terminal in a
   particular state, was not resolved here. Next step: attach `--telnet` and a
   real console, re-run the Rank-2 shim, and watch device 300-307 output.
2. ~~**Exactly why the loaded OV15 image is not runnable.**~~ **RESOLVED
   (2026-07-21) -- and the earlier framing was wrong.** The OV15 image on disc
   is **correct**, and it is **never actually read for OV15** at all. Two
   independent lines of byte/trace evidence:

   * **The disc holds a correct, fully-fixed-up OV15.** `Build/drum/tss-cdc.img`
     matches the assembled `VOR` window for OV15 (image words 072000..072777)
     **word-for-word** at its DKADR physical sectors (72*0244=027040 and
     72*0245=027050). An instrumented `)9MOVE` showed **zero** unresolved
     forward-refs/literals in any overlay window at snapshot time, so the
     staged body is fully patched (not the "snapshot-timing" hazard). See
     `docs/OVERLAY-DISC-SPEC.md` sec 8 item 1; pinned by `test_mac.c [12]`.

   * **The OV15 disc read is never issued.** A full `--start=000301` boot trace
     (`Build/drum/livetrace.txt`, ~1.04 M instr) shows the overlay-read address
     converter `DKADR` (010006) is entered **exactly twice**, both for logical
     sector `0156`/`0157` (= `2*(-1)+OVDK`, the `OVLAY=-1` boot gap-load into
     `ROVER`/`ROV4`). The OV15 logical sector `0244` is **never** requested.
     Cause: the user-side overlay dispatcher **`GOVX` (016456) executes 0
     times** over the whole run, so `OVLAY` is never set to `032`, so `S5`'s
     `SYSOV==OVLAY` test always skips the read.

   **The real blocker is that `GOVER OV15` never reaches `GOVX`.** `GOVER`
   (macro `TSS3.SYMB:15-26`) ends with `SWAP DP SX`, which sets `P:=0` and
   `X:=&(overlay-number word)`, so execution continues at **virtual address 0**,
   where the resident vector word 0 = `JMP I *+3` = `JMP I 3` → word 3 = `GOVX`
   (`TSS1.SYMB:62-65`, correctly assembled: `tss-drum.img` word0=`125003`,
   word3=`016456`). But at **PIL 2** the trace shows virtual address 0 executed
   **2336 times, always as `000000` (STZ 0), never `125003`**; it falls through
   to word 1 (`JMP I 3`) whose word-3 in the *user* map is `USTK` (020535, run
   2351 times), not `GOVX`. So at user level virtual page 0 is **not** the
   resident vector page -- GOVER dispatches into `USTK` and loops, and the
   overlay swap for OV15 is never even requested.

   This is a **runtime paging / user-context page-0 mapping** issue, entirely
   upstream of and independent from the overlay-on-disc staging (which is
   proven correct). It is **not** a `mac-c` overlay-image bug: `mac-c` produces
   the correct resident page-0 dispatch vectors and the correct OV15 disc body.
   Open sub-question (next owner): whether nd100x's MMU should map user
   virtual page 0 to resident physical page 0 (emulator side), or TSS's context
   setup (`AQCTX`/PCR/level-2 page table) is expected to do so and something in
   the boot sequence leaves page 0 private. That is a CPU/MMU/scheduler
   question, not an overlay-staging one. [VERIFIED by trace, 2026-07-21.]
3. **The `LEV6` scanner's `SUB (33` character threshold.** The scanner ignores
   input characters below `33` octal before waking a dead terminal
   (`TSS1.SYMB:2676`); the precise intent (control-char filtering vs. a specific
   wake key) was not pinned down. Not on the console path, so not blocking.
4. **The nd100x DAP quirks:** instruction breakpoints did not stop execution
   until after a `launch`+`configurationDone` had been issued *and* the target
   was re-dispatched; the CPU trace ring / attach requests returned errors. The
   verification therefore used `pause`+memory/register reads, native `--trace`
   grepping, and one confirmed breakpoint hit. [Observed behaviour, not a TSS
   fact.]
