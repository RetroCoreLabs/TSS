# NORD TSS Source Archive — Analysis

Analysis of `E:\Dev\Ronny\TSS` (NoParity variants, bit 7 cleared).
Date: 2026-07-19. Cross-checked against the MAC manual
`E:\Dev\Ronny\NDInsight\Reference-Manuals\ND-60.096.01 MAC Interactive Assembly and Debugging System User's Guide.md`.

Everything below is marked **[VERIFIED]** (read directly from the files / manual) or
**[ASSUMPTION]** (plausible interpretation, not proven).

---

## 1. What this is

**[VERIFIED]** This is the **NORD Time Sharing System (TSS) by Bo Lewendal** — a complete,
self-contained timesharing OS for NORD-1 / NORD-10, written in **MAC assembler**
(NOT NPL — there is no NPL anywhere in these files; the `FOR J:=0 TO NDPGS-1 DO`
lines are pseudo-code comments describing the algorithm, prefixed with `%`).

The files are NOT all "build log files". The breakdown:

| File | Kind | What it is |
|---|---|---|
| `TSS1.SYMB` … `TSS5.SYMB` | **SOURCE** | The real MAC source of TSS, split in 5 parts (tab-indented) |
| `TSS1.ORG` … `TSS5.ORG` | **SOURCE — the 1973 ORIGINAL** | **[VERIFIED]** `.ORG` = original. These are a genuinely *different version* from the `.SYMB` files, not a whitespace variant. See the correction below. |

> **CORRECTION.** An earlier revision of this document claimed the `.ORG`
> files were "line-for-line identical to the .SYMB files, only whitespace
> differs". That was based on line counts alone and is **wrong**. Comparing
> the content (parity stripped, whitespace normalised) shows **153 changed
> lines** across TSS2–TSS5 (TSS1 is indeed whitespace-only). Every change is
> a symbol rename: `STR`→`XTR` (77), `STRX`→`XTRX` (8), `LSS`→`XSS` (7),
> `STR2`→`XTR2`, `STR1`→`XTR1`, `LSS`→`LSSX` (2 each), `STR0`→`XTR0` (1).
>
> The reason is verifiable: `STR` and `LSS` **are symbols in MAC's permanent
> table** (`STR`=030000, `LSS`=002400), so TSS's own labels of those names
> collided with the assembler. `ASSYSA` says so — *"STR and LSS are defined
> in the MAC/FMAC symbol table … CVS: STR has been substituted by XTR"*.
> So `.ORG` is Lewendal's 1973 source and `.SYMB` is CVS's patched copy.
>
> The rename is also **incomplete**: `STR`, `STR0`, `STR1`, `STR2`, `STR1X`
> and `STR2X` are referenced but never defined in the patched sources, which
> accounts for six of the twenty symbols left undefined by a full rebuild.
> Reproduce with `mac-c/diff_cvs_vs_original.sh` and
> `mac-c/check_rename_gaps.sh`. Full detail in `archive/README.md`.
| `ASSYSA.SYMB` | **BUILD SCRIPT** | MAC console/mode file that assembles version **A** (SYSA) |
| `ASSYSB.SYMB` | **BUILD SCRIPT** | Same for version **B** (SYSB, with `DEBUG` mark set) |
| `LIST1.SYMB` … `LIST5.SYMB` | **BUILD OUTPUT** | MAC *list stream* output of assembling TSS1…TSS5 (`)9ASSM TSSn,LISTn,0`). LIST1 is **truncated mid-line** |
| `LIST.SYMB` | **BUILD OUTPUT** | One combined list-stream output covering **TSS3 + TSS4 + TSS5** in a single run (verified: TSS4 content starts at line 2861, TSS5 at line 7358, ends with `)LINE`) |
| `ASYMB.SYMB` | **BUILD OUTPUT** | Symbol table dump of the version-A build (`)LIST` to the object stream, per manual §4.2.3.3) — every symbol with its final octal value |
| `BSYMB.SYMB` | **BUILD OUTPUT** | Same for version B |
| `MINIT.SYMB` | **SOURCE** | Standalone "Mass Storage Initialization Program" (CDC disk, NORD-1 or NORD-10) |
| `TDUMP.SYMB` | **SOURCE** | Standalone "TSS Dumper" (loads at 40000, NCR/CDC, NORD-1/NORD-10) |

