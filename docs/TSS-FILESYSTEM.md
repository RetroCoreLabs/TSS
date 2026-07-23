# NORD TSS 3.0 — On-Disc File System and Disc Layout

> **Status / marking convention.** Every fact below is tagged.
> `[VERIFIED]` = read directly from the cited source line(s).
> `[ASSUMPTION]` = my interpretation of verified code, marked as such.
> `NOT DETERMINED FROM SOURCE` = I could not establish it from the files read.
> Citations are `src/FILE.SYMB:LINE` (or `archive/…`). Octal is the native
> radix of MAC source; values written `NNN` are octal unless said otherwise.
>
> Sources actually read for this document: `src/MINIT.SYMB` (full),
> `src/TSS1.SYMB`, `src/TSS2.SYMB`, `src/TSS3.SYMB`, `src/TSS4.SYMB`,
> `src/TSS5.SYMB`. Cross-references (not duplicated here):
> `docs/CDC-DISC-DEVICE.md`, `docs/OVERLAY-DISC-SPEC.md`,
> `docs/DISK-INIT-USERS-AND-BOOT.md`, `docs/TSS-Analysis.md`.

---

## 1. Physical disc addressing

### 1.1 Two address spaces: NCR and CDC

The code deals with **two** disc-address representations and converts between
them.

- **NCR address** — the linear/logical address TSS works in. All the
  file-system tables and disc-map constants below are NCR addresses.
- **CDC address** — the physical cylinder/track/sector address of the CDC
  9425 / 9427 drive, produced from the NCR address by `DKADR`.

`DKADR` (`NCR → CDC`) is defined only for the CDC build:
`src/MINIT.SYMB:526-586` and `src/TSS1.SYMB` (same routine, `)PCL DKADR`).
[VERIFIED] header `%CONVERT NCR DISK ADDRESS TO CDC DISK ADDRESS` at
`src/MINIT.SYMB:527`.

Geometry constant driving the conversion:

```
DKLIM=626    %NUMBER OF CYLINDERS (313 FOR CDC 9425 AND 626 FOR CDC 9427)
```
[VERIFIED] `src/MINIT.SYMB:534-535`. So the shipped build targets the
**CDC 9427** (626 cylinders); the 9425 (313) is the alternate. The legality
check rejects any NCR address whose derived cylinder ≥ `DKLIM`
(`SUB (DKLIM; JAP DKADF`) [VERIFIED] `src/MINIT.SYMB:571`.

`DKADR` internals worth recording:
- It divides the significant NCR field by **12 (decimal)** to get the CDC
  `[14,5]` field — comment `%DIVIDE BY 12 (DEC)` [VERIFIED]
  `src/MINIT.SYMB:543`. So there are **12 sectors per track** in the NCR→CDC
  mapping. [ASSUMPTION] "12 sectors/track" is the meaning of that divisor,
  based on the comment and the `MPY (14` sector-count step at
  `src/MINIT.SYMB:561` (14 octal = 12 decimal).
- The NCR address low field is masked with `AND (17777`
  [VERIFIED] `src/MINIT.SYMB:541`, and unit-select displacement `52600` is
  added when the drive-select side bit is set [VERIFIED]
  `src/MINIT.SYMB:565`.

### 1.2 Sector / block size

A "block" / disc transfer unit is **256 (400 octal) words**.
[VERIFIED] `DKOP` header `%PERFORM A 256 WORD DISK OPERATION`
`src/MINIT.SYMB:333-334`; the runtime CDC path loads word-count `400` into
`LWC` (`LDA (400; IOX LWC`) `src/TSS2.SYMB:569` and the file routines clear
whole 400-word blocks (`LDA (400; JPL I (SETB`) e.g. `src/TSS5.SYMB:139`,
`src/TSS4.SYMB:285`.

A **track** = the free-space allocation unit (one bit in the MIB). A track is
**8 sectors = 2000 (octal) = 1024 words**. [VERIFIED by arithmetic]: a user's
file/UIB sub-blocks are looped `0..7` (`AAA -10; JAN …`) — 8 sectors per track
— see `IUSER` `src/TSS5.SYMB:139-140`, `CRFIL` `src/TSS5.SYMB:286`, and the
MIB write of 8 sectors in `MINIT` (`SAA 7` extra blocks, below).
[ASSUMPTION] the phrase "track = 8 sectors" is inferred from those 0..7 loops
plus the disc-map constants that reserve areas in multiples of 200 octal
(128 words) and 10 octal (8 sectors); see §7.

### 1.3 The DMA/geometry registers (CDC)

