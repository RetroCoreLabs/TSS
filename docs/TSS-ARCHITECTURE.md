# NORD TSS 3.0 — Operating-System Architecture

**Status:** Source-grounded architecture overview of the recovered 1973
NORD TSS 3.0 (Bo Lewendal), read directly from `src/TSS1.SYMB` …
`src/TSS5.SYMB`. Every factual claim is tagged **[VERIFIED]** with a
`src/TSSn.SYMB:LINE` citation for where it was read, or **[ASSUMPTION]**
for interpretation, or **NOT DETERMINED FROM SOURCE** where the source
does not settle it. This document covers OS structure, memory/paging, the
process model, scheduling and interrupt levels, the monitor-call interface,
cold-start, and the overlay subsystem. It deliberately does **not** re-cover
disc layout / user init (see `docs/DISK-INIT-USERS-AND-BOOT.md`,
`docs/CDC-DISC-DEVICE.md`, `docs/OVERLAY-DISC-SPEC.md`), the boot chain in
detail (`docs/BOOT-PROCESS.md`, `docs/PROJECT-DESCRIPTION.md` §3), or the
MAC binary (`docs/MAC-BPUN-Analysis.md`).

---

## 1. The five-part structure

TSS is assembled as five MAC source parts, `TSS1`–`TSS5`, in that order
(the golden build stream is `)9ASSM TSS1,LIST1,0` … `)9ASSM
TSS5,LIST5,ASYMB:SYMB`; see `docs/PROJECT-DESCRIPTION.md` §5). What each
part contains, from its own contents:

| Part | Lines | Role — **[VERIFIED]** by cited content |
|---|---|---|
| **TSS1** | 4738 | The **resident kernel / machine layer**. Opens with the library-mark / configuration block (`src/TSS1.SYMB:1`–`159`); low-core vectors and the save-system word at location 7 (`src/TSS1.SYMB:62`–`70`); `INIT` interrupt-vector setup (`src/TSS1.SYMB:230`–`309`); paging init `IPGTB` (`src/TSS1.SYMB:323`); the interrupt-level routines `LEV0`,`LEV2`,`LEV5`,`LEV6`,`LEV7`,`LEV9`–`LEV14`; the scheduler+swapper `LEV5`/`SWAPR` (`src/TSS1.SYMB:2716`+); disc-address map (`src/TSS1.SYMB:2589`+); the process table `PRT` (`src/TSS1.SYMB:2639`+); the monitor-call transfer vector `MCTBL` and processor `LEV3` (`src/TSS1.SYMB:4300`–`4366`); the software-instruction emulator `LEV14`/`DECOD` and the memory tables `PCTBL`/`PDTBL` (`src/TSS1.SYMB:4704`,`4709`). |
| **TSS2** | 3528 | The **per-process context block, memory/paging state, terminal I/O and command-utility layer, and file buffers**. Opens `%CONTEXT BLOCK` with the three register blocks (`src/TSS2.SYMB:1`–`37`), the virtual-core table `VCTBL` (`src/TSS2.SYMB:43`), I/O and utility variables, the stack and the overlay-enter routine `GOVX` (`src/TSS2.SYMB:196`–`206`), the `PROGM`/`DATA`/`GLOBL` program-frame macros (`src/TSS2.SYMB:244`–`263`), and page-fault / virtual-address handling that references `40000` and `NPGS` (`src/TSS2.SYMB:393`,`443`,`468`). |
| **TSS3** | 1754 | **Overlay machinery, the hardware bootstrap + TSS loader, and debug/utility overlays.** Defines the `GOVER`/`SOVER`/`OVERL`/`OVERX` overlay macros (`src/TSS3.SYMB:9`–`98`), the hardware bootstrap and TSS loader (`src/TSS3.SYMB:99`–`239`), the disk restart routine (`src/TSS3.SYMB:240`+), and register/deposit debug commands `REGS`/`SETX`/`CMOD` (`src/TSS3.SYMB:305`+). |
| **TSS4** | 2039 | **User-image save/restore and user-directory services.** `DUMP` — dump a user's address space to a file (`src/TSS4.SYMB:4`); `RECV` — restart a user from a file (`src/TSS4.SYMB:78`); `BROAD` broadcast (`src/TSS4.SYMB:163`); `GUNUM`/`GUNAM` user-name ↔ user-number conversion (`src/TSS4.SYMB:194`,`217`). |
| **TSS5** | 1994 | **User-account administration.** `DLUSR` delete user (`src/TSS5.SYMB:4`), `CRUSE` create user (`src/TSS5.SYMB:61`), `IUSER` initialize user (`src/TSS5.SYMB:117`), `GETUN` get user number (`src/TSS5.SYMB:146`). TSS5 is where the golden `)LIST` fires, so it terminates the build. |

