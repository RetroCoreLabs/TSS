# NORD TSS cold-boot / interrupt-initialisation sequence

Why the overlay dispatcher skips the disc read and why interrupts are never
enabled — derived **strictly** from the TSS source
(`E:\Dev\Ronny\TSS\src\TSS1.SYMB … TSS5.SYMB`) and cross-checked against the
DRUM-build symbol dump `E:\Dev\Ronny\TSS\Build\drum\DSYMB.SYMB`.

Every claim is tagged **[VERIFIED]** (file:line + quote) or **[INFERRED]**.
Things I could not settle are in section 5. All numbers are **OCTAL** unless
marked "dec" (that is how MAC prints).

Addresses used below are the **DRUM (N10) build** values from
`Build/drum/DSYMB.SYMB` (the golden `ASYMB.SYMB` is the NN10 build and has
different addresses):

| symbol | addr | source of address |
|---|---|---|
| GOVX | 016456 | DSYMB:16 **[V]** (matches live trace) |
| OVLAY | 015205 | DSYMB:206 **[V]** |
| OVLAX | 015206 | DSYMB:376 **[V]** |
| SYSOV | 011333 | DSYMB:192 **[V]** |
| ROVER / ROV4 | 031377 / 031777 | DSYMB:193,194 **[V]** |
| INIT | 000301 | DSYMB:39 **[V]** |
| LEV0 / LEV5 | 006104 / 006324 | DSYMB:79,42 **[V]** |
| ISTRT / START / XSTAR | 025076 / 025101 / 033601 | DSYMB:202,325,453 **[V]** |
| OVDK | 000160 | DSYMB:175 **[V]** |
| OV19 | 000036 | DSYMB:674 **[V]** |
| CORLD | 000060 | DSYMB:20 **[V]** |
| MSTRT | 040000 | DSYMB:81 **[V]** |
| ENABL (cell) | 000535 (holds 77157) | DSYMB:77 **[V]** / `TSS1.SYMB:300` **[V]** |

---

## 1. GOVX disassembly and the "overlay-loaded" cell

### 1.1 GOVX in full

**[VERIFIED]** `TSS2.SYMB:181-187`:

```
GOVX, SWAP DX SL; STA SAVA; STX SAVL; COPY DX SL     ; 016456..016461 save regs
   LDA 0,X; SUB I 9OVL; JAZ *+7                       ; 016462..016464 "already loaded?"
   LDA I 9OVL; STA I (OVLAX; LDA 0,X; STA I 9OVL      ; 016465..  save old OVLAY, set OVLAY:=req
   SAA 40; MST PID; LDA SAVL; COPY DL SA              ; trigger LEVEL 5, restore L
   LDA SAVA; LDX 1,X; STX SAVST                       ; SAVST := 2nd GOVER word (overlay entry)
   LDX SAVX; JMP I SAVST                              ; jump into the overlay body
9OVL, OVLAY                                           ; 016502: pointer word = addr of OVLAY
```

This maps 1:1 onto the live trace `016462 LDA ,X 0 ; 016463 SUB I 17 ; 016464
JAZ 7`: `LDA 0,X` = the overlay number from the `GOVER` call site,
`SUB I 9OVL` = subtract-indirect through the pointer word `9OVL`, and the trace's
displacement `17` is exactly `9OVL - (SUB address)` = `016502 - 016463` **[V]**.
`JAZ 7` = `JAZ *+7`, the skip-the-load branch.

### 1.2 The cell is OVLAY (015205)

**[VERIFIED]** `TSS2.SYMB:187` `9OVL, OVLAY` — the pointer word `9OVL` contains
the **address of OVLAY**. `SUB I 9OVL` therefore subtracts the **contents of the
OVLAY cell** (015205) from the requested overlay number. If they are equal
(`JAZ`) GOVX concludes the overlay is resident and **skips straight to the
overlay body** without triggering a load.

