# TSS 3.0 Bring-Up and MINIT — deep, source-grounded reference

> **Status / evidence marking.** Every claim below is tagged:
> - **[VERIFIED]** — read directly from the cited source line(s) in this repo.
> - **[ASSUMPTION]** — my interpretation of verified facts; reasoning stated.
> - **NOT DETERMINED FROM SOURCE** — cannot be settled from the files read; the
>   pointer to where it *would* be resolved is given.
>
> Citations are `src/FILE:LINE`. Line numbers are from the current `src/` copies
> (`MINIT.SYMB` = 810 lines, `TSS1.SYMB`, `TSS2.SYMB`). This document covers the
> MINIT internals and the cold-to-login runbook. It **cross-references** and does
> not duplicate `docs/DISK-INIT-USERS-AND-BOOT.md` and
> `docs/HANDOFF-LOGIN-BRINGUP.md`; read those for the switch table and the
> login-authentication path.

---

## 0. One-paragraph orientation

MINIT (`src/MINIT.SYMB`, "MASS STORAGE INITIALIZATION PROGRAM", line 1) is a
**standalone operator tool**, not part of the TSS OS image. It lays down the
disc-resident free-track bitmap (MIB) that the running kernel's track allocator
`GTRK` later consumes, so that the first cold-boot's `SINIT` can create user
`SYSTEM`. It is self-contained: it carries its own TTY I/O, octal I/O, message
printer, and the full CDC disc driver (`DKTR`/`DKOP`/`DKADR`/`DWAIT`), and it
selects NORD-1 vs NORD-10 code from library marks. It builds (via
`mac-c/build_minit.sh`) to `Build/minit/minit.bpun` with image base `000000`,
0 errors. **[VERIFIED]** by the file contents and the build script.

---

## 1. MINIT source structure (map of the file)

`src/MINIT.SYMB`, in order **[VERIFIED]**:

| Lines | Content |
|---|---|
| 1–2 | Title; CVS-added equates `IOT/ACT/SKA/PIN/SNI` (line 2) |
| 8 | Bare `CDC` mark (line 8) — selects the CDC-disc code paths throughout |
| 11–15 | `NN10` / `"N10 )KILL NN10 "` — defines the "not-NORD-10" mark; killed when `N10` is set (line 11–15) |
| 24–45 | `TCI` (TTY char in), `TCO` (char out) |
| 49–71 | `CRLF`, `TCF` (I/O error → prints `I/O ERROR`, `WAIT`, restarts `MINIT`) |
| 75–94 | `MSG` (print packed-string message to TTY) |
| 98–132 | `OCTIN` (read octal number), `OCTOT` (print octal number) |
| **134–221** | **`MINIT` main + branches `M1..M5`, `MSS`, fault handlers `MF1..MF3`, `MUP`, `ER1`, `REG..REG6`** |
| 224–225 | `MASKS` table — 16 single-bit masks `1,2,4,...,100000` |
| 228–243 | Operator prompts `MS1..MS6` and error strings `ERM1..ERM8` |
| 246–265 | `SBIT` — set/reset one bit in the MIB bit table |
| 267–304 | Device-number equates; `DCHN`, `DISC`, `LCA/LBA/LMR/RST/...` |
| 307–327 | `DWAIT` — disc ready/timeout test |
| 333–360 | `DKOP` — one 256-word disc operation |
| 364–519 | `DKTR` — disc transfer routine (the real driver) |
| 526–587 | `DKADR` — NCR→CDC address conversion, `DKLIM=626` |
| 590–796 | NORD-10 (`"N10`) I/O library: `INBT`, `OUTBT`, `IOINI` |
| 798–805 | NORD-1 (`"NN10`) I/O: `INBT`/`OUTBT` = `IOT` polling; `IOINI` = `EXIT` |
| 808–810 | `MIB=*`, `BUFF=MIB+4000`, `)LINE` |

---

## 2. The MIB bitmap and its geometry constants

### 2.1 Where MIB and BUFF live