Device register symbols (CDC, NORD-10 build): `LCA` load-core-address,
`LBA` load-block(disc)-address, `LMR` load-modus, `LWC` load-word-count,
`RST` read-status, `RCA/RSECT/RDC/SEEK`. Defined at
`src/MINIT.SYMB:295-304` (N10) and `:280-293` (NN10 / NORD-1). The disc
device number base is `DCHN` (`DCHN=500` for N10, `DCHN=100` for NORD-1)
[VERIFIED] `src/MINIT.SYMB:267-272`. Cross-ref `docs/CDC-DISC-DEVICE.md`.

`DKS` = **start-of-disc offset** added to every logical address in `TRSF`
(`ADD (DKS`) [VERIFIED] `src/TSS2.SYMB:2765`. Default `DKS=0`
[VERIFIED] `src/TSS1.SYMB:2591-2594`. `DKBAS` is an analogous base added on
the NCR path (`ADD I (DKBAS`) `src/TSS2.SYMB:545`, and `DKBAS, 0`
[VERIFIED] `src/TSS1.SYMB:186`.

---

## 2. The MIB (Master Information Block)

The MIB is the disc's control block. It lives at NCR disc address **`DKBIT`**
and is **8 sectors (2048 = 4000 octal words)** long.

- `DKBIT=0` for the NCR build, `DKBIT=50` for the CDC build.
  [VERIFIED] `src/TSS1.SYMB:2596` (`DKBIT=0  %FILE SYSTEM MIB`, under `"NCR`)
  and `src/TSS1.SYMB:2598-2599` (`DKBIT=50`, under `"CDC`).
- In `MINIT` the whole MIB image is one 4000-octal buffer:
  `MIB=*` / `BUFF=MIB+4000` [VERIFIED] `src/MINIT.SYMB:808-809`, and it is
  cleared in a `0..4000` loop `src/MINIT.SYMB:142-144`.

### 2.1 MIB internal layout (8 sectors at `DKBIT`)

| MIB sector | disc addr | contents | evidence |
|---|---|---|---|
| 0 | `DKBIT+0` | **free-track bit table** (bitmap) | RBITB/WBITB read/write `LDT (DKBIT` sector 0 — `src/TSS2.SYMB:2960,2980` |
| 1..7 | `DKBIT+1 … DKBIT+7` | **user table** (7 sectors, index 1..7) | RUTBL/WUTBL `LDA (DKBIT; ADD TREG` where `T` = index 1..7 — `src/TSS2.SYMB:3048,3070` |

MINIT writes the MIB as one 8-sector transfer: `LDX (MIB; SAT 50; SAA 7;
COPY DD SA; SAA 1; JPL I (DKTR` — `T=50` (=`DKBIT`, CDC), `D=7` extra blocks
→ 8 sectors total, `A=1` write. [VERIFIED] `src/MINIT.SYMB:170-171` (and the
regenerate path `src/MINIT.SYMB:213`).

**In-core copies.** The running system keeps two independently-locked
in-core sector images, each with a 3- or 4-word header:

```
        0  %LOCK
        0  %IN CORE FLAG
        0  %DEVICE ID
BITBL, BSS 400          % free-track bit sector (256 words)
        0  %LOCK
        0  %IN CORE FLAG
        0  %DEVICE ID
        0  %INDEX (FROM 1 TO 7)
USTBL, BSS 400          % user-table sector (256 words)
```
[VERIFIED] `src/TSS1.SYMB:4583-4598`. A third general buffer `BTEMP` (same
4-word header) is used for arbitrary sectors, UIBs and file blocks
[VERIFIED] `src/TSS1.SYMB:4601-4609`. `BTEM4=BTEMP-4`.

Header word meanings: `-4`=LOCK, `-3`=IN-CORE flag (which sector is cached),
`-2`=cached DEVICE ID, `-1`=cached DISK ADDRESS / user-table INDEX. Used e.g.
in `RBITB` (`BITBL-3/-2/-1`) `src/TSS2.SYMB:2955-2963`, `RUTBL`
(`USTBL-4/-3/-2/-1`) `src/TSS2.SYMB:3042-3051`, `RBLOC`
(`BTEMP-4/-3/-2/-1`) `src/TSS2.SYMB:2996-3007`.

### 2.2 Free-track bit table — layout and semantics

- One MIB sector 0 = **256 words × 16 bits = 4096 track-bits maximum**.
  The usable size is returned by `BSIZ`: for the default device id it returns
  **400 (256 words)**; the alternate returns `10`. [VERIFIED]
  `src/TSS2.SYMB:2936-2941`.