Declared **[VERIFIED]** `TSS2.SYMB:78` `OVLAY, 0 %OVERLAY CELL FOR THE USER`.
A *second* cell, **SYSOV** (011333, "OVERLAY CELL FOR THE SYSTEM"
**[VERIFIED]** `TSS1.SYMB:4539` `SYSOV, 0`), records what is *physically* in
`ROVER`; it is the cell the disc-reader **S5** tests (section 2.2). `OVLAX`
(015206) holds the previous overlay **[VERIFIED]** `TSS2.SYMB:80`.

### 1.3 What the cells hold on the cold assembled image — and why the check passes

They are **not** left at their declared `0`. The `OVERX` macro that closes every
overlay body **overwrites all three cells with `RQR-1`** at assembly time.

**[VERIFIED]** MACF path `TSS3.SYMB:88-90`:
```
OVLAY/ RQR-1        % '/' sets the location counter to OVLAY, then emits the word RQR-1
OVLAX/ RQR-1
SYSOV/ RQR-1
```
(the NMACF path is identical, `TSS3.SYMB:73-75`). `RQR` is the running overlay
counter, bumped by each `OVERL` **[VERIFIED]** `TSS3.SYMB:56-61`, starting at 0
**[VERIFIED]** `TSS2.SYMB:171` `RQR=0`. At the **last** overlay assembled —
`OV19` (the command processor), `OVERL OV19 … OVERX QOV19` is the final pair,
`TSS5.SYMB:1786…1969` **[VERIFIED]** — `RQR-1 = 36 = OV19`
(`OV19=000036` **[VERIFIED]** DSYMB:674).

**So on the cold image `OVLAY = SYSOV = OVLAX = 000036 = OV19`.** [VERIFIED by
construction; and by the live trace, where `016464 JAZ 7` is taken.]

The very first overlay call on the normal path is `XSTAR = GOVER OV19,S1,XSTAR`
**[VERIFIED]** `TSS5.SYMB:1808`, i.e. it requests overlay 36. GOVX computes
`36 - OVLAY(36) = 0` → `JAZ` → **believes OV19 is resident → skips the load** and
jumps to the overlay entry `S1` inside `ROVER`. That is the false pass.

### 1.4 Why this is "correct by design" — and why it still dies for us

**[INFERRED, strong]** The preset is deliberate: after generation the overlay
physically resident in `ROVER` **is** the last one assembled (OV19), so
`OVLAY=SYSOV=36` is *truthful* and the first `GOVER OV19` is meant to run the
resident copy directly, with **no disc read**. Disc reads only occur for
*subsequent, different* overlays.

The failure in our run is therefore **not** that the flag is wrong in the
abstract — it is that our in-core `ROVER` does not contain a runnable OV19 (the
`)9MOVE ROVER VOR VORS` staging and the true saved-coreload are not reproduced),
so jumping into `ROVER` runs garbage. See `docs/OVERLAY-DISC-SPEC.md` for the
staging path.

### 1.5 Required value to force a genuine disc load of OV19

To make GOVX ignore the stale `ROVER` and read OV19 from the CDC disc, **both**
cells must be an *invalid* sentinel (any value that is not a real overlay number
0…36):

* `OVLAY (015205)` must be `≠ 36` so GOVX does **not** take `JAZ`, and instead
  sets `OVLAY:=36` and triggers level 5.
* `SYSOV (011333)` must be `≠ 36` so the level-5 reader **S5** (which tests
  `SYSOV − OVLAY`, section 2.2) actually performs the read rather than skipping.

Recommended sentinel: `177777` (−1) in **both** cells. Trace with that value:

```
GOVER OV19 (req=36)
  GOVX: 36 − OVLAY(-1) = 37 ≠ 0  → no skip; OVLAX:=-1; OVLAY:=36; SAA 40;MST PID (kick L5)
     L5 preempts → scheduler → S5: SYSOV(-1) − OVLAY(36) ≠ 0 → READ sector OVDK+2*36
        = 254/255 into ROVER/ROV4; SYSOV:=36; return
  GOVX continuation → JMP I SAVST (=S1) → runs the freshly-loaded OV19
```