**[ASSUMPTION]** The ordering is load-bearing for the kernel: TSS1 defines
the machine layer and all symbols the later parts reference (register-block
offsets, `NPGS`, disc addresses), TSS2 lays down the resident per-process
data the kernel indexes, and TSS3–5 add services on top. This is inference
from the symbol dependencies, not a statement in the source.

---

## 2. Memory and virtual-memory model

### 2.1 Physical layout

**[VERIFIED]** The resident system occupies low core; the user (virtual)
address space begins at **`MSTRT`**, which for the shipped 16K build is
octal **`40000`**: `"K16 … MSTRT=40000` (`src/TSS1.SYMB:4650`–`4651`; the
12K build uses `30000`, `src/TSS1.SYMB:4646`–`4647`). `TDUMP` independently
fixes `MSTRT=40000` (`src/TDUMP.SYMB:59`) and dumps user core starting there
(`src/TDUMP.SYMB:20`).

**[VERIFIED]** A user address space is **`NPGS` pages** and `NPGS=10` octal
= 8 pages (`src/TSS1.SYMB:4657`–`4659`). **[ASSUMPTION]** Each page is 2K
words: the context-block comment labels `VCTBL` "(IN 2K BLOCKS)"
(`src/TSS2.SYMB:43`–`44`) and `MSTRT=40000` + 8×2K spans to `60000`. 8×2K =
16K, matching the "16K system" `K16` build. **[VERIFIED]** the swapper
computes a page's core address as `page*2048` via `LDA II; SHA 13; ADD
9MSTR` — a 13-bit left shift = ×`10000` octal = 2048 (`src/TSS1.SYMB:2873`),
confirming the 2K page.

### 2.2 The three page tables

**[VERIFIED]** Three tables, one entry per page, hold the paging state:

- **`VCTBL`** — Virtual Core Table, resident per-process, `NPGS+NPGS`
  words (2 words/page) (`src/TSS2.SYMB:43`; comment `%VIRTUAL CORE TABLE`,
  `src/TSS2.SYMB:41`). It is what the user's address space *should* contain.
  Bit fields used by the swapper: `$W0` (word 0 = present/identity), `$X`
  (exists-on-drum flag, tested `BSKP ZRO 150 DA`, `src/TSS1.SYMB:2856`),
  `$MD` (mode), `$INDX` (drum index).
- **`PCTBL`** — Physical Core Table, `NPGS` words (`src/TSS1.SYMB:4704`,
  `%PHYSICAL CORE TABLE`). What is *actually* resident in each core page
  frame right now. `$MD` = 2 → read-only, = 3 → read/write
  (`src/TSS1.SYMB:2865`,`2880`).
- **`PDTBL`** — Physical Drum Table, `NDPGS+NDPGS` words (2/page)
  (`src/TSS1.SYMB:4709`, `%PHYSICAL DRUM TABLE`). Backing-store slot
  descriptors with a usage `$COUNT` for shared read-only pages
  (`src/TSS1.SYMB:2893`,`2950`). `NDPGS` (number of swap pages) scales with
  the terminal count `TEL*`; default `NDPGS=X4` where `X4=8*(NPGS+1)`
  (`src/TSS1.SYMB:4668`–`4695`).

### 2.3 NORD-10 hardware paging vs NORD-1

**[VERIFIED]** On the NORD-10 (`"N10`) the paging hardware is programmed at
init by `IPGTB` (`src/TSS1.SYMB:302`,`323`): it loops loading the paging
control register `TRR PCR` (`src/TSS1.SYMB:325`), then fills the page
registers `LDA (163000; ADD CNT; STA I CNT,X` over a computed count derived
from `MSTRT` and `NPGS` (`src/TSS1.SYMB:328`–`332`), and enables paging
with `PON` (`src/TSS1.SYMB:333`). **[VERIFIED]** On the NORD-1 (`"NN10`)
there is no paging hardware path in `INIT`; the `IPGTB` routine is entirely
inside `"N10` (`src/TSS1.SYMB:320`–`339`). **[ASSUMPTION]** the NORD-1 runs
the user in the resident 16K with the swapper moving whole 2K pages between
core and backing store rather than using address translation — inferred
from the absence of any NORD-1 page-register code and the presence of the
same `SWAPR` copy loop for both variants.

