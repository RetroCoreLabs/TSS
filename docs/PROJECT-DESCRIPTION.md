# NORD TSS 3.0 — History, Identity and Provenance

The single reference for **what this archive is, who wrote it, where every
file came from, and how the rebuild was proven correct**. Status as of
2026-07-23.

Facts are marked **[VERIFIED]** when read directly from the files, the MAC
binary or a reference manual, and **[ASSUMPTION]** when they are
interpretation. Nothing else is asserted.

Related documents: [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) (how the OS
works, including modifications made to the source),
[`TSS-BRINGUP.md`](TSS-BRINGUP.md) (how to build, boot and log in today),
[`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md) (the mac-c reimplementation and its
defect record), [`../README.md`](../README.md) (repository overview).

---

## 1. What the archive is

**[VERIFIED]** The **NORD Time Sharing System (TSS), version 3.0**, by
**Bo Lewendal** — a complete, self-contained timesharing operating system
for the Norsk Data NORD-1 and NORD-10, written in **MAC assembler** and
dated to **1973** (the startup banner reads `NORD TSS VERSION 3.0A IS UP`
and a prompt asks for `YEAR (E.G. 1973)` — TSS4 line 1724).

It is **not** NPL — there is no NPL anywhere in these files; the
`FOR J:=0 TO NDPGS-1 DO` lines are pseudo-code *comments* describing the
algorithms, prefixed with `%`. And the top-level files are **not** build
logs; the breakdown:

| File | Kind | What it is |
|---|---|---|
| `TSS1.SYMB` … `TSS5.SYMB` | **SOURCE** | The real MAC source of TSS, split in 5 parts — CVS's patched copy (see §2) |
| `TSS1.ORG` … `TSS5.ORG` | **SOURCE — the 1973 original** | Lewendal's unpatched source; genuinely a *different version* from `.SYMB`, not a whitespace variant (see §2) |
| `ASSYSA.SYMB` | **BUILD SCRIPT** | MAC console stream that assembles version **A** (SYSA) |
| `ASSYSB.SYMB` | **BUILD SCRIPT** | Same for version **B** (SYSB, `DEBUG` mark set) |
| `LIST1.SYMB` … `LIST5.SYMB` | **BUILD OUTPUT** | MAC *list stream* output of `)9ASSM TSSn,LISTn,0`. LIST1 is truncated mid-line (listing only; source TSS1 is complete) |
| `LIST.SYMB` | **BUILD OUTPUT** | One combined list-stream run covering TSS3+TSS4+TSS5 (TSS4 starts at line 2861, TSS5 at 7358) |
| `ASYMB.SYMB` | **BUILD OUTPUT** | Symbol-table dump of the version-A build (`)LIST` to the object stream) — every symbol with its final octal value |
| `BSYMB.SYMB` | **BUILD OUTPUT** | Same for version B |
| `MINIT.SYMB` | **SOURCE** | Standalone "Mass Storage Initialization Program" (CDC disk, NORD-1 or NORD-10) |
| `TDUMP.SYMB` | **SOURCE** | Standalone "TSS Dumper" (loads at 40000, NCR/CDC, NORD-1/NORD-10) |

Repository layout: `archive/` holds the untouched 8-bit originals (both
`.ORG` and `.SYMB`), `src/` the one clean assemblable copy, `reference/`
the golden build outputs, `derived/` project-made inputs, `Build/`
regenerated output. Each folder has its own README.

### Version identity

**[VERIFIED]** TSS5 lines 1085/1092 carry the startup banners
`'$NORD TSS VERSION 3.0A IS UP$\ '` and `…3.0B…` — this archive is
**TSS version 3.0**, where **A = production** and **B = the DEBUG build**
(ASSYSB sets the `DEBUG` library mark).

---

## 2. Provenance: `.ORG` vs `.SYMB`, and the "CVS" restoration

**[VERIFIED]** The archive holds the source **twice, and they are different
programs**. Comparing content (parity stripped, whitespace normalised)
shows **153 changed lines** across TSS2–TSS5 (TSS1 is whitespace-only).
Every change is a symbol rename: `STR`→`XTR` (77), `STRX`→`XTRX` (8),
`LSS`→`XSS` (7), `STR2`→`XTR2`, `STR1`→`XTR1`, `LSS`→`LSSX` (2 each),
`STR0`→`XTR0` (1).

The reason is verifiable: `STR` and `LSS` **are symbols in MAC's permanent
table** (`STR`=030000, `LSS`=002400), so TSS's own labels of those names
collided with the assembler. `ASSYSA` says so itself — *"STR and LSS are
defined in the MAC/FMAC symbol table … CVS: STR has been substituted by
XTR"*. So **`.ORG` is Lewendal's 1973 source and `.SYMB` is CVS's patched
copy**. `src/` carries the patched version, because that is what the golden
dumps were built from.

**[VERIFIED]** The rename is also **incomplete**: `STR`, `STR0`, `STR1`,
`STR2`, `STR1X` and `STR2X` are referenced but never defined in the patched
sources, which accounts for **six of the twenty symbols left undefined by a
full rebuild**. Reproduce with `mac-c/scripts/attic/diff_cvs_vs_original.sh` and
`mac-c/scripts/attic/check_rename_gaps.sh`; full detail in `archive/README.md`.

### Parity-clearing caveat

**[VERIFIED]** The parity-stripped `LIST.SYMB` contains 245 bytes of `0x1D`
(GS) that were `0x9D` in the original — all inside string constants like
`MS1, 'P = <0x9D> '` (register-dump message texts). Inside `'…'` string
data, bit 7 is not necessarily parity — clearing it may alter real string
data. Harmless here (a listing file; the master TSSn sources contain no
such bytes — verified scan), but this is why `archive/` keeps the raw
8-bit originals for any byte-level work.

---

## 3. Authorship — the hands in the code

**[VERIFIED]** at least three period hands plus one imported table and one
modern restorer, distinguishable by style:

| hand | what | evidence |
|---|---|---|
| **Bo Lewendal** | the base system — scheduler, paging, file system, monitor calls, command processor | credited in the TSS1 header; dense `;`-packed lines, `)FILL`+`)PCL` scoping, ALGOL-style pseudo-code comments |
| **Torolf Paulsen** | co-proposed TSS with Lewendal on 14 June 1971; the double-precision normalise `NORM` | the memo "FRA : BL, TEP" and Lewendal 2014 ("my coworker Torolf Paulsen and I proposed"); `%DUE TO T.E. PAULSEN` at TSS2 line 2244 |
| **NJL = Nils Jakob Langeland** | the 1973 NORD-10 device drivers — `XDRUM` (drum, signed 17/4/73) and `IOXLIB` (NORD-10 I/O, signed 28/5/73) | signed and dated in the source; one instruction per line, letter-spaced banners, formal CALLING SEQUENCE / RETURN INFORMATION contracts |
| the `"N10` patch hand | ~120 conditional blocks porting the machine layer | written in Lewendal's compact style, so the porter is **[ASSUMPTION]** either Lewendal or someone matching him |
| an imported table | card-code conversion table, `% REVISED 1971.08.04` | TSS1 line 1685; yyyy.mm.dd date unlike NJL's dd/mm/yy — **[ASSUMPTION]** imported from an existing source; oldest date in the archive |
| **"CVS"** | a modern restorer | `% Added by CVS` symbol patches in ASSYSA/ASSYSB/MINIT/TDUMP only; the `STR`→`XTR` / `LSS`→`XSS` renames of §2; no code |

