# NORD TSS 3.0 — Recovery, Analysis and Rebuild

Full description of what this archive is, what was done to it, and what the
current state of the work is.

Facts below are marked **[VERIFIED]** when they were read directly from the
files, the MAC binary or a reference manual, and **[ASSUMPTION]** when they
are interpretation. Nothing else is asserted.

---

## 1. What the archive is

**[VERIFIED]** The **NORD Time Sharing System (TSS), version 3.0**, by
**Bo Lewendal** — a complete, self-contained timesharing operating system
for the Norsk Data NORD-1 and NORD-10, written in **MAC assembler** and
dated to **1973** (the startup banner reads `NORD TSS VERSION 3.0A IS UP`
and a prompt asks for `YEAR (E.G. 1973)`).

It is **not** NPL, and the top-level files are **not** build logs: the
`TSSn:SYMB` files are the real MAC source, the `LISTn:SYMB` files are the
list-stream output of assembling them, and `ASYMB:SYMB` / `BSYMB:SYMB` are
the symbol-table dumps of the two original builds.

### Authorship

**[VERIFIED]** at least three hands, distinguishable by style:

| hand | what | evidence |
|---|---|---|
| **Bo Lewendal** | the base system — scheduler, paging, file system, monitor calls, command processor | credited in the TSS1 header; dense `;`-packed lines, `)FILL`+`)PCL` scoping, ALGOL-style pseudo-code comments |
| **Nils Jakob Langeland** ("NJL") | the 1973 NORD-10 device drivers — `XDRUM` (drum, 17/4/73) and `IOXLIB` (NORD-10 I/O, 28/5/73) | signed and dated in the source; one instruction per line, letter-spaced banners, formal CALLING SEQUENCE / RETURN INFORMATION contracts |
| the `"N10` patches | ~120 conditional blocks porting the machine layer | written in Lewendal's compact style, so the porter is **[ASSUMPTION]** either Lewendal or someone matching him |
| "CVS" | a modern restorer | `% Added by CVS` symbol patches in the build scripts only; no code |

Full argument: `TSS-Style-Authorship.md`.

### The NORD-10 port

**[VERIFIED]** The NORD-1 (`"NN10`) is the original target; every `"N10`
block swaps in the NORD-10 equivalent: hardware register levels and `IRW`
instead of memory vectors, a paging system (`IPGTB`, `TRR PCR`), hardware
`RDIV` instead of a software divide, `IOX` instead of `IOT`, and two
devices that exist **only** on the NORD-10 — the **swapping drum**
(`XDRUM`, device `IOX 540`) and a Versatec printer. Mag tape and plotter
were never ported. The drum driver's own header calls itself a "1st
approximation", so the port was still in progress.

---

## 2. What was done

1. **Sources recovered.** The parity bit was already cleared in `NoParity/`;
   the clean MAC sources were extracted to `Sources/`.
2. **Build scripts decoded** — `ASSYSA`/`ASSYSB` are MAC command streams,
   not shell scripts. Usable versions are in `Build/`.
3. **Bootstrap chain traced** — how TSS was actually started (below).
4. **The real MAC assembler binary reverse-engineered** in Ghidra
   (`D:\ND\BPUN\MAC.BPUN`), which yielded the byte-verified permanent
   symbol table used as ground truth.
5. **A MAC assembler was written in C** (`mac-c/`) and used to rebuild TSS,
   reconciling the result against the 1978 symbol dumps.

---

## 3. How TSS was bootstrapped **[VERIFIED from the source]**

TSS occupies core `0–37777` (16K) and keeps a copy of itself on disk in the
`CORLD` "core load" area. Two tiny routines drive everything:

- **Location 7** holds `LDT *+3; JMP I *+1; SYSSV; CORLD`. Starting the
  machine at address 7 runs `SYSSV`, which writes all 64 pages of core to
  disk at `CORLD` and then falls into `INIT`. Save-and-boot in one jump.
- **`DKRST`/`DBOOT` on disk sectors 0 and 2** do the reverse: read the
  image back from `CORLD` into core and jump to `INIT` at address 301.

Four ways in:

1. **Cold build** — MAC assembles TSS1–TSS5 straight into core; the
   operator starts at address 7. **[ASSUMPTION]** this is what the
   otherwise-undocumented `)9TSS` at the end of `ASSYSA` automated.