### 2.4 Swapping (core ⇄ mass storage)

**[VERIFIED]** The swapper `SWAPR` (`src/TSS1.SYMB:2794`+) is annotated with
ALGOL-style pseudocode. It (a) writes the outgoing user's context/register
block to swap area `DKA1+(PRT[0]<<3)` via `TRSFR` (`src/TSS1.SYMB:2811`,
`2818`–`2819`), (b) reads the incoming user's block (`src/TSS1.SYMB:2827`–
`2831`), then (c) for each of the `NPGS` pages reconciles `VCTBL` against
`PCTBL`: pages present in core but stale are written back if read/write
(`$MD=3`, `src/TSS1.SYMB:2865`–`2874`), cleared unless read-only
(`src/TSS1.SYMB:2880`–`2883`), and the required pages are read in from
`PDTBL`/drum (`src/TSS1.SYMB:2885`+). **[VERIFIED]** page transfers use
`MSTRT+I*2048` as the core end and `DKA1+(index<<3)` as the mass-storage
end (`src/TSS1.SYMB:2869`–`2874`, `2898`).

**[VERIFIED]** `TRSFR` is the swap front end that chooses drum vs disc: with
the `DRUM` mark, pages below `4*DRMSZ` go to the drum and above it to the
disc; without `DRUM`, `TRSFR = TRXX` (disc only) — stated in
`docs/PROJECT-DESCRIPTION.md` §6, drawn from `src/TSS1.SYMB` lines 3695–
3877. The archived golden builds set neither `DRUM` nor `N10`, so the drum
is compiled out (see `CLAUDE.md` corpus notes).

Per-user backing store is twelve fixed swap areas `DKA1`…`DKA12`, each
`200` octal words apart, based at `USRDK+10` (`src/TSS1.SYMB:2611`–`2622`).

---

## 3. The process model

### 3.1 What a process is

**[VERIFIED]** There is **one process per terminal**. The process table
`PRT1` is `BSS NTTY+NTTY+…` (8 words × number of teletypes) with
`PRT2, BSS NTTY*8-20` following (`src/TSS1.SYMB:2642`–`2659`), and the
per-entry `%TTY USAGE` word documents the life-cycle: negative = dead
(scanner owns the TTY), zero = scheduler will initialize this process,
positive = alive/in use (`src/TSS1.SYMB:2650`–`2654`).