**[VERIFIED]** `src/MINIT.SYMB:808-809`:
```
MIB=*                 % MIB is the current location counter after all code
BUFF=MIB+4000         % BUFF is 4000(octal) words above MIB
```
So MIB is a **4000₈-word (2048-decimal) region** placed immediately after the
program body, and BUFF is a second 4000₈-word region above it. The `M1` clear
loop confirms the size: it zeroes `MIB[0..4000₈)` (`src/MINIT.SYMB:143-144`,
`LDA CNT; SUB (4000; JAN M1`). The `M3` loop likewise zeroes `BUFF[0..4000₈)`
(`:150-151`).

**[ASSUMPTION]** MIB is a 2048-word (= 4000₈) bitmap → 2048×16 = 32768 track
bits, indexed by NCR address. Reasoning: bits are addressed as
`word = NCR >> 7`, `bit = NCR >> 4 & 17` (see §2.3), and the clear loop bound is
exactly 4000₈ words.

### 2.2 MASKS table

**[VERIFIED]** `src/MINIT.SYMB:224-225`: `MASKS` is the 16 powers of two
`1,2,4,10,20,40,100,200,400,1000,2000,4000,10000,20000,40000,100000` (octal),
i.e. `MASKS[k] = 1<<k` for bit `k` in a 16-bit word.

### 2.3 The bit-addressing math (from INITIALIZE, `M4/M5`)

**[VERIFIED]** `src/MINIT.SYMB:165-167`:
```
LDA ADRI; SAD SHR 7; ADD (MIB; STA MIBX      % word index = ADRI >> 7, + MIB base
SAA 0; SAD 4; ADD (MASKS; COPY DX SA         % bit index  = (ADRI >> 4) & 17
LDA 0,X; ORA I MIBX; STA I MIBX              % set that bit in the MIB word
```
And the step to the next track **[VERIFIED]** `:168`: `LDA ADRI; AAA 10; STA ADRI`
— **NCR addresses advance by 10₈ (8 decimal) per track**. So each track occupies
one NCR unit of 8; `>>4 & 17` selects one of 16 bits per word; `>>7` selects the
word. **[ASSUMPTION]** the low 3 bits of an NCR address are within-track
(sector) and bits [3..] index the track; consistent with `AAA 10` stepping and
`SHR 4`/`SHR 7` shifts.

The same math is centralised in `SBIT` **[VERIFIED]** `src/MINIT.SYMB:252-259`:
```
SBIT,  STF SBTAD; STX SBX
       SHT ZIN SHR 3; COPY DA ST            % T = addr >> 3
       SAD ZIN SHR 4; ADD (MIB; STA MIBX    % word  = (T>>4)+MIB
       SAA 0; SAD 4; ADD (MASKS; COPY DX SA % bit   = T & 17
       LDA SBTAD+1; JAZ SB1                 % A operand: 1 → set, 0 → clear
       LDA 0,X; ORA I MIBX; JMP SB2         % set bit
SB1,   LDA 0,X; COPY CM1 DA SA; AND I MIBX  % clear bit (AND NOT mask)
SB2,   STA I MIBX ...
```
`SBIT` takes `T = disc address`, `A = 1 (set) / 0 (clear)`.

### 2.4 What the MIB looks like after INITIALIZE

**[VERIFIED]** control flow `MIT..M5` (`src/MINIT.SYMB:149-172`):

1. `M1` (`:143-144`) zeroes the whole MIB (all bits 0).
2. `MIT` (`:149`) zeroes `BUFF` and **writes zeroed test data to each track**
   (`:152-154`, two `DKTR` calls, disc error → `MF1`), then reads each track
   back and verifies it came back all-zero (`M4`, `:163-164`: `LDA I CNT,X;
   JAF MF2` — any non-zero word fails the verify → `MF2`).