This only works if interrupts and the level-5 reader are up (section 2); with
interrupts off, `MST PID` sets the request but it is never serviced and GOVX
falls through into stale `ROVER` — exactly the observed hang.

---

## 2. Interrupt-system bring-up, the level-5 reader, and why our entry misses it

### 2.1 Page-zero vectors and the real init routine

Page zero holds the software dispatch vectors **[VERIFIED]** `TSS1.SYMB:62-70`:
```
0/ JMP I *+3          ; loc 0 -> loc 3 = GOVX   (the overlay dispatcher)
1/ JMP I *+3          ; loc 1 -> loc 4 = USTK
2/ JMP I *+3          ; loc 2 -> loc 5 = STK
3/ GOVX; USTK; STK
6/ MCTBL              ; monitor-call table pointer
7/ LDT *+3; JMP I *+1; SYSSV; CORLD   ; save/restore-system trap
```
and a **restart vector** at location 20 **[VERIFIED]** `TSS1.SYMB:197`:
```
20/ JMP I *+1; 301    ; loc 20 -> loc 21 = 301 = INIT
```

The actual hardware/level vectors and the interrupt **enable** are done by
**INIT** (address 000301), the N10 body **[VERIFIED]** `TSS1.SYMB:237-290`:

```
INIT, IOF; POF                                   ; ints + paging off
   SAA 0; TRR PIE; LDA (*+2; IRW 0 DP; ION; IOF  ; clear PIE, prime level-0 P
   LDA (LEV2; IRW 20 DP                          ; arm level 2  (utility/user)
   LDA (LEV3; IRW 30 DP; LDA (LEV5; IRW 50 DP     ; arm level 3, LEVEL 5 (swapper/overlay reader)
   LDA (LEV6; IRW 60 DP; LDA (LEV9; IRW 110 DP    ; arm level 6, 9
   LDA (LEV10;IRW 120 DP;LDA (LEV12;IRW 140 DP    ; arm level 10, 12  (I/O)
   LDA (LEV11;IRW 130 DP                          ; arm level 11      (I/O)
   LDA (LEV13;IRW 150 DP;LDA (LEV14;IRW 160 DP    ; arm level 13 (clock), 14 (MON/trap)
   ... clear buffers / locks / timers ...          ; TSS1:247-266
   SAA -1; MCL PID; MCL PIE                        ; clear pending
   LDA ENABL; MST PIE                             ; **set the PIE mask** = 77157
   SAA 40; MST PID                                ; **kick level 5 once** (start scheduler)
   LDA (1136; TRR IIE; JPL IPGTB                  ; internal-int enable + init paging
   LDA (310; IOX 11                               ; device setup
   LDA (122001; IOX 13                            ; **program the RTC clock** (level 13)
   ION; JMP I (LEV0                               ; **interrupts ON**, hand to the idle level
```

* Level-5 handler armed: **[VERIFIED]** `TSS1.SYMB:241` `LDA (LEV5; IRW 50 DP`
  (register slot 50 = level 5). `LEV5=006324` DSYMB:42.
* PIE mask: **[VERIFIED]** `TSS1.SYMB:285` `LDA ENABL; MST PIE`, with
  `ENABL, 77157` **[VERIFIED]** `TSS1.SYMB:300`. Octal `77157` has **bit 5 set**
  (levels 2,3,5,6,9,10,11,12,13,14 enabled) — so level 5 and the level-13 clock
  are both unmasked. [VERIFIED by bit expansion of 77157.]
* Interrupts enabled: **[VERIFIED]** `TSS1.SYMB:290` `ION; JMP I (LEV0`.
* Level 5 primed once so the scheduler runs immediately: **[VERIFIED]**
  `TSS1.SYMB:286` `SAA 40; MST PID`.

### 2.2 The level-5 overlay reader (S5)