### Hand 1 — Bo Lewendal: the base system

**[VERIFIED]** TSS1 line 1: `% NORD TIMESHARING SYSTEM  BY BO LEWENDAL`.
The bulk of all five parts is one uniform, very recognizable style:

- **Dense multi-statement lines** — 3–6 instructions joined with `;`:
  `LEV6, SAT NTTY; COPY CM2 DX ST` … the whole kernel reads like this.
- **Uniform terse routine headers**: `%NAME` / one-line purpose / register
  contract (`%T = USER NUMBER`) / `%RETURN` — nothing more.
- **A systematic local-symbol idiom**: work cells after the code
  (`TCL, 0` / `TCT, 0`), then `)FILL` and `)PCL <name>` to close the scope
  and free the names for reuse in the next routine. This `)KILL`/`)PCL`
  scoping runs through all five files — one mind, one discipline.
- **ALGOL-ish pseudo-code comments** for the hard algorithms, e.g. the page
  allocator (TSS1 lines 2917/3214): `% FOR J:=0 TO NDPGS-1 DO;` with
  stepwise `%*`-prefixed narration (TSS1 3029–3040). **[ASSUMPTION]** the
  habits of someone at home in ALGOL-style structured thinking.
- **Assembler metaprogramming**: the GOVER/OVERL/OVERX overlay macros
  (TSS3 head) that move assembled code and write it to disk *during
  assembly* — the most sophisticated MAC usage in the archive.