3. **On SUCCESS** (the track wrote, read, and verified clean) control reaches
   `:165-167`, which **SETS** that track's MIB bit (`LDA ADRI; SAD SHR 7;
   ADD (MIB ...; ORA I MIBX; STA I MIBX`).
4. **On FAILURE** the disc-error paths `MF1`/`MF2` (`:173-182`) print the report
   and end with `JMP M5` — jumping **past** the bit-set at `:167`, so a bad
   track's bit stays **0**.
5. `M5` (`:168-169`) advances `ADRI += 10₈` and loops until `ADRI >= ADRN`.

**[VERIFIED]** after INITIALIZE, **a set bit (1) means the track is GOOD and
FREE; a clear bit (0) means the track is BAD or in use.** The MIB starts all-zero
(`M1`), and the *only* place INITIALIZE sets a bit is the success path at
`:165-167`; both failure paths (`MF1`→`M5`, `MF2`→`M5`) skip it. Good tracks are
set, bad tracks are left clear. Finally `M5` at `:170-171` writes the MIB itself
to disc (`LDX (MIB; SAT 50; SAA 7; ... JPL I (DKTR`) — 50₈ blocks starting at
NCR address `50` (`SAT 50`).

> **Reconciled with the kernel's `GTRK`:** this polarity agrees with the running
> allocator. `GTRK` (`src/TSS2.SYMB:2663-2682`) scans the bit table for a word
> `≠0` (`:2671` `JAZ G4` skips fully-allocated all-zero words) and **claims a
> track by CLEARING its bit** — i.e. a **set bit = free**, cleared = taken.
> That is exactly the polarity MINIT lays down here, and it matches the
> `mac-c` corpus note ("GTRK scans the MIB bitmap for a set (free) bit").
> **NOT DETERMINED FROM SOURCE:** the exact on-disc load path from MINIT's MIB
> (written at NCR 50) into the kernel's core bit table `BITBL`/`BITM`. That
> linkage lives in the TSS disc/bootstrap load code, not in MINIT; to close it,
> trace how `BITBL` is filled at startup in `TSS2`/`TSS1`.
>
> (An earlier draft of this section had the polarity inverted; corrected against
> `src/MINIT.SYMB:163-182` and cross-checked with `TSS-FILESYSTEM.md`.)

---

## 3. MINIT operator dialogue — exact prompts and errors

**[VERIFIED]** `src/MINIT.SYMB:228-243`. `$` = CR/LF in these packed strings
(the `MSG` routine at `:84-87` treats `##\`→string-end, `##\-##$`→CRLF).

Prompts:
| Sym | Line | Text (as stored) |
|---|---|---|
| `MS1` | 228 | `MASS STORAGE INIT` / `FIRST DISK ADDRESS (NCR): ` |
| `MS2` | 229 | `LAST DISK ADDRESS (NCR): ` |
| `MS3` | 230 | `INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): ` |
| `MS4` | 231 | `NITIALIZE` (echo tail after operator types `I`) |
| `MS5` | 232 | `PDATE` (echo tail after `U`) |
| `MS5A`| 233 | `EGENERATE` (echo tail after `R`) |
| `MS6` | 234 | `FINISHED` |

Errors:
| Sym | Line | Text |
|---|---|---|
| `ERM1` | 236 | `ILLEGAL ADDRESSES` |
| `ERM2` | 237 | `DISK ERROR ` |
| `ERM3` | 238 | ` AT NCR(` |
| `ERM4` | 239 | `) CDC(` |
| `ERM5` | 240 | `)` |
| `ERM6` | 241 | `TRANSFER ERROR` |
| `ERM7` | 242 | `UNABLE TO WRITE MIB` |
| `ERM8` | 243 | `NOT IMPLEMENTED` |
Plus `TCFS` = `I/O ERROR ` (`:69`).

### 3.1 Entry and prompt sequence — `MINIT`

**[VERIFIED]** `src/MINIT.SYMB:137-148`:
```
MINIT, JPL I (IOINI                              % init I/O devices
       LDX (MS1; JPL I (MSG                       % print banner + FIRST prompt
       JPL I (OCTIN; AND (77770; STA ADR1; STA ADRI  % read FIRST NCR, mask low 3 bits
       LDX (MS2; JPL I (MSG                       % LAST prompt
       JPL I (OCTIN; AND (77770; STA ADRN         % read LAST NCR, mask low 3 bits
       SUB ADR1; JAP *+2; JMP I (ER1              % LAST<=FIRST → ILLEGAL ADDRESSES
       STZ CNT; LDX (MIB
M1,    STZ I CNT,X; MIN CNT                       % zero MIB
       LDA CNT; SUB (4000; JAN M1
M1A,   LDX (MS3; JPL I (MSG; JPL I (TCI           % prompt I/U/R, read one char
       AAA -##I; JAF M2; LDX (MS4; JPL I (MSG; JMP MIT      % 'I' → INITIALIZE
M2,    AAA ##I-##U; JAF M2A; LDX (MS5; JPL I (MSG; JMP I (MUP  % 'U' → UPDATE
M2A,   AAA ##U-##R; JAF M1A; LDX (MS5A; JPL I (MSG; JMP I (REG % 'R' → REGENERATE
```

- `AND (77770` **[VERIFIED]** masks the low 3 bits of the entered NCR address —
  i.e. addresses are rounded down to an 8-unit track boundary (matches the
  `AAA 10` step of §2.3). **[ASSUMPTION]** low 3 bits = intra-track sector.
- `ER1` (`:188`) prints `ILLEGAL ADDRESSES` and restarts `MINIT`.
- Any char other than I/U/R loops back to `M1A` (re-prompt) — `M2A` `JAF M1A`.

### 3.2 The three branches

- **INITIALIZE (`I`) → `MIT`** (`:149-172`): surface-test every track FIRST..LAST,
  build MIB (good/free tracks set, bad tracks left clear), write MIB to disc.
  Detailed in §2.4.
- **UPDATE (`U`) → `MUP`** (`:184`): **`MUP, LDX (ERM8; JPL I (MSG; JMP I (MINIT`**
  — prints **`NOT IMPLEMENTED`** and restarts. **[VERIFIED] UPDATE is a stub in
  this source.**
- **REGENERATE (`R`) → `REG`** (`:192-214`): rebuilds the MIB from the existing
  filesystem — see §4.

**FIRST / LAST DISK ADDRESS (NCR) meaning [VERIFIED from use]:** they are the
inclusive NCR-address range MINIT operates on. INITIALIZE loops
`ADRI = ADR1; ADRI < ADRN; ADRI += 10₈` (`:168-169`). Both are rounded to
`AND 77770`. **[ASSUMPTION]** they let the operator format a sub-range or the
whole pack.

### 3.3 The I/U/R choice, in plain terms

**[ASSUMPTION]** from the code semantics:
- **I(nitialize)** — destructive surface test + fresh empty bitmap (marks bad
  tracks). Use on a bare/new pack.
- **U(pdate)** — intended incremental update; **not implemented here**.
- **R(egenerate)** — non-destructive: walk the live user/file/track structures
  and rebuild the MIB to match what's actually allocated. Use to repair a
  corrupted bitmap without losing data.

### 3.4 REGENERATE detail

**[VERIFIED]** `src/MINIT.SYMB:192-214`. `REG` marks every track in the range as
allocated (`REG1` sets bits via `SBIT` with A=1), then walks the user table
(340₈ user slots, `REG2` at `:197-212`), each user's UIB / file index blocks,
each file's track chain, calling `SBIT`/`DKOP`/`DKTR` to set the bits for tracks
actually in use, and finally `REG6` (`:213`) writes the reconstructed MIB back
(`LDX (MIB; SAT 50; ... DKOP`) then `JMP I (MSS` → prints `FINISHED`. The exact
on-disc layout constants it walks (`51` user-table base at `:197`, `70` files
per user at `:211`, `340` users at `:212`) are read directly but their meaning
beyond the loop bounds is **[ASSUMPTION]**.

---

## 4. Disc addressing: NCR ↔ CDC, and the geometry MINIT expects

### 4.1 Device numbers

**[VERIFIED]** `src/MINIT.SYMB:267-304`. `DCHN=100` (NORD-1) or `DCHN=500`
(NORD-10) at `:269/271`. From it the CDC controller registers: `LCA` (load core
addr), `LBA` (load block addr), `LMR` (load modus reg), `RST` (read status),
`RCA`, `RSECT`, `SEEK`, `LWC` (`:296-303`, NORD-10 set).

### 4.2 DKADR — NCR → CDC conversion

**[VERIFIED]** `src/MINIT.SYMB:526-587`. Header (`:526-532`): input `A = NCR
disc address`, returns `A = CDC disc address`; failure return if illegal.
Key constant **[VERIFIED]** `:534-535`:
```
DKLIM=626    % NUMBER OF CYLINDERS (313 FOR CDC 9425 AND 626 FOR CDC 9427)
```
So this build targets a **CDC 9427 (626 cylinders)**; 313 = CDC 9425.
The conversion (`:537-575`):
- masks NCR to 14 bits (`AND (17777`, `:541`),
- **divides by 12 decimal** (comment `:543`, NORD-10 path uses `RDIV ST` with
  `SAT 14`, `:550-551`; NORD-1 path multiplies by 952=`52525` reciprocal,
  `:546-548`, `952,` defined at `:585`) to get the cylinder field,
- multiplies the remainder by 14 (`MPY (14`, `:561`) for the sector count,
- adds a per-unit displacement `52600` if a unit-select bit is set
  (`:565`), and
- range-checks against `DKLIM` (`SUB (DKLIM; JAP DKADF`, `:571`) → illegal
  return if out of range.

**The physical NCR→(cylinder,track,sector) mapping [ASSUMPTION]:** the "divide by
12" produces the cylinder and "×14" the sector, consistent with a CDC 9427
geometry. **NOT FULLY DETERMINED FROM SOURCE:** the exact head/track count per
cylinder and sectors/track are not spelled out as named constants in MINIT
(only 12, 14, 626, and the 52600 unit stride). The full physical geometry would
be confirmed from the CDC 9427 hardware manual / the TSS main disc driver;
within this file the addressing is purely arithmetic on the NCR number.

### 4.3 DKTR / DKOP / DWAIT

- **`DWAIT`** (`:312-327`) polls the disc ready bit (NORD-10: `IOX RST; BSKP ONE
  20 DA`, `:316`), timing out via `DWX`.
- **`DKOP`** (`:333-360`) does one 256-word operation (read/write/parity/compare),
  wrapping `DKTR`.
- **`DKTR`** (`:364-519`) is the full transfer: wait ready, load `LCA`/`LBA`,
  call `DKADR` to convert, load modus/word-count, start transfer, wait, check
  status; supports up to 3K-word multi-block transfers. This is the same driver
  the OS uses; MINIT carries its own copy so it can run bare.

**The "4000 loop bound"** referenced in the task is the MIB/BUFF size (4000₈
words), §2.1 — not a geometry constant. `AND 77770` is the track-boundary mask,
§3.1.

---

## 5. Does MINIT load/run at address 0?

**[VERIFIED, partial]:**
- The **build image base is `000000`** — `mac-c/build_minit.sh` header comment
  and the MACIMG range-check it prints ("image base 000000"); the source has **no
  explicit ORG/location directive** at the top (`src/MINIT.SYMB:1-15` are equates
  and marks only), so assembly starts the location counter at 0 by default.
- The **entry point is the symbol `MINIT`** (`src/MINIT.SYMB:137`), and the build
  passes `-e MINIT` so the BPUN autostart vector = MINIT's address, not 0
  (`build_minit.sh`: `"$MAC" ... -e MINIT`).

**[ASSUMPTION]** Because the image is based at 0 and the code sits low with MIB
and BUFF appended at the top (`MIB=*` after all code, `:808`), MINIT occupies a
contiguous low-memory block `[0 .. MIB+BUFF+4000₈)`. The exact top address =
`MIB + 2×4000₈` — **read the built `minit.img` MACIMG header for the real word
count** (the build script prints it).

**NOT DETERMINED FROM SOURCE:** whether nd100x will boot this BPUN correctly to
`MINIT`, whether the low-memory placement collides with anything the emulator
needs, and whether the CDC disc device is present/answering in the emulator.
Those are runtime questions — see §8 open items.

---

## 6. Other standalone bring-up tools in the repo

Searched `archive/` and `src/` **[VERIFIED]** (directory listing + file heads):

| File | What it is | Standalone? |
|---|---|---|
| `MINIT.SYMB` | Mass-storage init (this doc) | **Yes** — the disc formatter |
| `TDUMP.SYMB` | **"TSS DUMP"** utility (`archive/TDUMP.SYMB` line 1 title, parity-8-bit) — carries the same `MARKS`, `NN10/N10`, `DCHN` disc equates as MINIT | **Yes [ASSUMPTION]** — a standalone disc/memory dump tool; header shows base `60000`, `SKA/PIN/SNI` equates like MINIT. Not analysed in depth here. |
| `ASSYSA.SYMB`, `ASSYSB.SYMB` | MAC **command streams** (build recipes), not programs | No — assembler input |
| `TSS1..TSS5.SYMB` | The TSS OS itself (5 parts) | No — the OS image |
| `LIST*.SYMB`, `ASYMB/BSYMB.SYMB` | Golden listings / symbol dumps | No — reference data |

The **assembler itself** (`mac-c/mac-as`) is the other host-side tool but is the
C reimplementation, not a NORD program. **[VERIFIED]** no other standalone NORD
bootstrap/loader source is present in `archive/` or `src/` beyond MINIT and
TDUMP. **NOT DETERMINED FROM SOURCE:** the paper-tape bootstrap / BPUN loader that
would load MINIT or TSS on real hardware is not in this repo (nd100x supplies the
BPUN load path).

---

## 7. End-to-end bring-up runbook (cold → login)

Each step tagged **VERIFIED-working** (proven in this repo) or **OPEN** (not yet
run/proven). Cross-reference `docs/DISK-INIT-USERS-AND-BOOT.md` §2–4 for the
switch table and login authentication.

### Step 1 — Assemble TSS and MINIT with the fixed mac-c assembler
**VERIFIED-working.**
- TSS: `mac-c/build_tss_assysa.sh` → `Build/` (679/693 & 675/689 golden matches,
  0 assembly errors — see `CLAUDE.md` "current state").
- MINIT: `mac-c/build_minit.sh` → `Build/minit/minit.bpun` + `minit.img`, base
  `000000`, entry `MINIT`, 0 errors (build script; N10 mark selects NORD-10 IOX
  I/O). The `9377` octal-parse bug that zeroed console chars is fixed
  (memory `tss-login-char-path`; `HANDOFF-LOGIN-BRINGUP.md` §1).

### Step 2 — Run MINIT to format the disc
**OPEN (the finish line).**
- Boot `Build/minit/minit.bpun` on nd100x with a CDC disc attached.
- Answer: FIRST NCR, LAST NCR (both `AND 77770`), then `I` (INITIALIZE).
- MINIT surface-tests the range, writes the MIB free-track bitmap to NCR 50₈
  (`src/MINIT.SYMB:170-171`), prints `FINISHED` (`MS6`).
- **Why required:** without a valid MIB, the kernel's `GTRK`
  (`src/TSS2.SYMB:2665`) finds no free track and `SINIT`'s `CRUSE` cannot
  allocate storage for user `SYSTEM`. (See `DISK-INIT-USERS-AND-BOOT.md` §2.)

### Step 3 — Cold-boot with `OPR = 131313` → SINIT creates SYSTEM
**VERIFIED (code path); OPEN (live run, depends on Step 2).**
- At cold start the swap/restart code reads the operator switch register and
  compares to `131313` **[VERIFIED]** `src/TSS1.SYMB:3180`:
  ```
  TRA OPR; SUB (131313; JAF *+3; LDA (SINIT; JMP S99
  ```
  If `OPR == 131313`, it dispatches `SINIT` as the first process; otherwise it
  checks `AVAIL` and falls to `LEV2` (normal) (`:3181-3182`).
- `SINIT` **[VERIFIED]** `src/TSS2.SYMB:858-862`:
  ```
  SINIT, LDX (SINQ; RCLR DD; JPL I (CRUSE; JMP *+1
         JMP I (LEV2
  SINQ,  #SY; #ST; #EM; 0; 0; 0; 0      % packed name "SYSTEM"
  ```
  It calls `CRUSE` (create user) with name `SYSTEM`, then jumps to `LEV2`
  (normal operation). Per memory `tss-login-disc-init`, `CRUSE`/`SINIT` writes
  the name into `USTBL`; `USRDK` holds only passwords (empty = passwordless).

### Step 4 — Normal boot → `@ENTER` → login `SYSTEM`
**OPEN (live), path VERIFIED.**
- Reboot without the `131313` switch value → `LEV2` normal path
  (`src/TSS1.SYMB:3182`).
- The console reaches the `@` command interpreter; `ENTER SYSTEM` authenticates
  against `USTBL`/`USRDK`. The login char path (WBUF/RBUF `AND` masking) is the
  one the `9377` fix repaired. See `DISK-INIT-USERS-AND-BOOT.md` §4 and
  `HANDOFF-LOGIN-BRINGUP.md` §2 for the authentication detail.

---

## 8. Live run on nd100x — VERIFIED 2026-07-23

MINIT was assembled with the fixed `mac-as` (`make minit` → `Build/minit/minit.bpun`,
entry `MINIT=000206`) and run on nd100x under the DAP debugger:

```
nd100x --boot=bpun --image=minit.bpun --cdc=cdc.img --debugger --port=1777
```

**Result — [VERIFIED] by live trace:**

1. The BPUN autostart landed the CPU exactly at `P = 000206` (MINIT entry) — the
   base-0 image and `-e MINIT` autostart work. (Closes former open item "boot/image placement".)
2. The console printed the full operator dialogue:
   ```
   MASS STORAGE INIT
   FIRST DISK ADDRESS (NCR): 0
   LAST DISK ADDRESS (NCR): 2000
   INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): I
   INITIALIZE
   FINISHED
   ```
   — so the standalone tool reaches and completes its dialogue against the
   emulated CDC disc at IOX 500. No `UNABLE TO WRITE MIB`/`I/O ERROR`. (Closes
   former open item "CDC disc device in nd100x": the nd100x `--cdc` device at
   `DCHN=500` answers MINIT's `DKTR`/`DWAIT` correctly.)
3. The formatted `cdc.img` was inspected: **exactly one non-zero sector**, at
   physical sector `0o150` (104 dec), containing **128 set bits (16 bytes of
   `0xFF`) then zeros**. FIRST=0, LAST=`2000₈`=1024, stepping `10₈`=8 per track ⇒
   `1024/8 = 128` tracks ⇒ **128 free-track bits**. Exact match.
4. This **empirically confirms the MIB bit polarity: set bit = good/FREE track**
   (§2.4) — 128 good tracks produced 128 set bits — and shows `DKADR` maps
   NCR `50₈` (the MIB home) to physical sector `0o150` on the emulated surface.

> Note: FIRST/LAST = `0`/`2000₈` here is a **functional test range**, not a
> claim about the production system-disc geometry (still open item #2 below).
> The nd100x CDC device treats LBA as a linear physical sector and grows the
> surface to fit, so any modest range formats cleanly.

---

## 9. Open questions

1. ~~CDC disc answering in nd100x~~ — **CLOSED** (§8: `--cdc` at 500 works).
2. **NCR range for a real TSS system-disc.** NOT DETERMINED FROM SOURCE: the
   production FIRST/LAST NCR values that match the TSS disc map (MIB@50, USRDK@1460,
   swap areas, `FSYS`) so the free-track pool doesn't overlap the system/overlay
   area. The functional test used `0..2000₈`; the real bring-up needs the range
   derived from the TSS CDC image layout, not invented.
3. **MIB → BITBL load linkage.** NOT DETERMINED FROM SOURCE: how the MIB written
   by MINIT at NCR 50₈ is read into the kernel's core `BITBL` that `GTRK`
   consumes (`src/TSS2.SYMB:2671`). The bit *polarity* is RESOLVED and confirmed
   both by source and by the live run (§8: **set = good/free**); only the
   on-disc→core *load path* remains to trace.
4. ~~Boot/image placement + `-e MINIT` autostart~~ — **CLOSED** (§8: P=000206).
5. **UPDATE is a stub.** VERIFIED (`:184`): only INITIALIZE and REGENERATE do real
   work; `U` prints `NOT IMPLEMENTED`. Any procedure must use `I` (or `R`).
6. **Format the actual TSS CDC image.** The remaining integration step: run MINIT
   (or `R`egenerate) against the *TSS* CDC disc image (the one carrying the
   overlays from `build_tss_drum.sh`), format only its free-track area, then
   cold-boot `--opr=131313` on that same disc so `SINIT`'s `GTRK` finds the tracks.
```