- Bit masks `1,2,4,…,100000` (16 masks) at `BITM` [VERIFIED]
  `src/TSS1.SYMB:4577-4578`; identical table `MASKS` in MINIT
  `src/MINIT.SYMB:224-225`.
- **A set bit = track free; a cleared bit = track allocated.** [VERIFIED by
  code]: `GTRK` searches for a word `≠0` then finds a set bit and, when it
  claims the track, does `COPY CM1 DT ST; RAND DT SA` — clears that bit and
  stores it back (`STT I CNT,B,X`) `src/TSS2.SYMB:2672-2679`. `RTRK`
  (release) sets the bit back. MINIT's `SBIT` with `A=1` **sets** a bit and
  `A=0` **clears** it (`LDA SBTAD+1; JAZ SB1` → OR-mask vs AND-NOT-mask)
  `src/MINIT.SYMB:256-259`; MINIT clears (marks used) system tracks by
  calling `SBIT` with `A=0` (`SAA 0; JPL I (SBIT`) e.g.
  `src/MINIT.SYMB:206`, and marks the free user area with `A=1`
  (`SAA 1; JPL I (SBIT`) `src/MINIT.SYMB:194`.

  > Note the polarity is the **reverse of an "in-use bitmap"**: here a **1**
  > means *available*. [VERIFIED] from the two call sites above.

- **Track number → bit position** (in `SBIT`): `SHT ZIN SHR 3` gives the
  word index (track ÷ 8… i.e. ÷ 16-bit-word by shifting 3? see below),
  `SAD ZIN SHR 4` isolates the bit-in-word, mask from `MASKS`, word address
  `= MIB + wordindex`. [VERIFIED] `src/MINIT.SYMB:252-259`.
  [ASSUMPTION] the exact shift accounting (a disc *address* is a track times
  8 sectors, then ÷16 to word / mod 16 to bit) — the two shifts `SHR 3`
  (÷8, address→track) and `SHR 4` (÷16, track→word/bit) are consistent with
  "address = track×8" and "16 tracks per bit-word", but I did not find a
  single comment stating it outright, so I mark the derivation as
  interpretation.
- In `GTRK` the reverse maps a found bit back to a disc address:
  `SAA 20; MPY CNT,B; ADD CNTX,B; SHA 3` (word×16 + bitindex, then ×8)
  [VERIFIED] `src/TSS2.SYMB:2678-2679`.

---

## 3. Track allocation

### 3.1 `GTRK` — acquire a free track

`GTRK` (`src/TSS2.SYMB:2655-2682`) [VERIFIED]:
- Header: `%ACQUIRE A FREE TRACK FROM THE DISK`, `A`=device id, returns
  `A`=disc address of track, failure `A=1` (no track available)
  `src/TSS2.SYMB:2656-2661`.
- `LOCK`s the file system, calls `BSIZ` for the table size, reads the bit
  sector via `RBITB` (`src/TSS2.SYMB:2669-2670`).
- Scans words `CNT` high→low for a non-zero word (`JAZ G4`), then scans the
  16 bit positions (`CNTX`) for a set bit `src/TSS2.SYMB:2671-2674`.
- On hit (`G5`): clears the bit, writes the sector back with `WBITB`, returns
  the computed disc address in `T`/`A` `src/TSS2.SYMB:2678-2680`.
- If none found: `SAA 1` failure `src/TSS2.SYMB:2675-2676`.

### 3.2 `RTRK` — release a track

`RTRK` (`src/TSS2.SYMB:2685-2714`) [VERIFIED]: `A`=device id, `T`=disc
address; sets the corresponding bit (frees). Range-checks address against
`BSIZ` (`SKP IF DA LST ST; JMP RF2`, failure `A=2` out of range) and detects
double-free (`A=1` already free, `RF1`). `src/TSS2.SYMB:2703-2711`.

### 3.3 `SBIT` (MINIT only) — set/reset a bit

`SBIT` (`src/MINIT.SYMB:246-265`) [VERIFIED] is the format-time equivalent:
`T`=disc address, `A`=0 (reset/mark used) or 1 (set/mark free).

### 3.4 Per-user track accounting

`UTRK` (`src/TSS2.SYMB:3102-3124`) [VERIFIED] charges/decrements a user's
remaining track quota. `A`=tracks to charge, `T`=user number; the quota word
lives in the user's **UIB header at `BTEMP+2`** (`LDA I (BTEMP+2; SUB AREG` …
`STA I (BTEMP+2`) `src/TSS2.SYMB:3117-3119`. Failure `A=1` no tracks left,
`A=4` no such user. `CRFIL` calls `UTRK` before allocating with `GTRK`
`src/TSS5.SYMB:259,267`.