- Short reused labels (`L1`, `L2`, `MS1`…), message strings `'…$\ '`.

### Hand 2 — Nils Jakob Langeland: the NORD-10 device drivers

**[VERIFIED]** two signed items, both spring 1973:

- `XDRUM` — TSS1 3730–3874: *"D R U M   T R A N S F E R   R O U T I N E …
  THIS IS 1. APPROXIMATION TO A NORD-10 VERSION — NJL 17/4/73"*
- `IOXLIB` — MINIT lines 592–596: *"STANDARD INPUT/OUTPUT FOR NORD-10 …
  ORIGINATOR: NJL 28/5/73"*

The style could not be more different from Lewendal's: **one instruction
per line**, operands in tab columns, an aligned `%` comment on nearly every
line; **letter-spaced banner titles** and a formal interface contract in
fixed sections — CALLING SEQUENCE / CALLED WITH / RETURN INFORMATION — with
the standard multi-exit convention (error / busy / finished) and the
tell-tale closing line `% B IS NOT USED BY THE ROUTINE`; device register
maps declared as commented symbol tables (`RCX= 0 %READ CORE ADDRESS`),
`)KILL`-ed after the routine; dates in dd/mm/yy.

**[ASSUMPTION, strong]** The MAGTP mag-tape driver (TSS3 455–560) is the
same hand or the same house template: identical banner, identical section
headings, identical `% B IS NOT USED BY THE ROUTINE` line — but it is the
NORD-1 (`"NN10`, `IOT`) variant, so this template predates the N10 port.
It is the documentation shape later seen in SINTRAN driver sources, i.e.
Norsk Data house driver style. NJL = **Nils Jakob Langeland**
(identification supplied by Ronny, 2026-07-19 — the archive itself carries
only the initials).

### Hand 3 — the N10 in-line patches (author uncertain)

The `"N10`/`"NN10` conditional blocks **[VERIFIED count: 97+104 in TSS1,
13+12 in TSS2, 5+10 in TSS3, 3+3 in TSS4, 2+2 in TSS5]** are mostly
surgical 1–5 line substitutions written in **Lewendal's compact style**,
not NJL's driver style — e.g. the whole N10 `INIT`, the `IRW`/`IRR`
context patches, the `VERDR` Versatec driver. **[ASSUMPTION]** either
Lewendal did the core N10 port himself, or the porter deliberately matched
his style; the archive cannot distinguish. Lewendal's own 2014 account
favours the first: the NORD-10 "included hardware to better support
timesharing among users", and "I kept improving the software to take
advantage of the Nord-10 hardware". He left Norsk Data on 1 January 1974, so
N10 work after that date was not his. See `TSS-AND-SINTRAN.md`. XDRUM's "1. approximation"
remark shows the port was still in progress in April 1973.

### Quick fingerprint guide

| Trait | Lewendal (base) | Langeland (drivers) |
|---|---|---|
| Line density | 3–6 instructions per `;`-line | one instruction per line |
| Comments | header contract only; pseudo-code for algorithms | aligned `%` on almost every line |
| Banners | `%NAME` | `% D R U M …` letter-spaced |
| Interface doc | `%T = …` one-liners | CALLING SEQUENCE / CALLED WITH / RETURN INFORMATION sections |
| Exits | RET/EXIT, skip-returns | formalized error/busy/finished multi-exit |
| Dates | none | dd/mm/yy (17/4/73, 28/5/73) |

---

## 4. The NORD-10 port — what `"N10` actually changes