**[VERIFIED]** Recovered clean source set copied to **`E:\Dev\Ronny\TSS\Sources\`**
(9 files: TSS1–TSS5, ASSYSA, ASSYSB, MINIT, TDUMP — all `.SYMB`).
On the original ND system these would be `TSS1:SYMB` … `TDUMP:SYMB`.

---

## 2. How it is built (the ASSYSA build script, decoded)

`ASSYSA.SYMB` contents, line by line — **[VERIFIED]** against the MAC manual:

```
FMAC                          ← [ASSUMPTION] command to start the FMAC assembler
100                           ← [ASSUMPTION] answer to a table-size / start prompt
SYSA                          ← [ASSUMPTION] output/system name typed to a prompt
CDC MACF DIAB K14 TEL4        ← library marks: referencing an undefined symbol makes
                                 it "true" for conditional assembly ("MARK ... " blocks)
IOT=160000;ACT=400;SKA=1000;PIN=2000;SNI=0    % Added by CVS
INTDS=150440;INTEN=150540                     % Added by CVS
TDBI=410; TDBO=40                             % Added by CVS
DIABD=156; TT3=162; TT4=DIABD; DIABT=4; DLP=143  % Added by CVS
GRP1=1;GRP2=2;GRP3=4;GRP4=10;
MPR=3;
8LP=200; 9PPT=100; 9TTI=40; 9TTO=40
9FP=200; 9CR=5; NOPEN=16
)9ASSM TSS1,LIST1,0           ← assemble source TSS1, listing → LIST1, no object
)9ASSM TSS2,LIST2,0
)9ASSM TSS3,LIST3,0
)9ASSM TSS4,LIST4,0
)9ASSM TSS5,LIST5,ASYMB:SYMB  ← last part: object stream → ASYMB:SYMB
)LIST                         ← dump symbol table to object stream = ASYMB:SYMB
*:                            ← [UNKNOWN]
?                             ← [UNKNOWN]
)9TSS                         ← [UNKNOWN — not in the MAC manual; possibly a
                                 patched/site-specific MAC command to start TSS]
```

`)9ASSM INFILE,LISTFILE,OBJECTFILE` semantics, `)LIST`, `)LINE`, `)KILL`, `)PCL`,
`)FILL`, `)MCDEF`, and library marks are all documented in ND-60.096.01.
Each TSSn part ends with `)LINE` (returns control to the console → next script line runs).

**Version B** (`ASSYSB.SYMB`) is identical except: lowercase `fmac`, name `SYSB`,
adds the `DEBUG` library mark, produces no listings, object/symbols → `BSYMB:SYMB`.

**[VERIFIED]** The `% Added by CVS` lines exist only in ASSYSA/ASSYSB/MINIT/TDUMP —
someone with initials/handle "CVS" already did a restoration pass and defined the
symbols that MAC/FMAC originally had built in (NORD-1 I/O opcodes `IOT`, `ACT`, `SKA`,
`PIN`, `SNI`; interrupt ops `INTDS`/`INTEN`; device numbers). The comments also say
`STR`→`XTR` and `LSS`→`XSS` substitutions were made. The presence of the LIST* output
files that exactly match the names in ASSYSA implies **this script has already been run
successfully through a MAC assembler at least once** [ASSUMPTION: by CVS, recently].

### Configuration marks (the "device list" your friend sent)

The list your friend sent is exactly the header comment block of `TSS1.SYMB`
lines 8–58 — the definable library marks / symbols with their NCR- and CDC-defaults
(TT1–TT12, DLP, DCR, DFP, DSP, MT, plus NCR/CDC/DRUM/DEBUG/N10/K4-K16/TEL4-TEL14 etc.).
The as-found builds use: **CDC disk, MACF, Diablo terminal support, 14K system,
4 teletypes, no DRUM, no N10** (NN10 default = NORD-1 I/O instructions `IOT`).

---

## 3. The DRUM driver — YES, it exists

**[VERIFIED]** `TSS1.SYMB` (Sources copy) contains a complete swapping-drum driver:

### 3.1 `XDRUM` — the low-level drum read/write driver
`E:\Dev\Ronny\TSS\Sources\TSS1.SYMB` lines **3730–3874**, inside library-mark block
`"DRUM N10` (i.e. only assembled when both `DRUM` and `N10` marks are set).