`GOVX`'s `SAA 40; MST PID` requests **level 5** (`40` octal = bit 5). The level-5
handler is the scheduler/swapper `LEV5` **[VERIFIED]** `TSS1.SYMB:2705`, whose
swap-in path reaches **S5**, the overlay reader **[VERIFIED]**
`TSS1.SYMB:3069-3073`:
```
S5, LDA I (SYSOV; SUB I 9OVL; JAZ S5X            ; SYSOV == OVLAY ? already loaded -> skip
   LDA I 9OVL; STA I (SYSOV; SHA 1; ADD (OVDK    ; SYSOV:=OVLAY; A := OVLAY*2 + OVDK
   COPY DT SA; LDX (ROVER; SAA 1; JPL I 9SDK     ; READ sector       -> ROVER
   AAT 1;      LDX (ROV4;  SAA 1; JPL I 9SDK     ; READ sector+1     -> ROV4
```
with `9OVL, OVLAY` **[VERIFIED]** `TSS1.SYMB:3135` and `9SDK, SDISK`
`TSS1.SYMB:3136`. This is the routine that actually touches the CDC disc; GOVX
only flags the request. (The clock chain that keeps the scheduler running:
level-13 RTC `LEV13` **[VERIFIED]** `TSS1.SYMB:3996` triggers level 9
`LEV9` **[VERIFIED]** `TSS1.SYMB:4009`, which does `SAA 40; MST PID` to poke
level 5 **[VERIFIED]** `TSS1.SYMB:4016`.)

### 2.3 Why entering at ISTRT misses all of it

**[VERIFIED]** `TSS2.SYMB:1846-1855`:
```
ISTRT, SAA 1; STA I (IDEV; STA I (ODEV
START, JPL I (ISTK; LDA I (RMODE; JAZ LEV2A
LEV2, ... JPL I (LOGON
LEV2A, JMP I (XSTAR
```
`ISTRT`/`START`/`LEV2A` contain **no** `IRW` vector writes, **no** `MST PIE`, and
**no** `ION`. They set the console device, the stack, read `RMODE`, and jump to
`XSTAR`. Nothing here arms level 5 or enables interrupts. That is exactly why the
live run shows essentially no interrupt activity (1 ION/IOF, 0 IOX): the code
that would enable them — **INIT** — is never executed on the ISTRT path.

`ISTRT` is in fact the **per-user restart address**, not a machine init: the
swapper writes it into a user's level-2 P-register when it (re)dispatches that
user **[VERIFIED]** `TSS1.SYMB:3110-3114` (`LDA (ISTRT; SAT 1; IRW 20 DP; JPL I
(SXBRK`). So `ISTRT` is meant to be reached *from the scheduler, after INIT has
already brought the machine up*, not entered cold.

---

## 3. The true cold-boot entry and ordered sequence

### 3.1 There is init code before ISTRT: DBOOT → INIT

The genuine cold entry is **INIT (000301)**, reached two ways, both independent
of ISTRT:

1. **Disc bootstrap DBOOT** **[VERIFIED]** `TSS3.SYMB:265-268`:
   ```
   DBOOT, STZ DBCOR; LDA (CORLD; STA DBDKA
   DBLOP, LDX DBCOR; LDT DBDKA; JPL RDKOP        ; read one 256-word sector coreload->core
      MIN DBDKA; LDA (400; ADD DBCOR; STA DBCOR
      SUB (MSTRT; JAN DBLOP; JMP I (301          ; until core reaches MSTRT, then JMP INIT
   ```
   DBOOT reads the **coreload** (the resident system image) from disc starting at
   sector **CORLD=000060** **[VERIFIED]** (`CORLD=CORLX=CORL1=DKBIT+DKS+10 = 60`,
   `TSS1.SYMB:2586,2612,2619`; DSYMB:20) into core `0 … MSTRT=040000`
   **[VERIFIED]** DSYMB:81, then `JMP I (301` = **INIT**. DBOOT/RDKOP themselves
   are the tiny bootstrap dumped to disc sectors 0-2 via `)8DUMP`
   **[VERIFIED]** `TSS3.SYMB:284-296`, loaded by the hardware disc-load microcode.
2. **Restart vector** location 20 → 301 **[VERIFIED]** `TSS1.SYMB:197`, and the
   system-save routine ends by re-entering INIT: **[VERIFIED]** `TSS1.SYMB:4082`
   `SYSSV, … MIN SYSCT; JMP SYSLP; JMP I (INIT`.