**[VERIFIED]** The NORD-1 (`"NN10`) is the original target; every `"N10`
block replaces a NORD-1 mechanism with the NORD-10 equivalent. Complete
inventory of the N10-specific machinery **[VERIFIED locations]**:

| Area | NORD-1 way (`"NN10`) | NORD-10 way (`"N10`) | Where |
|---|---|---|---|
| Interrupt entry | Memory vector blocks (20/ 61; RBLOK; …) + software `SAVE`/`UNSAV` of registers to memory | Hardware register levels; `IRW nn DP`; `IDENT PL11` vectored device identification | TSS1 178–206, 236–245, 1797–1811 |
| Interrupt init | `IOT 6`, poke vectors via `STA I (61…` | `POF`, `TRR PIE`, `IRW`, `TRR IIE`, `ION` | TSS1 219–291 (INIT fully rewritten) |
| Interrupt enable/disable | `INTDS=150440`/`INTEN=150540` (device words) | `INTDS=IOF`, `INTEN=ION` (CPU instructions) | TSS1 103–106 |
| Error accounting | — | `NER10/NER11/NER12/NIOX/SER23/SER10/SER12` false-interrupt and IOX-timeout counters + restart entry `20/ JMP I *+1; 301` | TSS1 193–206 |
| **Paging / memory protection** | 4K/16K `MPR` protect register (`TRR MPR`) | `IPGTB` "INITIALIZE PAGING SYSTEM": loop `TRR PCR` writing paging-control registers, `LDA (1136; TRR IIE`, `POF/PON` | TSS1 236–291, 305+ |
| Context switch | `RBLOK` register block in memory, `STT I (RBLOK` | Level-1 hardware register block: `IRW 10 DP`, `IRR 10 DP`, `LRB/SRB 10` | TSS2 380–384, 962–965, 993–995, 1734–1759 |
| Divide | Software `DIV` routine (~20 lines) | Hardware `RDIV`: `DIV, SAD ZIN SHR 20; RDIV ST; EXIT` — one line | TSS2 2165–2179 |
| Real-time clock | `IOT PIN 6` / `IOT 6` | `IOX 13` with control words `122001` (on) / `20001` (off), preset `LDA (310; IOX 11` | TSS2 1796–1814, TSS1 288–289 |
| TTY / paper tape I/O | `IOT` with SNI/ACT/SKA/PIN bit combinations | `IOX` register model: dev+0 data, +2 status, +3 control; TTY at 300, reader at 400 | MINIT IOXLIB 590–655, TSS1 ~1580–1660 |
| **Swapping drum** | *(does not exist — disk only, `TRSFR=TRXX`)* | `XDRUM` driver, device `IOX 540`+0..7, plus `TRSFR` drum/disk router | TSS1 3711–3874 |
| CDC disk | `IOT`-style channel (DISC=DCHN+44 etc.) | N10 register set `LCA=DCHN+1, LBA=+3, LMR=+5, RST=+4, LWC=+7` | TSS1 3341–3600 `"CDC N10` blocks, TDUMP 51–56 |
| Versatec printer | — | `VERDR` driver (`"N10 VERSA` mark), `IOX DLP+1/3/5`, hangs off LEV11 DMA ident | TSS1 1813–1837 |
| Mag tape | Full `MAGTP` driver (`IOT`) | **No N10 driver present** — `"NN10`-only | TSS3 455+ |
| Plotter output | `IOT ACT SKA PLOT` busy-wait | plain jump — plotter I/O dropped/deferred | TSS2 1351–1355 |
| Enable mask | `ENABL, 65357` + `SVEEN, 3717` | `ENABL, 77157` | TSS1 296–301 |

Reading of the table **[ASSUMPTION]**: the N10 port rewrote exactly the
machine-dependent layer — interrupts, context, paging, clock, and each
device driver — while every line of scheduler / file-system / command logic
is shared between both targets via the mark system. Mag tape and plotter
were never ported; the drum and Versatec exist *only* for the NORD-10 —
the drum being the point of the port (real swapping hardware).

### The drum driver

**[VERIFIED]** `derived/DRUM-DRIVER.SYMB` is a verbatim extraction of
TSS1's `"DRUM N10` block:

- **`XDRUM`** (TSS1 3730–3874) — the NORD-10 drum driver. Device
  `IOX 540`; function codes 0=read, 1=write, 2=read-test, 3=compare,
  20=read-status; three exits (error / busy / finished); 32 sectors per
  track with automatic track-boundary splitting; word count = blocks × 64.
- **`TRSFR`** (TSS1 3711–3728) — the swap front end: pages below
  4×`DRMSZ` go to the drum, above it to the CDC disk via `TRXX`/`DKTR`.
  Without the `DRUM` mark, `TRSFR = TRXX` (disk-only).

It is compiled **out** of both archived builds — neither sets `DRUM` or
`N10`. `derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB` enables it.

---

## 5. How TSS was built — the ASSYSA script, decoded

`ASSYSA.SYMB` is a MAC console/command stream, not a shell script.
Line by line — **[VERIFIED]** against the MAC manual ND-60.096.01:

```
FMAC                          ← [ASSUMPTION] command to start the FMAC assembler
100                           ← [ASSUMPTION] answer to a table-size / start prompt
SYSA                          ← [ASSUMPTION] output/system name typed to a prompt
CDC MACF DIAB K14 TEL4        ← library marks: referencing an undefined symbol
                                makes it "true" for conditional assembly
IOT=160000;ACT=400;SKA=1000;PIN=2000;SNI=0    % Added by CVS
INTDS=150440;INTEN=150540                     % Added by CVS
TDBI=410; TDBO=40                             % Added by CVS
DIABD=156; TT3=162; TT4=DIABD; DIABT=4; DLP=143  % Added by CVS
GRP1=1;GRP2=2;GRP3=4;GRP4=10;
MPR=3;
8LP=200; 9PPT=100; 9TTI=40; 9TTO=40
9FP=200; 9CR=5; NOPEN=16
)9ASSM TSS1,LIST1,0           ← assemble TSS1, listing → LIST1, no object
)9ASSM TSS2,LIST2,0
)9ASSM TSS3,LIST3,0
)9ASSM TSS4,LIST4,0
)9ASSM TSS5,LIST5,ASYMB:SYMB  ← last part: object stream → ASYMB:SYMB
)LIST                         ← dump symbol table to the object stream
*:                            ← [UNKNOWN]
?                             ← [UNKNOWN]
)9TSS                         ← custom command — see below
```

Each TSSn part ends with `)LINE` (returns control to the console, so the
next script line runs). **Version B** (`ASSYSB.SYMB`) is identical except:
lowercase `fmac`, name `SYSB`, adds the `DEBUG` mark, produces no listings,
object/symbols → `BSYMB:SYMB`.

**[VERIFIED]** The `% Added by CVS` lines define symbols MAC/FMAC
originally had built in (NORD-1 I/O opcodes, interrupt ops, device
numbers). The as-found builds use: **CDC disk, MACF, Diablo terminal
support, 14K system, 4 teletypes, no DRUM, no N10** (NN10 default =
NORD-1 `IOT` I/O).

**[VERIFIED]** `)9TSS` is solved: in `MAC.BPUN` it and `)9EXIT` share one
handler (`MON 0`), so `)9TSS` is a **custom alias added for the TSS
restoration** — no standard MAC has it.

### The MAC binary itself

`$ND_BPUN_DIR/MAC.BPUN` is **MAC dated 14 March 1978**, the SINTRAN III
subsystem build, loading at octal `145000–177777`. **[VERIFIED]**:

- **Cold-start entry = octal 173724** (`0xF7D4`), confirmed dynamically in
  the nd100x emulator.
- **It cannot run without SINTRAN.** Every I/O path is a monitor call
  (`MON 1/2` + `MON 65`), `)9EXIT` is literally `MON 0`, and there is no
  console IOX and no MON handler in the image. On bare metal it loads
  correctly, then loops forever retrying its first character. This is why
  a host reimplementation (mac-c) was written instead of running MAC.
- Its permanent symbol table at `0xE9B3` (3-word entries, names packed
  5 chars × 6 bits) is where every opcode value in `mac-c/src/mac_permsym.c`
  came from — byte-verified ground truth for the instruction set.

---