---

## 4. The user / directory tables on disc

There are **two** distinct on-disc user tables. Do not conflate them.

### 4.1 `USTBL` — user name → UIB, inside the MIB (`DKBIT+1..7`)

- Read/written by `RUTBL` / `WUTBL`, one MIB sector per **index 1..7**,
  disc address `DKBIT+index` [VERIFIED] `src/TSS2.SYMB:3040-3074`.
- **Entry size = 8 (10 octal) words**; 32 entries per 256-word sector
  [VERIFIED by code]: index into sector is `SHA 3` (×8) from the entry number
  in `GETUN`/`USARR`/`UNDK` (`LDA CNTX,B; SHA 3; ADD (USTBL`)
  `src/TSS4.SYMB` login path, `src/TSS2.SYMB:3094`, `src/TSS3.SYMB:660`,
  `src/TSS5.SYMB:165`; the per-sector loop bound is `AAA -40` (32 entries)
  `src/TSS5.SYMB:169`, and the sector index is `(user-1) ÷ 32` (`SAD SHR 5`)
  `+1` `src/TSS5.SYMB` (`GETUN` G1) and `src/TSS3.SYMB:657-658` (`USARR`).
- **Entry field offsets** [VERIFIED]:
  - offset **0..6** — user **name**, 7 words (compared 7-wide by
    `SAA 7; JPL I (BEQL` in `GETUN` `src/TSS5.SYMB:166`; copied 7-wide in
    `USARR` `src/TSS3.SYMB:661`).
  - offset **7** — **UIB disc address** (the user's index block). `UNDK`
    returns `LDA 7,X` as the disc address `src/TSS2.SYMB:3094-3095`; MINIT
    reads it as `LDA 7,X; STA UIB` `src/MINIT.SYMB:200`.
- Max user index checked in `USARR` is **101 octal (65 dec)** → up to 64
  users → 2 of the 7 sectors normally in use. [VERIFIED]
  `AAA -101; JAP UFF` `src/TSS3.SYMB:656`.
- `GETUN` (name→user number, `src/TSS5.SYMB:146-174`) and `UNDK`
  (user number→UIB disc address, `src/TSS2.SYMB:3077-3099`) are the two
  lookups over `USTBL`. [VERIFIED]

### 4.2 `USRDK` — password / login table (separate, outside the MIB)

- Located at NCR address **`USRDK`** (see §7; = **1460 octal** in the CDC
  build). [VERIFIED] `USRDK=MAILD+MAILS+400` `src/TSS1.SYMB:2610`.
- Read/written with `XDISK` into core buffer `USRTB`, e.g. login
  `LDX (USRTB; LDT (USRDK; SAA 1; JPL I (XDISK` (read),
  `SAA 3` (write) [VERIFIED] `src/TSS4.SYMB:557,633,688,697`,
  `src/TSS4.SYMB:620` (PASWD).
- **Entry size = 8 (10 octal) words; 32 per 256-word sector.** [VERIFIED by
  code]: login computes record = `((uno-1) mod 32) × 8` — `AAA -1;
  RCLR DD; SAD SHR 5; … SAA 0; SAD 10; ADD (USRTB` (the `SAD SHR 5` picks
  the sector `(uno-1)/32`, `SAD 10`=×8 the low bits) `src/TSS4.SYMB:559-561`;
  identical in `PASWD` `src/TSS4.SYMB:621-623` and `CPASW`
  `src/TSS4.SYMB:693-695`.
- **Offset 7 = the user's password.** [VERIFIED]:
  - login checks it: `SAX 7; LDA I OBJ,B,X` then compares against the typed
    password `src/TSS4.SYMB:562,569`.
  - `PASWD` stores the new password there: `LDA PASSW,B; SAX 7;
    STA I OBJ,B,X` `src/TSS5.SYMB`… actually `src/TSS4.SYMB:632`.
  - `CPASW` (clear password) zeroes it: `STZ 7,X` `src/TSS4.SYMB:696`.
  - **An empty (zero) password = passwordless login**: login jumps straight
    to `LOGS` when `LDA I OBJ,B,X` (offset 7) is zero (`JAZ LOGS`)
    `src/TSS4.SYMB:562`. There is also a master/override compare against
    `636` (`SUB (636; JAZ L6A`) `src/TSS4.SYMB:568`.
    [ASSUMPTION] `636` being a built-in override password is my reading of
    that unconditional accept branch; the source has no comment naming it.

  > Password is a single word built by packing typed characters 3 bits at a
  > time (`SHT 3; RADD DA ST`) `src/TSS4.SYMB:566` — i.e. it is a hash/pack
  > of the characters into one 16-bit word, not stored as text. [VERIFIED].

### 4.3 `MAILD` — the mailbox

- At NCR address **`MAILD`**, size **`MAILS=200` octal words**. [VERIFIED]
  `MAILD=ACCTD+200`, `MAILS=200` `src/TSS1.SYMB:2606-2607`.
- Accessed by the mail routines in TSS4 with `XDISK`, addressing
  `(MAILD + CNT)` per mailbox slot, e.g.
  `LDA (MAILD; ADD CNT,B; COPY DT SA; LDX (QBUF` `src/TSS4.SYMB:279,284,291`
  and the 3-sector transfer at `src/TSS4.SYMB:256`
  (`LDT (MAILD; SAA 3; JPL I (XDISK`). [VERIFIED]
- Mailbox slot layout / per-user indexing: **NOT DETERMINED FROM SOURCE** in
  detail (I did not trace the `CNT` slot arithmetic to a documented record
  format).

### 4.4 `ACCTD` — account directory

- At NCR address **`ACCTD`**. [VERIFIED] `ACCTD=CORL2+200`
  `src/TSS1.SYMB:2603`. Initialized/cleared by `IACCT`
  (`LDX (QBUF; LDT (ACCTD; SAA 3; JPL I (XDISK`) `src/TSS4.SYMB:718`.
  Record layout: **NOT DETERMINED FROM SOURCE**.

---

## 5. Files: user index blocks (UIB) and file descriptors

### 5.1 The UIB (one track per user, first sub-block is the header)

`IUSER`/`FIUSE` initializes a user's index block
(`src/TSS5.SYMB:117-143`) [VERIFIED]:
- `A`=device id, `X`=number of tracks, `T`=user number.
- Finds the user's UIB disc address via `UNDK` (`STA DKAD`).
- Writes the UIB **header** (first sector of the track):
  ```
  STZ 0,X          % word 0 = 0
  LDA (731; STA 1,X % word 1 = 731  (UIB signature/magic)
  LDA XREG,B; STA 2,X % word 2 = number of tracks (the quota, see UTRK BTEMP+2)
  ```
  [VERIFIED] `src/TSS5.SYMB:134-135`. The `731` constant is the UIB
  identifier. [ASSUMPTION] "signature/magic" is my label; source has no
  comment. The quota word at UIB offset 2 is the same one `UTRK` decrements
  as `BTEMP+2` when the UIB header is in core `src/TSS2.SYMB:3118`.
- Then loops the remaining 7 sub-blocks of the track (`CNT 1..7`,
  `AAA -10; JAN I2`) clearing/`SETB`-ing each and writing it
  `src/TSS5.SYMB:136-140`. So **a UIB occupies one whole track = 8 sectors**,
  of which sector 0 is the header and sectors 1..7 hold file-descriptor
  entries.

### 5.2 File-descriptor entries inside the UIB

`SUIB`/`FSUIB` (`search UIB for a given file`, `src/TSS5.SYMB:182-228`)
[VERIFIED]:
- Reads the UIB header sector (`RBLOC`), keeps its access word from
  `BTEMP+1` (`STA ACCWD`) `src/TSS5.SYMB:210`.
- Then scans the 7 data sub-blocks (`CNT 1..7`, `AAA -10`), and within each
  sector scans entries `CNTX 0..7` with stride **`SHA 5` (×32, = 5-word…**
  wait: `SHA 5` = ×32 words) — **entry size = 32 (40 octal) words**, 8 per
  256-word sector. [VERIFIED] `LDA CNTX,B; SHA 5; ADD (BTEMP`
  `src/TSS5.SYMB:213`, loop bound `AAA -10` (8 entries)
  `src/TSS5.SYMB:220`.
- Entry offset **1** = valid/occupied flag (`LDA 1,X; JAF U4` — free if
  ≤0) `src/TSS5.SYMB:214`.
- The **file id / name key is compared 12 (octal) = 10 words** starting at
  entry offset 1: `AAX 1; LDT XREG,B; AAT 1; SAA 12; JPL I (BEQL`
  `src/TSS5.SYMB:216-217`. [VERIFIED]

`CRFIL`/`FCRFI` (create file, `src/TSS5.SYMB:231-294`) writes a new
descriptor and shows the descriptor field offsets [VERIFIED]:
- `X` points at a file-id array `(user no., name, edition, type, password,
  object type, logical block size, date)` — header comment
  `src/TSS5.SYMB:234-236`.
- It charges a track (`UTRK`), checks access (`CKACC`), searches
  (`SUIB` → free slot or "already exists" `CF6`), allocates the file's first
  data track with `GTRK` (`STA DKA2`) `src/TSS5.SYMB:267`, then fills the
  descriptor. Observed field stores into the new object (`OBJ`) with `SAX n`
  offsets [VERIFIED] `src/TSS5.SYMB:271-281`:
  - copies 16 (octal) words of the file id in at offset 1 (`AAT 1;
    … SAA 16; JPL I (BCOPY`) `src/TSS5.SYMB:271-272`;
  - offset **17** = access word `OR`ed with the UIB access
    (`SHA 11; ORA ACCWD; SAX 17`) `src/TSS5.SYMB:273`;
  - offset **20** = (device-id/object) word `src/TSS5.SYMB:274`;
  - offset **21** = **first data track disc address** (`LDA DKA2; SAX 21`)
    `src/TSS5.SYMB:277`. This matches MINIT's regenerate scan, which reads a
    descriptor's first-track pointer at **offset 21** (`LDA 21,X; STA IX`)
    `src/MINIT.SYMB:205` and then walks the track chain.
  - offsets **22, 24, 25, 30, 32, 33** = date / block-size / object-type
    words (`SAX 22 STD`, `SAX 24`, `SAX 25 STF`, `SAX 30 STD`, `SAX 32`,
    `SAX 33 STZ`) `src/TSS5.SYMB:275-281`. Exact semantics of each of these
    six: **NOT DETERMINED FROM SOURCE** (I did not fully decode every field).

### 5.3 Reading / writing user data — `ROBJ`, `RUTBL`, `TRSF`

- `TRSF` is the low-level "read/write one in-core sector to disc"
  primitive: `D`=device, `A`=1 read / 3 write, `T`=disc addr (`+DKS`),
  `X`=core addr; forwards to `TDISK`→`XDISK`. [VERIFIED]
  `src/TSS2.SYMB:2755-2768`.
- `XDISK` is the retrying transfer that talks to the hardware (NCR and CDC
  variants), including the `NCR→CDC` `DKADR` call on the CDC path
  (`JPL I (DKADR; JMP XDF1`) `src/TSS2.SYMB:556,568`. [VERIFIED]
  `src/TSS2.SYMB:532-598`.
- `RBLOC`/`WBLOK` read/write an arbitrary sector via the `BTEMP` buffer
  `src/TSS2.SYMB:2987-3028`. `RUTBL`/`WUTBL` do the same for the `USTBL`
  user-table sector; `RBITB`/`WBITB` for the `BITBL` bit sector. [VERIFIED]
- `ROBJ` (read object, `src/TSS2.SYMB:3127-3166`) is the file-read entry: it
  resolves the user's UIB via `UNDK`, indexes to the object's descriptor
  (`AND (7; SHA 5; ADD (BTEMP` — same 32-word stride), reads the access word
  at descriptor offset **17** (`LDA 17,X; AND (777`) and checks it with
  `CKACC` before copying data. [VERIFIED] `src/TSS2.SYMB:3149-3164`.
- `CKACC` (`src/TSS2.SYMB:3169-…`) validates a user's access rights against
  the object/UIB access word; it also reads a cached UIB into `USTBL` via
  `TRSF` when needed (`UIBTB` cache) `src/TSS2.SYMB:3192-3199`. Access-bit
  meanings (the "OWR" 3 bits named in the header `src/TSS2.SYMB:3174`):
  owner/write/read — [ASSUMPTION] from the comment `%TYPE OF ACCESS WANTED
  (3 BITS, I.E. OWR)`.

### 5.4 `FINIT` — file-system core-structure init (not on-disc)

`FINIT` (`src/TSS1.SYMB:4711-4726`) initializes the **in-core** headers
(`BITBL-3`, `USTBL-4`, `BTEMP-4`) and the page/object tables, not disc
content. [VERIFIED]. On-disc formatting is `MINIT`'s job (§6).

---

## 6. Disc initialization — `MINIT` (the formatter)

`MINIT` (`src/MINIT.SYMB:134-243`) is the standalone CDC-disc formatter.
[VERIFIED]. Operator dialogue:
`FIRST DISK ADDRESS (NCR)` / `LAST DISK ADDRESS (NCR)` /
`INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R)`
`src/MINIT.SYMB:228-234`.

- **Address entry** is masked `AND (77770` (rounds to an 8-sector track
  boundary) `src/MINIT.SYMB:139-141` → confirms **track = 8 sectors** and
  addresses are sector-granular with the low 3 bits = sector-within-track.
- **INITIALIZE** (`MIT`, `src/MINIT.SYMB:149-171`): clears a 4000-word
  buffer, writes it out track-by-track across `ADR1..ADRN`, and for each
  written track **sets its free bit in the MIB** (`LDA ADRI; SAD SHR 7;
  ADD (MIB` → word index; `OR` in the mask) `src/MINIT.SYMB:165-167`;
  advances `ADRI` by `10` (8 sectors = one track) each iteration
  `src/MINIT.SYMB:168`. Finally writes the whole 8-sector MIB to `DKBIT`
  (`SAT 50`) `src/MINIT.SYMB:170-171`.
- **REGENERATE** (`REG`, `src/MINIT.SYMB:192-221`): rebuilds the MIB from the
  live directory. It walks all users (`USN 0..340` = 224, `SUB (340`)
  `src/MINIT.SYMB:212`, reads each `USTBL` block at **`DKBIT+1`**
  (`AAA 51`, =50+1) `src/MINIT.SYMB:197`, reads each user's UIB (offset 7 →
  `UIB`) `src/MINIT.SYMB:200-201`, walks the UIB's file descriptors
  (offset 21 → first track `IX`) `src/MINIT.SYMB:205`, and for every file
  track marks the bit **used** (`SBIT` with `A=0`) — user table blocks,
  UIB tracks, and each data track in the file's track chain
  `src/MINIT.SYMB:206-210`. Then writes the MIB back
  `src/MINIT.SYMB:213`. This is the authoritative on-disc structure walk:
  **USTBL(name,+7=UIB) → UIB(+21=first track) → track chain**.