Header comment (verbatim): *"DRUM TRANSFER ROUTINE — THIS IS 1. APPROXIMATION TO A
NORD-10 VERSION, NJL 17/4/73"* — so it is NORD-10 code (uses `IOX`), author initials
NJL, dated 17 April 1973.

Interface (from the code, verified):

```
CALL:   JPL I (XDRUM
        JMP ERROR      % error exit    (X = drum status register)
        JMP BUSY       % busy exit     (call again with X,T,A,D unchanged)
        JMP FINIS      % finished exit (X = status, A = core address)

  X = number of blocks
  T = function code: 0=READ 1=WRITE 2=READ TEST 3=COMPARE 20=READ STATUS ONLY
      bits[8,9] = core address bits [16,17]  (>64K addressing!)
  A = core address
  D = block (drum) address
```

Hardware programming model (verified from the code):

```
DRM=540    drum main device number (octal)     accessed via IOX DRM+offset
  +0  RCX  read core address
  +1  LCX  load core address
  +3  LBX  load block address
  +4  RSX  read status register
  +5  LCR  load control register
  +7  LWX  load word count
status bits: 020 DVA device active, 040 ERR inclusive-or of errors
control reg: function<<13 | addr-bits-16/17 | 7 (enable interrupt + activate)
```

Geometry handling (verified): 32 sectors per track (`SAT 37; RAND SD DT` = sector
within track, `40 - sector` = sectors left in track); word count = blocks × 64
(`SHA ZIN 6`); it splits a multi-block transfer at track boundaries and computes
new core/block addresses for the continuation (busy-exit / re-call model, so it can
run interrupt-driven). The error path also does `IOX 545` [ASSUMPTION: clears/acks
the drum interrupt device 545 = DRM+5].

### 3.2 `TRSFR` — the swap-transfer front end that mixes disk + drum
Lines **3711–3728** (`"DRUM N10` variant): TSS's swapper calls `TRSFR`; addresses
below `4×DRMSZ` go to the drum via `XDRUM` (loop-on-busy: `JPL XDRUM; JMP TRSF1; JMP *-2`),
addresses above spill to the CDC disk via `TRXX`/`DKTR`. When `DRUM` is not set,
`TRSFR=TRXX` (line 3712) — pure disk swapping.

### 3.3 Related
- `DRMSZ` = drum size in 256-word pages, default 2000 octal (line 46, 98–101).
- `PDTBL` (line 4679) — "PHYSICAL DRUM TABLE", 2×`NDPGS` words; the page allocator
  treats drum pages as the swap pool.