## 6. How TSS was bootstrapped **[VERIFIED from the source]**

TSS occupies core `0–37777` (16K) and keeps a copy of itself on disk in
the `CORLD` "core load" area (versions A and B have separate areas
`CORL1`/`CORL2`, TSS1 2611–2620; in TDUMP: CDC A=60, CDC B=260, NCR A=10,
NCR B=210). Disk layout (TSS1 2595–2608): user table, 12 swap areas
`DKA1..DKA12` (200 octal pages each), then the file system `FSYS`.

Two tiny routines drive everything:

- **Location 7** holds `LDT *+3; JMP I *+1; SYSSV; CORLD` (TSS1 line 70,
  "%SAVE SYSTEM ON THE DISK"). Starting the machine at address 7 runs
  `SYSSV` (TSS1 4078–4087), which writes all 64 pages of core to disk at
  `CORLD` and then falls into `INIT`. Save-and-boot in one jump.
- **`DKRST`/`DBOOT` on disk sectors 0 and 2** do the reverse: read the
  image back from `CORLD` into core and jump to `INIT` at address 301
  (TDUMP 264–327; TSS3 240–275 carries the same routines inside TSS).

### The four ways in

1. **Cold build (MAC-resident)** — MAC assembles TSS1–TSS5 straight into
   core 0–37777; the operator starts at address 7. **[ASSUMPTION]** the
   operator used MAC's `7!` run command; plausibly what `)9TSS` at the end
   of `ASSYSA` automated.
2. **Warm restart from disk** — get sector 0 into core (hand-keyed
   mini-loader or hardware disk bootstrap, **[ASSUMPTION — not present in
   this archive]**), then `DKRST` reads the 16K image from `CORLD` and
   jumps to 301. On NORD-10 there is additionally the restart entry
   `20/ JMP I *+1; 301` (TSS1 197) — hitting restart at 20 re-INITs a
   resident image.
3. **Boot/distribution tape (made by TDUMP)** — TDUMP runs *under* TSS as
   a user program at 40000 (its 161xxx service calls are instructions
   trapped to the TSS monitor — cf. LEV14 "MEMORY PROTECT INTERRUPT"
   decode, TSS1 4092+). It punches ONE self-contained tape: an ASCII
   section depositing `HLOAD` (hardware reader bootstrap, `IOT` or N10
   `IOX 400/402/403` variants) + `TBOOT` (block loader) in MAC-loader
   octal-text format (TDUMP 78–126, 221–235); then binary blocks that
   write `DKRST` to sector 0, `RDKOP`/`DBOOT` to sector 2, the 16K image
   to `CORLD`, the same image again into core, and a trailer that jumps
   to **address 7** — SYSSV re-saves + INIT → TSS up. One tape both
   installs and starts the system.
4. **Virgin disk** — `MINIT` ("MASS STORAGE INITIALIZATION PROGRAM") is
   run first to initialise the disk / file-system structures; it talks to
   the console via MAC's `INBT`/`OUTBT` (or its own IOXLIB on NORD-10),
   i.e. it runs in a MAC environment, before TSS exists.

Minimum inventory: NORD-1 or NORD-10 with 12/14/16K core (per the
K12/K14/K16 mark), CDC or NCR disk, console TTY, paper-tape reader (and
punch to make tapes), optionally the drum for the `N10`+`DRUM` build.

---

## 7. Rebuilding TSS today — mac-c and the golden reconciliation

A MAC assembler was written in C99 (`mac-c/`) because no MAC binary can
run without SINTRAN (§5). It executes the **original 1973 `ASSYSA` command
stream verbatim** — `)9ASSM TSS1,LIST1,0` … `)9ASSM TSS5,LIST5,ASYMB:SYMB`
followed by `)LIST` — and produces `ASYMB:SYMB` itself.

Result, measured against the archived symbol dumps:

| build | golden symbols | exact matches | assembly errors |
|---|---|---|---|
| **A** (`ASSYSA`) | 693 | **679** | 0 |
| **B** (`ASSYSB`, DEBUG) | 689 | **675** | 0 |