- **UPDATE** is `%NOT IMPLEMENTED` (`MUP` → error message)
  `src/MINIT.SYMB:184,243`.

> `MINIT` proves the free-track bitmap is derivable purely from the directory
> (REGENERATE), i.e. the MIB is a **cache/index**, and the directory
> (USTBL + UIBs) is the source of truth for allocation.

---

## 7. Overall disc map (constants from `src/TSS1.SYMB:2589-2623`)

All symbols are NCR disc addresses, defined as a running sum
[VERIFIED] `src/TSS1.SYMB:2589-2623`:

```
DKS      = 0                 % start of disk (base offset added in TRSF)
DKBIT    = 0 (NCR) / 50 (CDC)% MIB (8 sectors)
CORL1    = DKBIT+DKS+10      % coreload 1
CORL2    = CORL1+200         % coreload 2
OVDK1    = CORL1+100         % overlay area 1
OVDK2    = CORL2+100         % overlay area 2
ACCTD    = CORL2+200         % account directory
DKLP     = ACCTD-20          % line-printer spool buffer
DKFP     = ACCTD-10          % fast-punch spool buffer
MAILD    = ACCTD+200         % mailbox (size MAILS=200)
USRDK    = MAILD+MAILS+400   % user password table
DKA1..DKA12 = USRDK+10, +200 each  % 12 swap areas (200 words each)
FSYS     = DKA12+200         % start of file-system storage (user data area)
```