- The CDC **disk** driver is also in TSS1: `DKTR` at line 3417 ("DISK TRANSFER
  ROUTINE", NORD-1 `IOT ACT DISC` style), NCR→CDC address conversion at 3564,
  plus 2K file-system transfer routines at 3628/3656.

**Important consequence [VERIFIED logic]:** the archive's own build scripts do NOT
set `DRUM` or `N10`, so the drum driver is compiled OUT of SYSA/SYSB as found. To
get a drum-swapping NORD-10 TSS you add `DRUM N10` to the mark line — and check the
`"DRUM N10"`-guarded code paths, which the original authors called a "1st approximation".

---

## 4. Can we assemble it with modern ND-100 tools?

Short honest answer: **not with the NPL compiler, and not directly with nd100-as —
but the path is realistic.**

- **NPL is the wrong tool** — this is MAC assembler source, a completely different
  language. Nothing to compile with NPL. [VERIFIED]
- **The MAC dialect is heavily used and non-trivial** [VERIFIED from sources]:
  - Library marks `"MARK … "` conditional blocks (hundreds of them)
  - `)KILL` + re-definition and `)PCL` (partial clear) used as a *local-symbol
    scoping system* — e.g. `TCL,`/`TCT,` are redefined per routine. Any converter
    must model MAC's symbol-table time semantics, not just do text substitution.
  - Literals `(expr` with `)FILL` dumps
  - `)MCDEF` macros with `$A1…` parameters (GOVER/OVERL/OVERX overlay machinery)
  - `location/` counter setting, `;` multi-statement lines, `%` comments,
    octal-default numbers, `*` current location, `'text'` word-packed strings
  - `)9MOVE`, `)8DUMP`, `)BPUN`-style memory commands executed at assembly time
    (the overlay system *moves assembled code* and writes it to disk mid-assembly!)

Realistic options, in order of fidelity:

1. **Run the real MAC/FMAC under an ND emulator** (nd100x / RetroCore) and feed it
   `ASSYSA.SYMB` as console input — this is exactly what the file is. The overlay
   `)9MOVE`-at-assembly-time tricks then work for free. **Blocker: do we have a
   MAC or FMAC binary/paper-tape image?** I could not verify one in this folder.
   Note TSS's overlay machinery *writes overlays to disk during assembly* and
   `7/ LDT *+3 … SYSSV; CORLD` saves the built system to disk — the original
   workflow assembles TSS *into memory on the target machine* and saves it.
2. **Write a MAC-dialect cross-assembler** (or a MAC-mode front end for the
   existing nd100-as). The manual ND-60.096.01 is a complete spec; ASYMB/BSYMB
   give a **golden symbol table** (every symbol's final octal value for both
   variants) to verify against — an unusually good test oracle.
3. **Mechanical translation to nd100-as syntax** — hardest to get right because of
   the `)KILL`/`)PCL` scoping and assembly-time memory commands; I'd advise against.

Target-CPU note [VERIFIED]: the default (NN10) build uses NORD-1 `IOT`-style I/O.
For running on an ND-100-family emulator you want the **N10** build (`IOX`,
`ION`/`IOF`), which is also the only variant containing `XDRUM`.

### Missing pieces / open questions
- MAC/FMAC assembler binary (see above) — **the** missing tool.
- `)9TSS`, `*:`, `?` at the end of ASSYSA — not in the manual [UNKNOWN].
- `LIST1.SYMB` is truncated mid-line (ends `SMPR, STA SMPA; STX S`) — listing only,
  source TSS1 is complete (ends properly with `)LINE`). No action needed.
- All five TSSn sources end with `)LINE` and look complete. I found no reference to
  any additional source file beyond what is present.
- Runtime environment: TSS expects MAC resident in memory (comment "RETURNS TO MAC
  VIA JMP 103,B" in the overlay writer; MINIT calls MAC's `INBT`/`OUTBT`).

---

## 5. Parity-clearing caveat found

**[VERIFIED]** `NoParity\LIST.SYMB.TXT` contains 245 bytes of `0x1D` (GS) that were
`0x9D` in the original — all inside string constants like `MS1, 'P = <0x9D> '`
(register-dump message texts). Inside `'…'` string data, bit 7 is not necessarily
parity — clearing it may have altered real string data. Harmless here (listing file,
and the master TSSn sources contain no such bytes — verified scan), but keep the
originals for any byte-level work. All other NoParity files scanned clean.

---

## 6. How TSS was bootstrapped **[VERIFIED from source, interpretations marked]**

TSS lives in core 0–37777 (16K) and keeps a copy of itself on disk in the
"core load" area `CORLD` (version A and B have separate areas, `CORL1`/`CORL2`,
TSS1 lines 2611–2620; in TDUMP: CDC A=60, CDC B=260, NCR A=10, NCR B=210).
Disk layout (TSS1 2595–2608): user table, 12 swap areas `DKA1..DKA12`
(200 octal pages each), then the file system `FSYS`.

### The four ways in

1. **Cold build (MAC-resident):** MAC assembles TSS1–TSS5 directly into core
   0–37777. Location 7 holds `LDT *+3; JMP I *+1; SYSSV; CORLD` (TSS1 line 70,
   "%SAVE SYSTEM ON THE DISK"). Starting at 7 runs `SYSSV` (TSS1 4078–4087):
   writes 100 octal × 400-word pages (= the whole 16K image) to disk at
   `CORLD` using the disk driver, then falls into `INIT` — TSS is up.
   **[ASSUMPTION]** the operator started it with MAC's `7!` run command;
   plausibly what the mysterious `)9TSS` at the end of ASSYSA automated.

2. **Warm restart from disk:** `TDUMP` plants a disk-restart routine on
   **disk sectors 0–2**: `DKRST` (sector 0, with the word `CORLD` at address 1)
   + `RDKOP`/`DBOOT` (sector 2). Executing DKRST reads the 16K image back
   from `CORLD` into core 0–37777 and jumps to `301` = `INIT`
   (TDUMP 264–327; TSS3 240–275 carries the same routines inside TSS).
   What reads sector 0 into core initially: a hand-keyed mini-loader or
   hardware disk bootstrap **[ASSUMPTION — not in the archive]**.
   On NORD-10 there is additionally the restart entry `20/ JMP I *+1; 301`
   (TSS1 197) — hitting restart at 20 re-INITs a resident image.

3. **Boot/distribution tape (made by TDUMP):** TDUMP runs *under* TSS as a
   user program at 40000 (its 161xxx service calls are instructions trapped to
   the TSS monitor — cf. LEV14 "MEMORY PROTECT INTERRUPT" decode, TSS1 4092+).
   It asks for the saved core-image file and punches ONE self-contained tape:
   1. ASCII section: deposits `HLOAD` (hardware reader bootstrap, NN10 `IOT`
      or N10 `IOX 400/402/403` variants) + `TBOOT` (block loader) as
      octal-text with `/` and a final `!` — i.e. MAC-loader command format
      **[ASSUMPTION on the consumer]** (TDUMP 78–126, 221–235).
   2. `TBOOT` then reads binary load blocks (core addr, disk addr, word
      count, data, checksum) from the same tape:
      - DKRST → **disk sector 0**, RDKOP/DBOOT → **disk sector 2**
      - the 16K TSS image → **disk `CORLD` area**
      - the same image again → **core 0–37777 directly**
      - trailer block: **start address 7** → SYSSV re-saves + INIT → TSS up.
   So one tape both installs the disk-resident system AND cold-starts it.

4. **Virgin disk:** `MINIT` ("MASS STORAGE INITIALIZATION PROGRAM") is run
   first to initialize the disk/file system structures; it talks to the
   console via MAC's `INBT`/`OUTBT` (or its own IOXLIB on NORD-10) —
   i.e. it runs in a MAC environment, before TSS exists.

### Minimum inventory to bootstrap

- NORD-1 or NORD-10, 12K/14K/16K core per the K12/K14/K16 mark
- CDC (or NCR) disk; console TTY; paper-tape reader (tape boot path);
  paper-tape punch (to make boot tapes); optionally drum (N10 + DRUM build)
- Software: MAC/FMAC + these sources (path 1), or a TDUMP-produced boot tape
  (path 3), or an already-installed disk + a way to read sector 0 (path 2);
  MINIT once for a virgin disk

## 7. Suggested next steps

1. Locate a MAC or FMAC binary (paper tape image / :BPUN) — that unlocks the
   authentic build path inside nd100x/RetroCore.
2. Failing that, build a MAC-mode assembler using ND-60.096.01 as spec and
   ASYMB/BSYMB as the verification oracle.
3. For a drum-swapping system: build with marks `CDC MACF K14 TEL4 DRUM N10`
   (and review every `"N10`/`"DRUM` block — the drum driver is self-described
   as a first approximation).