**[VERIFIED]** 13 of the 14 unmatched entries in each build are **macro
names**, which real MAC lists with the address of the macro body *inside
MAC's own memory image* (values 146152…146627). Those addresses are
artifacts of MAC's internal layout and cannot be reproduced by a host
assembler, and are not to be faked. The remaining discrepancy is the known
**`QOV1C` off-by-one** in the TSS3 EDIT overlay. Excluding the macro
names, **679 of 680 reproducible symbols match**.

**[VERIFIED]** 20 symbols remain undefined after a full build and that is
correct: the library marks themselves, `"N10`-only definitions referenced
from unconditional code, the six casualties of CVS's incomplete rename
(§2), and `&L` / `PROGM&M`, which are expressions by design.

### The validation constraint — no instruction-bit oracle exists

**[VERIFIED]** The `reference/LIST*.SYMB` files are a **verbatim echo of
the source with NO generated-code column** — a location+word pattern
(`^[0-7]+\s+[0-7]{6}`) finds zero matches; the only 6-digit octals in them
are inline data constants from the source itself. MAC's `)9ASSM …,LISTn,…`
list output does not carry generated words.

**Consequence:** there is **no instruction-bit oracle anywhere in the
archive.** `ASYMB`/`BSYMB` prove symbol *addresses* (i.e. word counts)
only; the LIST files add nothing on encoding. This is why encoding bugs
could — and did — survive a long time while the symbol scorecard stayed
perfect, and why `mac-c/tests/test_mac.c` asserts every instruction's
**encoding** directly, and why the ultimate test is running the built
system (§8).

---

## 8. Current status (2026-07-23): TSS runs

**[VERIFIED]** The rebuilt system **boots and takes an interactive
login** — likely the first interactive TSS 3.0 session since the 1970s:

- **mac-c** rebuilds TSS from the 1973 scripts with **0 errors**;
  golden reconciliation **679/693 (A)** and **675/689 (B)** per §7.
- A **MINIT-formatted CDC disc** plus a **cold start at address 7** (the
  historical save-and-boot vector of §6) with the operator switches at
  `--opr=131313` runs `SINIT`, which **creates user SYSTEM** on the disc
  (disc-verified: track allocated, name written to `USTBL`).
- Interactive login then reaches the **`@` prompt**; commands such as
  `HELP` and `WHO-IS-ON` work.

The road there required fixing a family of **silent mac-c miscompiles** —
bugs invisible to the symbol-address oracle (§7) that only surfaced as
wrong runtime behaviour: the all-digit-symbol octal misparse (`AND 9377`
assembled as `AND 0`, zeroing console characters), a SHR shift-count
negation, the missing `CLD` permanent symbol (verified against real MAC),
completion of CVS's `STR`→`XTR` rename, a forward-reference addend
error, and addressing mode 6 `,I ,X` encoded X-relative instead of
P-relative (which corrupted nine backward references in the swapper and
hung the `MEMORY` command). The full defect record, with tests, is in
[`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md). The few deliberate source
modifications made relative to the archive (e.g. restoring the `EXRGP`
definition) are documented in
[`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md).

The exact build/boot/login procedure is in
[`TSS-BRINGUP.md`](TSS-BRINGUP.md).

### Milestones

| date | milestone |
|---|---|
| 3 March 1971 | Bo Lewendal joins Norsk Data |
| 14 June 1971 | Lewendal and Torolf Paulsen propose a timesharing system for the NORD-1 |
| July 1971 | Lewendal writes the first working TSS during the company vacation |
| September 1972 | Lewendal's article "TIMESHARING: What, Why and Whiter?" in *ND-Nytt* |
| 16 February 1973 | *Reference Manual for the NORD Timesharing System*, ND-60.039.01 |
| 1973 | Lewendal's TSS 3.0; NJL's NORD-10 drivers (Apr–May) |
| 1 January 1974 | Lewendal leaves Norsk Data |
| 1978 | the archived SYSA/SYSB builds (the golden symbol dumps) |
| (modern) | "CVS" restoration pass: parity strip, `STR`→`XTR`/`LSS`→`XSS` build-script patches |
| 2026-07-19 | archive analysed; authorship and provenance established; `.ORG` vs `.SYMB` decoded |
| 2026-07-20 | mac-c reproduces the 1973 build; golden reconciliation 679/693 with 0 errors |
| 2026-07-21 | TSS executes its own cold-start on nd100x for the first time |
| 2026-07-23 | full bring-up: MINIT-formatted disc, cold start at 7, user SYSTEM created, interactive `@` login with working commands |
| 2026-07-26 | every `.SYMB` file documented in pseudo-C; `TDUMP` assembled for the first time; the `..` master password found and verified |

