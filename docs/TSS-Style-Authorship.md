# NORD TSS — Coding-Style and Authorship Analysis

Based on the recovered sources in `E:\Dev\Ronny\TSS\Sources\`.
**[VERIFIED]** = read directly from the files. **[ASSUMPTION]** = stylistic
inference, clearly argued but not provable from the archive alone.

---

## 1. The hands in the code

### Hand 1 — Bo Lewendal: the base system

**[VERIFIED]** TSS1 line 1: `% NORD TIMESHARING SYSTEM  BY BO LEWENDAL`.
The bulk of all five parts (scheduler, paging, file system, monitor calls,
command processor, TTY handling) is one uniform, very recognizable style:

- **Dense multi-statement lines** — 3–6 instructions per line joined with `;`:
  `LEV6, SAT NTTY; COPY CM2 DX ST` … the whole kernel reads like this.
- **Uniform terse routine headers**: `%NAME` / one-line purpose / register
  contract (`%T = USER NUMBER`, `%A = DEVICE ID`) / `%RETURN` — nothing more.
- **A systematic local-symbol idiom**: work cells after the code
  (`TCL, 0` / `TCT, 0`), then `)FILL` and `)PCL <name>` to close the scope and
  free the names for reuse in the next routine. This )KILL/)PCL scoping runs
  through all five files — one mind, one discipline.
- **ALGOL-ish pseudo-code comments** for the hard algorithms, e.g. the page
  allocator (TSS1 lines 2917/3214): `% FOR J:=0 TO NDPGS-1 DO;` and stepwise
  `%*`-prefixed narration (TSS1 3029–3040). **[ASSUMPTION]** the habits of
  someone at home in ALGOL-style structured thinking, writing assembler.
- **Assembler metaprogramming**: the GOVER/OVERL/OVERX overlay macros
  (TSS3 head) that move assembled code and write it to disk *during assembly* —
  the most sophisticated MAC usage in the archive.
- Short reused labels (`L1`, `L2`, `MS1`…), message strings `'…$\ '`.

### Hand 2 — NJL = Nils Jakob Langeland: the 1973 NORD-10 device drivers

**[VERIFIED]** two signed items, both spring 1973:
- `XDRUM` — TSS1 3730–3874: *"D R U M   T R A N S F E R   R O U T I N E …
  THIS IS 1. APPROXIMATION TO A NORD-10 VERSION — NJL 17/4/73"*
- `IOXLIB` — MINIT lines 592–596: *"STANDARD INPUT/OUTPUT FOR NORD-10 …
  %*=*=*…* IOXLIB *…=*% — ORIGINATOR: NJL 28/5/73"*

The style could not be more different from Lewendal's:

- **One instruction per line**, operands in tab columns, an aligned `%` comment
  on nearly every line.
- **Letter-spaced banner titles** (`% D R U M   T R A N S F E R …`) and a
  formal interface contract in fixed sections: CALLING SEQUENCE / CALLED WITH /
  RETURN INFORMATION — with the standard multi-exit convention
  (error exit / busy exit / finished exit) and the tell-tale closing line
  `% B IS NOT USED BY THE ROUTINE`.
- Device register maps declared as symbol tables with per-register comments
  (`RCX= 0 %READ CORE ADDRESS` …), `)KILL`-ed again after the routine.
- Dates in dd/mm/yy.

**[ASSUMPTION, strong]** The MAGTP mag-tape driver (TSS3 455–560) is the same
hand or the same house template: identical banner, identical section headings,
identical `% B IS NOT USED BY THE ROUTINE` line, identical
`% HDEV,B MUST HOLD THE HARDWARE DEV. NUMBER` remark — but it is the
NORD-1 (`"NN10`, `IOT`) variant, so this template predates the N10 port.
This is the documentation shape later seen in SINTRAN driver sources —
i.e. Norsk Data house driver style. NJL is **Nils Jakob Langeland**
(identification supplied by Ronny, 2026-07-19 — not stated in the archive
itself, which only carries the initials).

### Hand 3 — the N10 in-line patches (author uncertain)

The ~120 `"N10`/`"NN10` conditional blocks **[VERIFIED count: 97+104 in TSS1,
13+12 in TSS2, 5+10 in TSS3, 3+3 in TSS4, 2+2 in TSS5]** are mostly surgical
1–5 line substitutions written in **Lewendal's compact style**, not NJL's
driver style — e.g. the whole N10 `INIT`, the `IRW`/`IRR` context patches, the
`VERDR` Versatec driver. **[ASSUMPTION]** either Lewendal did the core N10
port himself, or the porter deliberately matched his style; the archive cannot
distinguish. The XDRUM comment "1. approximation to a NORD-10 version" shows
the N10 port was in progress in April 1973.

### Hand 4 — imported table

**[VERIFIED]** TSS1 1685: `% CARD CODE CONVERSION TABLE — % REVISED 1971.08.04`
— a yyyy.mm.dd date, unlike NJL's dd/mm/yy; likely imported from an existing
source **[ASSUMPTION]**. Oldest date in the archive.

### Hand 5 — "CVS": the modern restorer

**[VERIFIED]** `% Added by CVS` lines exist only in ASSYSA/ASSYSB/MINIT/TDUMP:
symbol patches for opcodes MAC no longer had built in, and the notes
`STR has been substituted by XTR`, `LSS has been substituted by XSS`. No code.