**[VERIFIED]** Each `PRT` entry is 8 words (`src/TSS1.SYMB:2642`–`2657`):
0 = PDTBL index of the context block, 1 = quantum timer, 2–3 = compute
timer, 4 = status (bit15 block, bit14 rubout, bit13 block-on-output,
bit12 logon-timer interrupt, bit11 I/O interrupt), 5 = TTY usage, 6 = free,
7 = saved PID. `PRT` (`src/TSS1.SYMB:2663`) is a pointer to the *current*
process entry.

### 3.2 The context block

**[VERIFIED]** The resident per-process context block is defined at the head
of TSS2 under `%CONTEXT BLOCK` (`src/TSS2.SYMB:1`+) and consists of three
register images plus the memory/IO state:

- **`RBLOK`** (`BSS 11`, `src/TSS2.SYMB:7`) — the **user** register block:
  P at `RBLOK`, then X,T,A,D,L,STS,B,MPR at offsets `+1`…`+10`
  (`src/TSS2.SYMB:13`–`20`).
- **`UBLOK`** (`BSS 10`) + `UMPR` (`src/TSS2.SYMB:24`–`25`) — the register
  block for the **utility** (command interpreter) level.
- **`MBLOK`** and its named fields `XRG,TRG,ARG,DRG,LRG,SRG,BRG,MRG`
  (`src/TSS2.SYMB:29`–`37`) — the register block for the **MCALL
  processor** (level 3).

Following it: the `VCTBL` (§2.2), the terminal I/O locals (`TDEV`,`TCH`,
`IDEV`,`ODEV`… `src/TSS2.SYMB:48`–`55`), the utility/command variables
(`CMDLN`, `RUBAD`, `RMODE`, `PRNUM`, `OVLAY` overlay cell, `src/TSS2.SYMB:
59`–`80`), locks (`LOGLK`,`PASLK`,`ELOCK`,`KMAIL`,`KTTYL`,`KACCL`,
`src/TSS2.SYMB:82`–`94`), the open-file machinery (`FBUF`,`OFT`,`OFTU`,
`src/TSS2.SYMB:141`–`177`), and the per-process **stack** `STK0, BSS STLEN`
with `STLEN=1040` octal (`src/TSS2.SYMB:189`,`210`).

### 3.3 Save / restore

**[VERIFIED, NORD-1]** Context save/restore uses the hardware register-file
`STF`/`LDF` register-block instructions. The `SAVE`/`UNSAV` pair at
location `40` stores/loads the full register file plus STS and MPR through
an X-indexed block, ending in `WAIT` (`src/TSS1.SYMB:197`–`206`); the vector
list at `20/` maps the 16 interrupt levels to their save areas
(`src/TSS1.SYMB:197`).

**[VERIFIED, NORD-10]** `RRBLK`/`WRBLK` read and write the three register
blocks with the NORD-10 `SRB`/`LRB` (save/load register block) instructions:
`LDX (RBLOK; SRB 10` for the user block on level 10, `UBLOK` level 20,
`MBLOK` level 30 (`src/TSS1.SYMB:2772`–`2783`). The swapper calls `RRBLK`
before writing a context out and `WRBLK` after reading one in
(`src/TSS1.SYMB:2816`,`2833`).

---

## 4. Scheduling and interrupt levels (PIL)

### 4.1 The level assignment

**[VERIFIED]** `INIT` installs the interrupt handlers. On the NORD-10 it
loads each level's P-register through `IRW nn DP` (`src/TSS1.SYMB:252`–`260`),
on the NORD-1 it stores handler addresses into the fixed low-core vector
cells (`src/TSS1.SYMB:234`–`250`). The mapping read from those two blocks:

| PIL level | Symbol | Role — **[VERIFIED]** citation |
|---|---|---|
| 0 | `LEV0` | Traps the `WAIT` and hands control to level 2 (utility). `%TRAPS THE WAIT INSTRUCTION AND SENDS CONTROL TO LEVEL 2` (`src/TSS1.SYMB:2569`–`2586`). Installed at cell `61` / level 0 (`src/TSS1.SYMB:237`,`253`). |
| 2 | `LEV2` | Utility / command-interpreter level; its handler address is stored into `UBLOK` (`src/TSS1.SYMB:238`,`255`). **[ASSUMPTION]** this is the level the user command loop runs at, from the `UBLOK` = "utility" pairing (`src/TSS2.SYMB:22`). |
| 3 | `LEV3` | **Monitor-call processor** (§5). Stored into `MBLOK`/level 30 (`src/TSS1.SYMB:239`,`256`; body `src/TSS1.SYMB:4322`+). |
| 5 | `LEV5` | **Scheduler + swapper** (`src/TSS1.SYMB:240`,`256`; body `src/TSS1.SYMB:2716`+). |
| 6 | `LEV6` | **Teletype scanner**, `%RUNS EVERY 80 MILLISECONDS` (`src/TSS1.SYMB:241`,`257`; body `src/TSS1.SYMB:2668`–`2710`). |
| 7 | `LEV7` | Device-interrupt handler (`src/TSS1.SYMB:242`; body `src/TSS1.SYMB:1860`). |
| 9 | `LEV9` | Subsidiary clock: divides the tick, enables the TTY scanner and the scheduler, ages response/compute timers (`src/TSS1.SYMB:243`; body `src/TSS1.SYMB:4037`–`4093`). |
| 10 | `LEV10` | NORD-10 device level using `IDENT PL10` (`src/TSS1.SYMB:258`; body `src/TSS1.SYMB:1890`). |
| 11 | `LEV11` | Input-interrupt routine (card reader, paper tape, modem, teletype input); on NORD-1 uses `IOT SNI`, on NORD-10 `IDENT PL11` (`src/TSS1.SYMB:244`,`259`; bodies `src/TSS1.SYMB:1387`+, `1816`+). |
| 12 | `LEV12` | NORD-10 device level, `IDENT PL12` (`src/TSS1.SYMB:258`; body `src/TSS1.SYMB:1429`). |
| 13 | `LEV13` | **Clock routine**, `MST PID` real-time clock at `1000` (`src/TSS1.SYMB:245`,`260`; body `src/TSS1.SYMB:4024`–`4035`). |
| 14 | `LEV14` | **Memory-protect / software-instruction interrupt** (NORD-1) or **internal-interrupt (IIC) dispatcher** (NORD-10) (`src/TSS1.SYMB:246`,`260`; bodies `src/TSS1.SYMB:4119`+, `4279`+). This is also where a NORD-1 `MON` traps (§5.3). |

**[VERIFIED]** The enable masks differ by machine: `ENABL, 65357` (NORD-1)
vs `ENABL, 77157` (NORD-10) (`src/TSS1.SYMB:311`–`316`). Level priority is
driven with `MST PID`/`MCL PID` (set/clear PID) throughout, e.g. the
scheduler is armed by `SAA 40; MST PID` (`src/TSS1.SYMB:4046`).

### 4.2 The scheduler

**[VERIFIED]** `LEV5` runs the scheduler first, then the swapper
(`%SCHEDULER RUNS FIRST`, `src/TSS1.SYMB:2718`). It walks the process table
from `9PRT`, examining each entry's status word 4 and quantum word 5,
choosing the next runnable process, and falls into `SWAPR` with the chosen
entry (`src/TSS1.SYMB:2720`–`2764`). **[VERIFIED]** Quanta: `QUANT, -17`
standard quantum (a negative count), `LQUA, 24` large quantum (number of
small quanta) (`src/TSS1.SYMB:2664`–`2665`); the scheduler ages `MIN 5,X`
and compares against `LQUA` to promote/demote (`src/TSS1.SYMB:2747`–`2750`).
**[ASSUMPTION]** this is a two-level round-robin with a small quantum for
interactive processes and a large quantum budget — inferred from the
`QUANT`/`LQUA` comments and the `MIN`/compare logic; the exact policy is not
spelled out in prose.