---

## 9. Curiosities found in the source

Things discovered while reading the corpus that are not architecture, not
provenance, and too good to leave in a footnote.

### `..` logs you in as anybody

`LOGON` contains a hard-coded master password. `src/TSS4.SYMB:568`:

```
L6,	LDA PASSW,B; SUB (636; JAZ L6A
	SAX 7; LDA I OBJ,B,X; SUB PASSW,B; JAF L7
L6A,	LDX (MS3; JPL I (MSG; SAT 1; JPL I (SXBRK
```

In C:

```c
if (typed == 0636)      goto ok;      /* bypass -- the stored password is
                                         never even loaded              */
if (stored[7] != typed) goto retry;
ok:  print("OK");
```

TSS does not store passwords as text. `src/TSS4.SYMB:566` folds the typed
characters into one 16-bit word, `passw = (passw << 3) + c`, each character
masked to 7 bits. `0636` octal is 414 decimal, and ASCII `.` is 46:

```
h("..") = (46 << 3) + 46 = 368 + 46 = 414 = 0636
```

So **typing `..` at the password prompt logs you in as any user that has a
password set.** Eleven other two-character strings collide (`$~`, `%v`, `&n`,
`'f`, `(^`, `)V`, `*N`, `+F`, `,>`, `-6`, `/&`); no single character can reach
414, and no three-character printable string can either, since the minimum for
three printable characters is `32*64 = 2048`. `..` is the only candidate a
person would type, and the only one where both characters are the same.

**It is in the 1973 original**, not a later addition. Present in
`archive/original-text/TSS4.ORG`, `archive/TSS4.ORG`, `archive/TSS4.SYMB`, and
-- decisively -- in `reference/LIST4.SYMB:1153`, the 1978 assembly listing, so
it was compiled into the system that actually shipped. The literal `0636`
occurs exactly once in all 15,244 lines of the corpus.

**Verified by running it** on 2026-07-26 under nd100x:

| password | result |
|---|---|
| `999` (wrong) | **REJECTED** -- returns to `@ENTER` |
| `123` (the real one) | ACCEPTED -- prints `OK` |
| `..` | **ACCEPTED** -- prints `OK` |

The rejected control matters: it proves the comparison is live, not disabled.
And `h("123") = 0o7003` differs from `h("..") = 0o636`, so the success is the
bypass and not a collision with the stored value.

Scope: it works at `@ENTER` only. You still need a valid user name and a
non-zero project number. **`PASSWORD` does not accept it** --
`src/TSS4.SYMB:627` compares against the stored word with no escape, so the
master password can be used to log in as someone, but not to change their
password. It is irrelevant for users with no password at all, since
`src/TSS4.SYMB:562` skips the prompt entirely when the stored word is zero --
which is the state `SYSTEM` is in after bring-up.

Why it exists is **unknown**. A field-service login for a site that had lost
its SYSTEM password is the obvious guess for 1973, but there is no comment on
the line and nothing nearby explains it. That `..` was the intended string is
**inferred**: what the source records is the constant and the hash function,
not the password.

There is no way to disable it short of editing the source and reassembling --
it is a compiled-in literal.

### Login failures are silent

Related, and visible from the same routine: an unknown user name
(`src/TSS4.SYMB:559`) and a wrong password (`src/TSS4.SYMB:601`) both jump back
to the same place and reprint `@ENTER `. No message, no attempt counter, no
lockout, nothing logged. From the terminal the two are indistinguishable.

---

## See also

- [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) — how the OS works;
  source modifications vs the archive
- [`TSS-BRINGUP.md`](TSS-BRINGUP.md) — building, booting and logging in
  on the emulator today
- [`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md) — the mac-c reimplementation,
  its validation methodology and defect record
- [`../README.md`](../README.md) — repository overview and folder map