### 7.1 Resolved octal addresses — **CDC build** (`DKBIT=50`, `DKS=0`)

| disc addr (octal) | length | contents | written by | source |
|---|---|---|---|---|
| `50` | 8 sect (`4000` w) | **MIB**: sector0 bit table, sect1–7 user table | `MINIT` (format), `WBITB`/`WUTBL` (runtime) | `TSS1.SYMB:2599`; `MINIT.SYMB:170`; `TSS2.SYMB:2980,3070` |
| `60` | `200` | CORL1 — coreload 1 | system loader | `TSS1.SYMB:2601` |
| `160` | `100` | OVDK1 — overlay area 1 | overlay loader | `TSS1.SYMB:2608`; overlays in `TSS3.SYMB:38` |
| `260` | `200` | CORL2 — coreload 2 | system loader | `TSS1.SYMB:2602` |
| `360` | `100` | OVDK2 — overlay area 2 | overlay loader | `TSS1.SYMB:2609,2630` |
| `440` | `10` | DKLP — line-printer spool buffer | print spooler | `TSS1.SYMB:2604` |
| `450` | `10` | DKFP — fast-punch spool buffer | punch spooler | `TSS1.SYMB:2605` |
| `460` | `200` | ACCTD — account directory | `IACCT` | `TSS1.SYMB:2603`; `TSS4.SYMB:718` |
| `660` | `200` (MAILS) | MAILD — mailbox | mail routines | `TSS1.SYMB:2606`; `TSS4.SYMB:256,279` |
| `1460` | `10` | **USRDK** — user password table | login/`PASWD`/`CPASW` | `TSS1.SYMB:2610`; `TSS4.SYMB:557,633` |
| `1470` | `200` | DKA1 — swap area 1 | swapper | `TSS1.SYMB:2611` |
| … | `200` ea | DKA2..DKA12 — swap areas 2–12 | swapper | `TSS1.SYMB:2612-2622` |
| `4470` | — | FSYS — start of file-system (user data) storage | `GTRK`/`CRFIL`/`IUSER` | `TSS1.SYMB:2623` |