**[VERIFIED]** `LEV9` (the sub-clock) is what periodically re-arms the
scanner (`SAA 100; MST PID`) and the scheduler (`SAA 40; MST PID`) and
decrements the current process's quantum `MIN 1,X` (`src/TSS1.SYMB:4039`–
`4046`), so scheduling is clock-driven.

---

## 5. The monitor-call interface

### 5.1 The call table

**[VERIFIED]** Monitor calls are dispatched through **`MCTBL`**, the
"MONITOR CALL TRANSFER VECTOR" (`src/TSS1.SYMB:4300`–`4319`). A resident
pointer to it lives at low-core word 6 (`6/ MCTBL %POINTER TO MONITOR CALL
TABLE`, `src/TSS1.SYMB:66`). Its length `MCSIZ=*-MCTBL` (`src/TSS1.SYMB:
4319`) is **81 entries** — counting the table (`src/TSS1.SYMB:4302`–`4318`):
LEAVE, INBT, OUTBT, ECHOM, BRKM, RDISK, WDISK, RPAG, WPAG, RCTIM, EDIT,
CIBUF, COBUF, SETUP, GCI, GCD, WCI, WCD, GC, LEN, SETR, SETW, ABLKP, STEQL,
STARR, GCMD, MSG, DIV, NORM, IOUT, CNS, CSN, TRIM, BCOPY, OPEN, CFILE,
GCLIN, DBRK, GBRK, SBRK, OPIII, BROAD, GUNUM, GUNAM, PMSG, IMAIL, GMAIL,
WHUSR, ABLKU, GCMD, STARR, SOUT, ERMSG, QERMS, ISIZE, OSIZE, EXEC, UGNXT,
UDLTR, XDRD, SSTAT, RSTAT, SETBS, RWACM, TTIM, RKLOK, MDRIV, RDATE, PDATE,
IOCLL, ESCTR, RESUM, RMAXB, SMAXB, LF, MO113, REABT, SETBT, SFLOP, ZFLOP,
TMSFL. (`LF` at slot 76 is the failure/no-op sink, `src/TSS1.SYMB:4316`.)

The monitor-call code is the **index** into this table: code 0 = `LEAVE`
(return to command processor), 1 = `INBT` (input byte), 2 = `OUTBT`
(output byte), etc.

### 5.2 The dispatcher (`LEV3`)

**[VERIFIED]** `LEV3` is the monitor-call processor (`src/TSS1.SYMB:4322`+).
It fetches the call code (`MCODE`), bounds-checks it against `MCSIZ`
(`LDT (MCSIZ; SKP IF DA LST ST; JMP LF`, `src/TSS1.SYMB:4330`), indexes
`MCTBL` (`LDX (MCTBL; RADD DX SA; LDA 0,X; JAZ LF`, `src/TSS1.SYMB:4331`),
loads the user's X and the double register from the user register block,
and calls the handler with `JPL 0,B` returning to one of three skip returns
`LL0/LL2/LL3` (`src/TSS1.SYMB:4333`–`4356`). On out-of-range or zero-entry
it goes to `LF`, which posts `TRAP` into `UBLOK` (`src/TSS1.SYMB:4357`–
`4359`). The successful return advances the user P by 1, 2 or 3 words
(`LL0`/`LL2`/`LL3`) to skip over the call and its optional return
arguments, then `WAIT; JMP LEV3` (`src/TSS1.SYMB:4340`–`4356`).

### 5.3 How a user program issues a MON