2. **Warm restart** — get sector 0 into core (hand-keyed loader or hardware
   disk bootstrap, **not present in this archive**), then `DKRST`.
3. **Boot tape** — `TDUMP`, running *under* TSS at 40000, punches one
   self-contained tape: an ASCII section depositing a reader bootstrap plus
   block loader, then binary blocks that write `DKRST` to sector 0, the
   image to `CORLD`, the image again into core, and finally a trailer that
   jumps to address 7. One tape both installs and starts the system.
4. **Virgin disk** — `MINIT` initialises the disk and file system first.

Minimum inventory: NORD-1 or NORD-10 with 12/14/16K core, CDC or NCR disk,
console TTY, paper-tape reader (and punch to make tapes), optionally the
drum for the `N10`+`DRUM` build.

---

## 4. The MAC assembler binary **[VERIFIED]**

`D:\ND\BPUN\MAC.BPUN` is **MAC dated 14 March 1978**, the SINTRAN III
subsystem build, loading at octal `145000–177777`.

- **Cold-start entry = octal 173724** (`0xF7D4`), confirmed dynamically in
  the nd100x emulator: it loads the workspace pointer, sets `B = 0xE87B`
  (the global pointer pool **is** the base register), calls init, and its
  first act is printing CR via `SAT 1; MON 2`.
- **It cannot run without SINTRAN.** Every I/O path is a monitor call
  (`MON 1/2` + `MON 65`), `)9EXIT` is literally `MON 0`, and there is no
  console IOX and no MON handler in the image. On bare metal it loads
  correctly, then loops forever retrying its first character.
- **The tape has no autostart** (start word 0), matching the emulator's
  "Action: 0000000".
- **Paper tape**: reading is native (`IOX 400/402/403`); punching is
  `MON 2` only — there is no punch IOX anywhere in the image.
- **`)9TSS` solved**: it and `)9EXIT` share one handler (`MON 0`), so
  `)9TSS` is a **custom alias added for the TSS restoration**. No standard
  MAC has it.

Full notes, including the permanent symbol table layout and the complete
MON-call inventory: `MAC-BPUN-Analysis.md`.

---

## 5. Rebuilding TSS today

A MAC assembler was written in C (`mac-c/`) because no MAC binary can run
without SINTRAN. It now executes the **original 1973 `ASSYSA` command
stream verbatim** — `)9ASSM TSS1,LIST1,0` … `)9ASSM TSS5,LIST5,ASYMB:SYMB`
followed by `)LIST` — and produces `ASYMB:SYMB` itself.

Result, measured against the archived symbol dumps:

| build | golden symbols | exact matches | errors |
|---|---|---|---|
| **A** (`ASSYSA`) | 693 | **679** | 0 |
| **B** (`ASSYSB`, DEBUG) | 689 | **675** | 0 |

**[VERIFIED]** 13 of the 14 unmatched entries in each build are **macro
names**, which real MAC lists with the address of the macro body *inside
MAC's own memory image* (values 146152…146627). Those addresses are
artifacts of MAC's internal layout and cannot be reproduced by a host
assembler. Excluding them, **679 of 680 reproducible symbols match**.

Known remaining discrepancies: `QOV1C` is one word off in the TSS3 EDIT
overlay, and `GRP2` is produced where the original expunged it.

---

## 6. Where the drum driver is

**[VERIFIED]** `Sources/DRUM-DRIVER.SYMB` is a verbatim extraction of
TSS1 lines 3695–3877:

- **`XDRUM`** — the NORD-10 drum driver. Device `IOX 540`; function codes
  0=read, 1=write, 2=read-test, 3=compare, 20=read-status; three exits
  (error / busy / finished); 32 sectors per track with automatic
  track-boundary splitting; word count = blocks × 64.
- **`TRSFR`** — the swap front end: pages below 4×`DRMSZ` go to the drum,
  above it to the CDC disk. Without the `DRUM` mark, `TRSFR = TRXX`
  (disk-only).

It is compiled **out** of both archived builds — neither sets `DRUM` or
`N10`. `Build/ASSYS-DRUM-N10-MAC-INPUT.SYMB` enables it.