> Arithmetic (octal) for the CDC column, all from `TSS1.SYMB:2589-2623`:
> CORL1=50+10=60; OVDK1=60+100=160; CORL2=60+200=260; OVDK2=260+100=360;
> ACCTD=260+200=460; DKLP=460−20=440; DKFP=460−10=450; MAILD=460+200=660;
> USRDK=660+200+400=1460; DKA1=1460+10=1470; DKA12=1470+(11×200)=4270;
> FSYS=4270+200=4470. [VERIFIED by arithmetic on the cited definitions.]

> **NCR build** (`DKBIT=0`): the MIB sits at address 0 and every constant
> shifts down by 50; e.g. CORL1=`DKS+10`. The relative layout is identical.
> [VERIFIED] `src/TSS1.SYMB:2596`.

### 7.2 Overlays on disc (OVDK)

The overlay subsystem loads overlay images from `OVDK` (=`OVDK1` normally,
`OVDK2` under `"DEBUG`) `src/TSS1.SYMB:2626-2633`, addressed as
`OVDK + (overlay# × 2)` — `LDA I (OVLAY; SHA 1; ADD (OVDK`
`src/TSS3.SYMB:38`, `src/TSS1.SYMB:3086`. Full overlay-on-disc detail is in
`docs/OVERLAY-DISC-SPEC.md` (cross-ref, not duplicated).