**[VERIFIED, NORD-1]** The user executes a `MON` instruction; the memory-
protect interrupt `LEV14` decodes it via `DECOD`/`DECTB`. The decode table's
first two rows `161000;177400;LMON` and `153000;177400;LMON`
(`src/TSS1.SYMB:4156`–`4157`) route the `MON` opcode to `LMON`, which sets
`MBLOK`←`LEV3`, extracts the low 8 bits as `MCODE` (`LDA INST; AND (377; STA
I (MCODE`), bumps the user P past the instruction, and raises to level 3
(`SAA 10; MST PID`) (`src/TSS1.SYMB:4136`–`4141`). Thus on the NORD-1 a
`MON n` is emulated: the opcode's low byte is the call number.

**[VERIFIED, NORD-10]** `LEV14` is instead the internal-interrupt handler:
`TRA IIC; RADD DP SA` then a jump table on the interrupt cause code, where
the monitor-call cause vectors to `MCI` (`src/TSS1.SYMB:4280`–`4286`). `MCI`
sets `MBLOK`←`LEV3` and raises to level 3 (`SAA 10; MST PID`)
(`src/TSS1.SYMB:4285`–`4286`). The call code is picked up by `LEV3`'s N10
path from the register file (`IRR 10 DP; … LDA 0,X; AND (377`,
`src/TSS1.SYMB:4328`).

**[VERIFIED]** The same `LEV14`/`DECOD` mechanism on the NORD-1 also
*emulates NORD-10-only instructions in software* — `LBYTE`, `SBYTE`,
`RGMPY`, `RGDIV`, `EXRG` (execute-register), `EXRGF` — each with a `DECTB`
row (`src/TSS1.SYMB:4156`–`4166`) and a handler (`src/TSS1.SYMB:4169`–
`4275`). This is why the `MON` trap and the software-instruction emulator
share level 14.

---

## 6. Cold-start / initialization flow (high level)

**[VERIFIED]** Two low-core words drive start and restart:

- **Location 7** = `LDT *+3; JMP I *+1; SYSSV; CORLD` (`src/TSS1.SYMB:70`).
  Starting the machine at 7 runs `SYSSV`, which writes all pages of core to
  disc at `CORLD` and then falls into `INIT` (`SYSSV` at `src/TSS1.SYMB:
  4108`–`4117`, loops `XDISK` writing `SYSCT` blocks then `JMP I (INIT`).
- The **restart entry** at `20/ JMP I *+1; 301` (NORD-10, `src/TSS1.SYMB:
  212`) jumps into `INIT` at address `301` (`src/TSS1.SYMB:230`).

**[VERIFIED]** `INIT` (`src/TSS1.SYMB:231`–`309`): disables interrupts,
installs every level handler (§4.1), initializes buffers (`IBUF`/`ILINK`
loop, `src/TSS1.SYMB:262`–`263`), clears the device-state cells and locks,
calls `FINIT` to initialize the file system (`src/TSS1.SYMB:267`;
`FINIT` body `src/TSS1.SYMB:4711`–`4726`), seeds the process table
(`LDX (PRT1; STX I (PRT; STZ 0,X`, `src/TSS1.SYMB:273`), sets up the TTY /
timer / restart tables via `SETB` (`src/TSS1.SYMB:268`–`271`), and on the
NORD-10 programs paging `JPL IPGTB` and the RTC `IOX 11/13`
(`src/TSS1.SYMB:302`–`304`). It finishes by enabling interrupt groups and
jumping to `LEV0` (`src/TSS1.SYMB:284`–`305`). `FINIT` clears `PCTBL`,
`PDTBL`, `VCTBL`, the bit/user/OBT tables (`src/TSS1.SYMB:4714`–`4722`).

Disc/user/boot-tape mechanics (the `DKRST`/`DBOOT` sectors, `MINIT`, the
`TDUMP` boot tape) are **out of scope here** — see
`docs/PROJECT-DESCRIPTION.md` §3, `docs/BOOT-PROCESS.md`,
`docs/DISK-INIT-USERS-AND-BOOT.md`.

---

## 7. Overlay subsystem

**[VERIFIED]** Kernel code that is not resident is kept on disc as
**overlays** and pulled in on demand. TSS3 defines the machinery
(`%OVERLAY AREA`, `src/TSS3.SYMB:1`+):