### Version identity

**[VERIFIED]** TSS5 1085/1092: startup banners
`'$NORD TSS VERSION 3.0A IS UP$\ '` and `…3.0B…` — this archive is
**TSS version 3.0**, A = production, B = DEBUG build. TSS4 1724 prompts
`ID[YEAR (E.G. 1973): ]` — consistent with a 1973 system.

---

## 2. What is specifically written for the NORD-10 (`"N10` code)

The NORD-1 (`"NN10`) is the original target; every N10 block replaces a
NORD-1 mechanism with the NORD-10 equivalent. Complete inventory of the
N10-specific machinery **[VERIFIED locations]**:

| Area | NORD-1 way (`"NN10`) | NORD-10 way (`"N10`) | Where |
|---|---|---|---|
| Interrupt entry | Memory vector blocks (20/ 61; RBLOK; …) + software `SAVE`/`UNSAV` of registers to memory | Hardware register levels; program level-P registers with `IRW nn DP`; `IDENT PL11` vectored device identification | TSS1 178–206, 236–245, 1797–1811 |
| Interrupt init | `IOT 6`, poke vectors via `STA I (61…` | `POF`, `TRR PIE`, `IRW`, `TRR IIE`, `ION` | TSS1 219–291 (INIT fully rewritten) |
| Interrupt enable/disable | `INTDS=150440`/`INTEN=150540` (device words) | `INTDS=IOF`, `INTEN=ION` (CPU instructions) | TSS1 103–106 |
| Error accounting | — | `NER10/NER11/NER12/NIOX/SER23/SER10/SER12` false-interrupt and IOX-timeout counters + restart entry `20/ JMP I *+1; 301` | TSS1 193–206 |
| **Paging / memory protection** | 4K/16K `MPR` protect register (`TRR MPR`) | `IPGTB` "INITIALIZE PAGING SYSTEM": loop `TRR PCR` writing paging-control registers, `LDA (1136; TRR IIE`, `POF/PON` | TSS1 236–291, 305+ |
| Context switch | `RBLOK` register block in memory, `STT I (RBLOK` | Level-1 hardware register block: `IRW 10 DP`, `IRR 10 DP`, `LRB/SRB 10` | TSS2 380–384, 962–965, 993–995, 1734–1759 |
| Divide | Software `DIV` routine (~20 lines, PROGM/ENTER) | Hardware `RDIV`: `DIV, SAD ZIN SHR 20; RDIV ST; EXIT` — one line | TSS2 2165–2179 |
| Real-time clock | `IOT PIN 6` / `IOT 6` (OLA variant for "special RTC") | `IOX 13` with control words `122001` (on) / `20001` (off), preset `LDA (310; IOX 11` | TSS2 1796–1814, TSS1 288–289 |
| TTY / paper tape I/O | `IOT` with SNI/ACT/SKA/PIN bit combinations | `IOX` register model: dev+0 data, +2 status (`BSKP … 30 DA` ready bits), +3 control; TTY at 300, reader at 400 | MINIT IOXLIB 590–655, TSS1 TTY blocks (1580–1660 area) |
| **Swapping drum** | *(does not exist on NORD-1 config — disk only, `TRSFR=TRXX`)* | `XDRUM` driver, device `IOX 540`+0..7, plus `TRSFR` drum/disk router | TSS1 3711–3874 |
| CDC disk | `IOT`-style channel (DISC=DCHN+44 etc.) | N10 register set `LCA=DCHN+1, LBA=+3, LMR=+5, RST=+4, LWC=+7` | TSS1 3341–3600 `"CDC N10` blocks, TDUMP 51–56 |
| Versatec printer | — | `VERDR` driver (`"N10 VERSA` mark), `IOX DLP+1/3/5`, hangs off LEV11 DMA ident | TSS1 1813–1837 |
| Mag tape | Full `MAGTP` driver (Kennedy or HP variants, `IOT`) | **No N10 driver present** — the driver is `"NN10`-only | TSS3 455+ |
| Plotter output | `IOT ACT SKA PLOT` busy-wait | plain jump — plotter I/O dropped/deferred | TSS2 1351–1355 |
| Enable mask | `ENABL, 65357` + `SVEEN, 3717` | `ENABL, 77157` | TSS1 296–301 |

Reading of the table **[ASSUMPTION]**: the N10 port was done in 1973 by
rewriting exactly the machine-dependent layer — interrupts, context, paging,
clock, and each device driver — while every line of scheduler/file-system/
command logic is shared between both targets via the mark system. Mag tape and
plotter were not (yet) ported; the drum and Versatec exist *only* for the
NORD-10 — the drum being the point of the port (real swapping hardware), and
XDRUM's own header admits it is a first approximation still being brought up.

---

## 3. Quick fingerprint guide

| Trait | Lewendal (base) | Langeland (drivers) |
|---|---|---|
| Line density | 3–6 instructions per `;`-line | one instruction per line |
| Comments | header contract only; pseudo-code for algorithms | aligned `%` on almost every line |
| Banners | `%NAME` | `% D R U M …` letter-spaced |
| Interface doc | `%T = …` one-liners | CALLING SEQUENCE / CALLED WITH / RETURN INFORMATION sections |
| Exits | RET/EXIT, skip-returns | formalized error/busy/finished multi-exit |
| Dates | none | dd/mm/yy (17/4/73, 28/5/73) |