---

## 8. Summary of record layouts (offsets, all [VERIFIED] unless noted)

| structure | disc location | entry size | key offsets |
|---|---|---|---|
| Free-track bit table | MIB sector 0 (`DKBIT`) | 256 words / 4096 bits | bit=1 free, bit=0 used |
| User table `USTBL` | MIB sect 1–7 (`DKBIT+idx`) | 8 words, 32/sector | 0–6 name (7 w); 7 = UIB disc addr |
| Password table `USRDK` | `USRDK` (1460 CDC) | 8 words, 32/sector | 7 = password word (0 = none) |
| UIB header | sector 0 of user's UIB track | — | 0=0; 1=`731` id; 2=track quota |
| File descriptor | UIB sect 1–7 | 32 words, 8/sector | 1=valid flag; 1..(1+12)=file id key; 17=access; 20=obj/dev; 21=first data track; 22/24/25/30/32/33 = date/size/type [semantics partly undetermined] |
| Mailbox | `MAILD` (660 CDC), 200 w | — | slot layout NOT DETERMINED |
| Account directory | `ACCTD` (460 CDC) | — | layout NOT DETERMINED |

---

## 9. Open questions / not determined from source

1. **File-descriptor fields 22/24/25/30/32/33** — I decoded offsets 17
   (access), 20 (obj/dev), 21 (first data track), but did not fully decode
   the date / logical-block-size / object-type / edition words. (`CRFIL`
   `src/TSS5.SYMB:271-281`.)
2. **Track chaining beyond the first track.** MINIT's REGENERATE walks a
   file's tracks (`REG5` loop over a track list in the file's blocks
   `src/MINIT.SYMB:207-210`), implying multi-track files store their track
   list somewhere in the file's own blocks, but I did not pin the exact
   in-block format of that list. NOT DETERMINED FROM SOURCE.
3. **Mailbox and account-directory record formats** — addresses verified,
   internal layout not traced.
4. **`636` login override** — treated as a possible master password; not
   confirmed by any comment (`src/TSS4.SYMB:568`). Marked [ASSUMPTION].
5. **Exact bit-position shift accounting in `SBIT`** (`SHR 3` then `SHR 4`)
   is consistent with "address = track×8, 16 tracks/word" but not stated in a
   comment; marked [ASSUMPTION] (§2.2).
6. **`BSIZ` alternate return `10`** vs default `400` — the meaning of the
   second device class (`src/TSS2.SYMB:2939`) is not documented in the
   sources read.