### 3.2 The ordered sequence

```
[hardware disc LOAD of sector 0]                 (DBOOT/RDKOP bootstrap)
  -> DBOOT (TSS3): read coreload sectors from CORLD(60) into core 0..MSTRT(40000)
  -> JMP 000301 = INIT
       INIT (TSS1:237): IOF/POF; arm level vectors 2,3,5,6,9,10,11,12,13,14 via IRW;
                        clear buffers/locks; MST PIE = ENABL(77157); MST PID lvl5;
                        init paging (IPGTB); program RTC (IOX 13); ION;
       -> JMP LEV0 (006104)  idle/background level
            RTC (lvl13) -> lvl9 -> lvl5 scheduler runs
            scheduler finds process slot (status 0 => "initialise me", TSS1:2638)
            swap-in path sets user P := ISTRT and dispatches the user at level 2
  -> ISTRT (025076) -> START -> (RMODE=0) -> LEV2A -> XSTAR (033601)
       XSTAR = GOVER OV19  -> GOVX (016456)
         if OVLAY!=36: kick lvl5 -> S5 reads OV19 from CDC disc (sector 254/255) -> ROVER
         run OV19 (command processor) -> LOGON prompt
```

**[VERIFIED]** each labelled step above by the citations in sections 1-2 and the
DSYMB addresses. The scheduler→dispatch→ISTRT linkage and the "status 0 =>
initialise" first-user creation are **[INFERRED, strong]** from
`TSS1.SYMB:2638` (PRT status comment) and `TSS1.SYMB:3110-3114` (swapper writes
ISTRT into the user P).

### 3.3 What must be true before entry

* Core `0…040000` holds the resident coreload (page-zero vectors, INIT, GOVX,
  LEV5/S5, the level handlers, OVLAY/SYSOV, ROVER). Our freshly-assembled image
  supplies this. **[VERIFIED]** MSTRT=040000, coreload span from DBOOT.
* The CDC system disc (IOX channel **500** in the N10 build) is mounted with the
  overlay image at `OVDK+2n` (OV19 at sectors 254/255). **[VERIFIED]**
  `docs/OVERLAY-DISC-SPEC.md` §2, DSYMB OVDK=160, OV19=36.
* The RTC exists and is programmed (INIT does `IOX 13`) so level 13→9→5 keeps
  the scheduler alive. **[VERIFIED]** `TSS1.SYMB:289,3996-4016`.
* **Enter at INIT (000301), not ISTRT (025076).**

**ISTRT is not the cold entry.** It assumes interrupts are already on, the
level-5 reader armed, and the overlay system primed — all of which are INIT's job.

---

## 4. Minimal actionable init for the emulator (ranked)

Given control over start PC, memory patches, the CDC overlay disc, and the RTC:

### Rank 1 — highest confidence: enter at INIT, force the first load
* **Start PC = 000301 (INIT).**  [reproduces the real path; VERIFIED INIT=301]
* **Patch `OVLAY (015205) = 177777` and `SYSOV (011333) = 177777`** before start.
  Forces GOVX to trigger level 5 and S5 to actually read OV19 from disc, instead
  of trusting the stale in-core `ROVER`. [VERIFIED cells/addresses; the sentinel
  value is INFERRED from the compare logic in §1.5/§2.2]
* **Mount the CDC overlay disc at IOX 500** with OV19 at sectors 254/255.
  [VERIFIED contract, OVERLAY-DISC-SPEC §2]
* **RTC running.** INIT programs it (`IOX 13`), then `ION`. [VERIFIED]
* Nothing else: INIT itself arms every level vector, sets PIE=77157, enables
  interrupts, primes level 5, and jumps to LEV0. The scheduler then dispatches
  the first user to ISTRT→XSTAR.

If `ROVER` in the loaded image already holds a *runnable* OV19, the two patches
are unnecessary (GOVX would run it directly); patching them is the robust choice
because our image's ROVER staging is not the true saved coreload (§1.4).