- **`GOVER`** — "GET OVERLAY (MACRO)", args = overlay number, transfer
  location, routine name (`src/TSS3.SYMB:9`–`28`).
- **`SOVER`** — "WRITE OVERLAY ONTO DISK", overlay number = `RQR-1`, waits
  in `WAIT 47` if the overlay is too large (`src/TSS3.SYMB:29`–`51`).
- **`OVERL`** — "DEFINE OVERLAY (MACRO)", sets a symbol to the overlay
  number (`src/TSS3.SYMB:52`–`63`).
- **`OVERX`** — "SAVE OVERLAY ON DISK (MACRO)" (`src/TSS3.SYMB:64`–`98`).

**[VERIFIED]** At runtime, a per-process **`OVLAY`** cell holds the
currently loaded overlay for that user (`src/TSS2.SYMB:78`), with `OVLAX` a
temporary (`src/TSS2.SYMB:80`). The overlay-enter routine **`GOVX`**
(`src/TSS2.SYMB:196`–`206`) compares the requested overlay in `0,X` against
the current `OVLAY` (`9OVL`); if different it saves the old, records the new,
raises priority (`SAA 40; MST PID`) — **[ASSUMPTION]** so the actual disc
read of the overlay body happens at a lower level / elsewhere; `GOVX` itself
only does the bookkeeping and the jump into the overlay
(`src/TSS2.SYMB:201`–`203`). The low-core dispatch word `3/ GOVX; USTK; STK`
(`src/TSS1.SYMB:65`) wires `GOVX` into the resident vector.

**[VERIFIED]** Overlay disc areas are `OVDK1=CORL1+100` and
`OVDK2=CORL2+100` (`src/TSS1.SYMB:2608`–`2609`), with `OVDK=OVDK1` (or
`OVDK2` for the `DEBUG`/version-B build) (`src/TSS1.SYMB:2626`–`2633`).
On-disc overlay format is detailed in `docs/OVERLAY-DISC-SPEC.md` — not
repeated here.

---

## 8. Open questions / not determined from source

- **NORD-1 user addressing.** Whether the NORD-1 build relies on any
  address relocation or runs the user purely in resident core with page
  swapping is **[ASSUMPTION]** (§2.3); the source shows no NORD-1 page
  hardware but does not state the intent.
- **Exact scheduler policy.** The `QUANT`/`LQUA` two-level scheme is read
  from the aging code and comments, but the precise runnability/priority
  rules are **NOT fully DETERMINED FROM SOURCE** beyond what §4.2 cites.
- **Level 2 (`LEV2`) body.** Its role as the utility/command level is
  **[ASSUMPTION]** from the `UBLOK` pairing; its handler body was not read
  line-by-line for this document.
- **`LEV7`/`LEV10`/`LEV12` device semantics.** Identified as device-
  interrupt levels via `IDENT PLnn` (`src/TSS1.SYMB:1860`,`1890`,`1429`),
  but which physical device each `IDENT` code selects is **NOT DETERMINED
  FROM SOURCE** in this pass.
- **`GOVX` → actual disc read.** Where the overlay body is physically read
  from `OVDK` into the overlay area is **NOT DETERMINED FROM SOURCE** here
  (see `docs/OVERLAY-DISC-SPEC.md`).

---

*Cross-references: `docs/PROJECT-DESCRIPTION.md` (recovery, authorship,
boot chain, rebuild results), `docs/MAC-BPUN-Analysis.md` (the MAC
assembler and its MON inventory), `docs/TSS-Analysis.md`,
`docs/OVERLAY-DISC-SPEC.md`, `docs/CDC-DISC-DEVICE.md`,
`docs/DISK-INIT-USERS-AND-BOOT.md`, and `CLAUDE.md` (MAC semantics and
corpus notes).*