### Rank 2 — medium: stay at ISTRT, replicate INIT's effect by hand
Before start, reproduce what INIT does, then start at ISTRT (025076):
* Write level P-registers (IRW slots): level 2←LEV2, 3←LEV3, **5←LEV5(006324)**,
  6,9,10,11,12,13←their handlers, 14←LEV14 — the full set from `TSS1.SYMB:240-245`.
* `MST PIE` with mask = 77157 (ENABL); `ION`; program RTC (`IOX 13` value
  122001); init paging (IPGTB path).
* Patch `OVLAY=SYSOV=177777`.
More fragile: you must faithfully replay every `IRW`, the paging init, and the
`IOX 11/13` device setup that INIT does. Prefer Rank 1, which runs that code for
you. [INFERRED from INIT body]

### Rank 3 — insufficient alone (documents a dead end)
Patch only `OVLAY=SYSOV=177777` and enter at ISTRT. GOVX will request level 5,
but with interrupts still **off** (ISTRT never does `ION`/`MST PIE`) the request
is never serviced and GOVX falls through into stale `ROVER` — the same hang.
This matches the observed "1 ION/IOF, 0 IOX". [VERIFIED by the ISTRT code
containing no enable, §2.3.]

### Fully faithful (heaviest)
Build/obtain a real **saved coreload** on disc (a SYSSV image where a consistent
overlay is resident) and cold-boot via the **DBOOT** disc bootstrap (sector 0),
which loads it and jumps to INIT. This is the historically exact path but
requires reproducing SYSSV/generation, which is outside the assembled image.
[VERIFIED DBOOT/SYSSV exist; producing the saved image is not reproduced here.]

---

## 5. COULD NOT DETERMINE (from the TSS source)

1. **The exact scheduler → first-user → ISTRT dispatch instructions.** That
   `LEV5` (006324) walks the PRT, sees a status-0 slot, and swaps a user in at
   `ISTRT` is **[INFERRED, strong]** from the PRT status comment
   (`TSS1.SYMB:2638`) and the swapper writing `ISTRT` into the user P
   (`TSS1.SYMB:3110-3114`); I did not single-step the full `LEV5→SWAPR→S4→S5→S6…`
   chain to prove the first dispatch lands on ISTRT with no prior console input.
2. **Whether INIT alone yields a login prompt without any operator input.**
   `LEV2`/`LOGON` vs `LEV2A`/`XSTAR` is chosen by `RMODE` (`TSS2.SYMB:1847`);
   `RMODE=0` → `XSTAR`. Whether a real first boot needs the operator `22!`/`10,0$`
   console step or an `RMODE` preset was not determinable from the source (those
   commands live in MACM/opcom, outside this tree).
3. **The precise interrupt latency assumption in GOVX.** GOVX does `MST PID`
   then, a few instructions later, `JMP I SAVST` into the overlay. That the
   level-5 read completes *before* that jump (because level 5 preempts level 2
   and the scheduler re-dispatches level 2 only after S5 returns) is
   **[INFERRED, strong]** from the level ordering, not proven by trace here.
4. **The N10 CDC linear→geometry mapping for sectors 254/255.** `DKADR`
   (`TSS1.SYMB:3563-3623`, DSYMB DKADR=010006) is present but not hand-executed;
   if the emulated disc is linear-sector addressed it is not needed.
5. **The contents of `ROVER` in the mac-c assembled image.** Whether it holds a
   runnable OV19 or garbage was not verified byte-by-byte here; §1.4 treats it as
   untrusted and routes around it by forcing the disc read.

---

### Appendix: reusable script

`E:\Dev\Ronny\TSS\mac-c\cold_boot_init.py` — reads a DRUM `)LIST`/symbol dump
(`Build/drum/DSYMB.SYMB`) and prints the cold-boot init cells (OVLAY, SYSOV,
OVLAX, INIT, LEV5, LEV0, ROVER, GOVX, ISTRT, OVDK, OV19) with the recommended
Rank-1 emulator patch set. Read-only; does not touch `mac.c`/`main.c` or any TSS
source.
