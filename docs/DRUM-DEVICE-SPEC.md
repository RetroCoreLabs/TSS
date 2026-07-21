# NORD TSS Swapping-Drum — Hardware Protocol Spec (as the TSS driver programs it)

Target: an emulator writer who must build a drum device that the TSS `XDRUM`
driver drives correctly. Every claim is tagged **[VERIFIED]** (with file+line and a
quoted instruction/constant) or **[INFERRED]** (reasoned from the code but not
literally stated). Anything not derivable from the source is in the
**UNKNOWN / COULD NOT DETERMINE** section — nothing there is invented.

Primary source (all quotes are from this file unless noted):
`E:\Dev\Ronny\TSS\src\TSS1.SYMB`, the `XDRUM`/`TRSFR` driver, lines **3695–3874**
(guards `"CDC-DRUM` / `"DRUM N10`). A verbatim extraction with identical text is
`E:\Dev\Ronny\TSS\derived\DRUM-DRIVER.SYMB`. Build variant that compiles it in:
`E:\Dev\Ronny\TSS\derived\ASSYS-DRUM-N10-MAC-INPUT.SYMB` (marks `DRUM N10`).

IOX semantics cross-checked against
`E:\Dev\Ronny\nd100-markdown\docs\cpu_documentation.md` (IOX section, ~line 3548)
and bit-instruction encoding against
`E:\Dev\Ronny\nd100-markdown\nd100-definitions\specs\category\bit_instructions.md`.

The driver's own header calls itself a *"1. APPROXIMATION TO A NORD-10 VERSION /
NJL 17/4/73"* (TSS1.SYMB:3732–3733) **[VERIFIED]** — this is prototype code, so
some fields are set up but never exercised.

---

## 0. IOX mechanics an emulator must implement

**[VERIFIED]** `cpu_documentation.md` IOX section (line 3554–3577):
- `IOX <devreg>` opcode base is **`164000`₈**; the assembled word is
  `164000 + devreg`. Mask `1111_1000_0000_0000`, so the device-register address
  is the low 11 bits (0–2047).
- **Transfer direction is the least-significant bit of the device-register
  address**: LSB `0` = *input* (device → A register), LSB `1` = *output*
  (A register → device).
- The exchange is between the **A register** and the addressed device register.

**[VERIFIED]** Bit-test encoding (`bit_instructions.md`): `BSKP ZRO/ONE <bn> <dr>`
skips the next word if bit `bn` of register `dr` is 0 / 1. The operand is the raw
octal field added to the opcode; the **bit number is the field value divided by 8**
(bits 3–6 hold the bit number, bits 0–2 the register). This is confirmed by the
STS-mnemonic table in that file: `SSK`=`20`₈→bit 2, `SSQ`=`40`₈→bit 4. So an
operand of `020`₈ tests **bit 2**, `040`₈ tests **bit 4**.

---

## 1. Device-register (IOX) numbers

**[VERIFIED]** TSS1.SYMB:3763–3769 defines the displacements off the base:

```
DRM=	540	%DRUM MAIN DEVICE NUMBER
RCX=	0	%READ CORE ADDRESS
LCX=	1	%LOAD CORE ADDRESS
LBX=	3	%LOAD BLOCK ADDRESS
RSX=	4	%READ STATUS REGISTER
LCR=	5	%LOAD CONTROL REGISTER
LWX=	7	%LOAD WORD COUNT
```

All accesses are `IOX DRM <disp>` = `IOX (540+disp)`. Base device number **540₈**.

| Octal dev | disp | Mnemonic | LSB dir | Register / operation | Used by XDRUM? | Evidence |
|---|---|---|---|---|---|---|
| **540** | 0 | RCX | 0 = input  | Read core address | **No** (defined only) | :3764; not issued anywhere in XDRUM |
| **541** | 1 | LCX | 1 = output | Load core (memory) address | Yes | `IOX DRM LCX` :3806 |
| 542 | 2 | — | 0 = input | (undefined / unused) | No | not referenced |
| **543** | 3 | LBX | 1 = output | Load block (drum) address | Yes | `IOX DRM LBX` :3815 |
| **544** | 4 | RSX | 0 = input  | Read status register | Yes | `IOX DRM RSX` :3783, :3846 |
| **545** | 5 | LCR | 1 = output | Load control register | Yes | `IOX DRM LCR` :3838; also `IOX 545` :3860 |
| 546 | 6 | — | 0 = input | (undefined / unused) | No | not referenced |
| **547** | 7 | LWX | 1 = output | Load word counter | Yes | `IOX DRM LWX` :3803 |

Assembled opcodes an emulator must decode (base `164000`₈):
`IOX 540`=`164540`, `541`=`164541`, `543`=`164543`, `544`=`164544`,
`545`=`164545`, `547`=`164547`. **[VERIFIED]** by the IOX base `164000` + device
number; the driver source shows the device numbers, `cpu_documentation.md` shows
the base.

Note **[VERIFIED]** TSS1.SYMB:3778 comment: *"HDEV,B MUST HOLD THE HARDWARE DEV.
NUMBER (THE RCX DEVICE NUMBER)"* — but `HDEV` is **not defined** anywhere
(`grep` finds only the comment). The code hard-codes `DRM=540`; the HDEV
indirection was never implemented. An emulator should respond at the fixed
`540`–`547` block.

---

## 2. Register bit layouts

### 2.1 Status register — read via `IOX 544` (RSX), value arrives in A

**[VERIFIED]** the driver defines and tests exactly two status bits
(TSS1.SYMB:3773–3774, 3784, 3786):

```
DVA=	020	%DEVICE ACTIVE        -> bit 2 (020/8)
ERR=	040	%INCLUSIVE OR OF ERRORS -> bit 4 (040/8)
```
```
DRDO1, IOX	DRM RSX	%READ STATUS REGISTER
	BSKP	ZRO DVA DA   ; skip if status bit 2 == 0
	JMP	DBUSY	         ;   bit2==1 -> drum still active/busy
	BSKP	ZRO ERR DA   ; skip if status bit 4 == 0
	JMP	DERRO	         ;   bit4==1 -> error
```

| Status bit | Symbol | Meaning | Emulator behaviour | Evidence |
|---|---|---|---|---|
| **2** (`020`₈) | DVA | **DEVICE ACTIVE** — 1 while a transfer is in progress | set 1 when activated, clear 0 on completion | :3773, tested :3784 |
| **4** (`040`₈) | ERR | **Inclusive OR of all error conditions** — 1 on any error | set on any error condition | :3774, tested :3786 |

- On the **error** path the *entire status word still in A* is handed back to the
  OS in X: `DERRO, COPY SA DX` (:3858). **[VERIFIED]** header (:3754):
  *"ERROR EXIT: X-DRUM STATUS REGISTER"*. So other status bits are propagated to
  the OS but the driver itself only branches on bit 2 and bit 4.
- On the **finished** path the status is re-read and returned in X:
  `DRDS1, IOX DRM RSX; COPY SA DX` (:3846–3847). **[VERIFIED]** header (:3757):
  *"FINISHED EXIT: X-DRUM STATUS REGISTER, A-CORE ADDRESS"*.

**[INFERRED]** For a minimal correct emulation only bits 2 and 4 matter to the
driver's control flow: return DVA=1 while busy, DVA=0 when done, ERR=0 on success.
The exact meaning of the other status bits is **UNKNOWN** (see §8).

### 2.2 Control register — written via `IOX 545` (LCR), value taken from A

**[VERIFIED]** built in A at TSS1.SYMB:3828–3838:

```
	SAA	3
	AND	DTREG	          ; A = (input function code) AND 3   (low 2 bits)
	SHA	ZIN 13	          ; A <<= 13  -> function in bits 13,14
	COPY	SA DT	          ; T = function<<13
	LDA	(1400	          ; A = 1400 (bits 8,9 mask)
	AND	DTREG	          ; A = DTREG AND 1400 (core-addr bits 16,17, held in T[8,9])
	SHA	ZIN SHR 3	  ; A >>= 3 -> those two bits now at bits 5,6
	RORA	ST DA	          ; A = A OR T  (merge addr-bits with function)
	AAA	7	          ; A |= 7  "ENABLE INTERRUP AND ACTIVATE DEVICE"
DRDCO, IOX	DRM LCR	  ; write control register
```

Control-word layout as the driver assembles it:

| Bits | Field | Value / source | Evidence |
|---|---|---|---|
| **0–2** | Activate + interrupt enable | always **`7`** (all three set) | `AAA 7` :3836, comment :3836 |
| 3–4 | (unused / 0) | 0 | — |
| **5–6** | Core address bits **16 and 17** (extended memory address) | `(DTREG & 1400) >> 3` | :3832–3834, comment :3834 |
| 7–12 | (unused / 0) | 0 | — |
| **13–14** | **Device operation / function** (2 bits) | `(DTREG & 3) << 13` | :3828–3830, comment :3830 "DEVICE OPERATION" |
| 15 | (unused / 0) | 0 | — |

Function values placed in bits 13–14 (from the input `T` register, §4): `0`=read,
`1`=write, `2`=read-test, `3`=compare. **[VERIFIED]** header :3744–3747.

**[INFERRED]** Bits 0–2 = `7` means: activate the transfer AND enable the
completion interrupt in one write. The driver never writes an activate-only or
interrupt-disabled control word during normal operation, so an emulator may treat
"control word with bit 0/1/2 set" as "start transfer, interrupt on completion".
The split of the three bits (which one is 'activate' vs which is 'interrupt
enable') is **not distinguished by the source** — they are always written together
as `7`.

**Error-time control write** — `DERRO` path, TSS1.SYMB:3859–3860:
```
DERRO, COPY SA DX
	SAA	4
	IOX	545	  ; = IOX DRM LCR, control register := 4 (bit 2 only)
```
**[VERIFIED]** on a transfer error the driver writes control = **`4`** (bit 2
alone, no interrupt-enable/activate low bits). **[INFERRED]** this is a
stop/clear/deactivate of the device after an error; the exact effect of writing
bit 2 in isolation is **UNKNOWN** from the source.

### 2.3 Core-address register — `IOX 541` (LCX)

**[VERIFIED]** TSS1.SYMB:3804–3806: `LDA DAREG` then `IOX DRM LCX`. Loads the low
16 bits of the physical core (memory) address from A. The two high bits (16,17)
are delivered separately in the control register (§2.2). This is the DMA memory
address. **[INFERRED]** 18-bit physical core address total (16 in LCX + 2 in LCR).

### 2.4 Word-counter register — `IOX 547` (LWX)

**[VERIFIED]** TSS1.SYMB:3801–3803:
```
	SHA	ZIN 6	  ; A = (blocks for this operation) << 6  = blocks * 64
DRLWC, IOX	DRM LWX	  ; load word counter
```
The word counter is loaded with `blocks * 64`. **[INFERRED]** a drum *block*
(= sector) is **64 words**, so this value is the transfer length **in words**
(see §4 for why block=64 words is consistent with 32 sectors × 64 = 2048-word
track/page). The counter is loaded fresh before every hardware operation.

### 2.5 Block-address register — `IOX 543` (LBX)

**[VERIFIED]** TSS1.SYMB:3806–3815 computes a "HARDWARE BLOCK ADDRESS" in A, then
`IOX DRM LBX`. See §5 for the packed format.

---

## 3. Transfer model — DMA, one track-segment per hardware start

**[VERIFIED]** This is **block DMA**, not word-by-word programmed I/O: the driver
loads a *core address* (LCX), a *word count* (LWX) and a *block address* (LBX),
then a single *control-register write* (LCR) starts an autonomous transfer of many
words to/from memory. There is no per-word data IOX (no `IOX 542/546` data
register is ever read or written). The driver then returns to the OS and waits for
a completion interrupt.

**[VERIFIED]** header + code: a single hardware operation transfers **at most the
number of sectors remaining in the current track** (max 32), because the driver
caps the count at the track boundary (TSS1.SYMB:3795–3800):
```
	SAT	37	          ; T = drum-block-addr AND 037  (sector within track, 0..31)
	RAND	SD DT
	SAA	40	          ; A = 40 (=32)
	RSUB	ST DA	  ; A = 32 - sector = sectors left in this track
	SKP	IF DA LST SX  ; if (sectors-left < blocks-requested)
	COPY	SX DA	  ;   transfer = blocks-requested, else = sectors-left
```
So a request spanning a track boundary is split into multiple hardware starts,
each ended by an interrupt and a BUSY re-entry (§6).

### Numbered protocol for one hardware start (driver → device)

From `XDRUM` (TSS1.SYMB:3780–3842). Given the OS wants blocks transferred:

1. **Read status** `IOX 544` (RSX). If status bit 2 (DVA) = 1 → device still
   busy → take BUSY exit immediately (do not reprogram). If bit 4 (ERR) = 1 →
   error exit. (:3783–3787)
2. Driver computes the count for this segment (≤ sectors-left-in-track, ≤ blocks
   requested) and forms `count*64` in A. (:3795–3801)
3. **Load word counter** `IOX 547` (LWX) ← `count*64` words. (:3803)
4. **Load core address** `IOX 541` (LCX) ← A = current core address
   (`DAREG`, low 16 bits). (:3804–3806)
5. Driver computes the packed hardware block address (§5). (:3807–3813)
6. **Load block address** `IOX 543` (LBX) ← packed drum block address. (:3815)
7. Driver updates residual counters (blocks-left in X, next block addr in D, next
   core addr in `DAREG`) and builds the control word = `func<<13 |
   addr16_17<<5 | 7`. (:3816–3836)
8. **Load control register** `IOX 545` (LCR) ← control word. This **starts** the
   DMA transfer and enables the completion interrupt. (:3838)
9. **BUSY exit** back to the OS (`EXIT AD1`, :3842). The OS re-enters XDRUM on the
   drum interrupt (§6).

**[INFERRED]** register-load ordering that the emulator should accept: LWX, LCX,
LBX, then LCR-last-as-start. The device must **latch** word-count, core-address
and block-address on their IOX writes and begin the transfer only on the LCR write
whose low bits are set.

---

## 4. What XDRUM is called with (software calling convention)

**[VERIFIED]** TSS1.SYMB:3741–3763 (header):
- **X** = number of blocks (blocks here = drum sectors of 64 words — see below).
- **T** = function code: `0`=read, `1`=write, `2`=read-test, `3`=compare,
  `20`₈=read-status-only; and **T bits [8,9] = core-address bits [16,17]**.
- **A** = core (memory) address.
- **D** = block (drum) address (linear).

Returns: ERROR exit → X = drum status; BUSY exit → call again unchanged, at once
or after interrupt; FINISHED exit → X = drum status, A = core address. B is
untouched.

**Block size = 64 words [INFERRED, self-consistent]:** the word counter is loaded
with `blocks << 6` (§2.4), and the per-operation cap is 32 sectors/track
(`SAA 40`, §3). 32 sectors × 64 words = **2048 words = one TSS page = one track**.
Independently, TSS calls `TRSFR` with a disk address `DKA1 + (page << 3)` (8 units
of 256 words per 2048-word page; TSS1.SYMB:2803, 2854, 2857), and the drum front
end re-chunks a page into 32 sectors (`SAX 40`, :3720). Both descriptions of a
2048-word page are consistent only if a drum sector = 64 words.

---

## 5. Drum addressing

### 5.1 Sizes and constants **[VERIFIED]**

- `DRMSZ = 2000`₈ = 1024 (default). Comment TSS1.SYMB:46 *"SIZE OF DRUM IN 256
  WORD PAGES"*; definition TSS1.SYMB:100 `DRMSZ=2000` under the `"DRMSZ` mark.
- 32 sectors per track (mask `037` and `SAA 40`; :3795–3797, :3816–3818).
- Drum sector = 64 words **[INFERRED, §4]**; track = 32 sectors = 2048 words.

### 5.2 Drum-vs-disk split (front end `TRSFR`, TSS1.SYMB:3714–3722) **[VERIFIED code]**

```
TRSF1, LDA TRSAV; SUB (DKA1; SHA 2; COPY DD SA
	SUB (DRMSZ+DRMSZ+DRMSZ+DRMSZ; JAN TRSF2   ; if (offset*4 - 4*DRMSZ) < 0 -> drum
	LDA TRSAV; SUB (DRMSZ; COPY DT SA          ; else spill to CDC disk (TRXX)
	LDA TRSAV+1; JPL TRXX; JMP *+1; JMP TRSF3
TRSF2, LDT TRSAV+1; SHT ZIN SHR 1
	SAX 40; LDA TRSX
	JPL XDRUM; JMP TRSF1; JMP *-2               ; JMP TRSF1 = BUSY re-entry loop
```
**[VERIFIED]** blocks whose offset from `DKA1` is below the drum region go to
`XDRUM`; higher blocks spill to the CDC disk driver `TRXX`. The `SUB
(DRMSZ+DRMSZ+DRMSZ+DRMSZ)` is the `4*DRMSZ` threshold mentioned in
`docs\BUILD-TSS.md`. **[INFERRED]** because the offset is pre-scaled `SHA 2`
(×4) and compared to `4*DRMSZ`, the effective drum capacity is `DRMSZ` of these
units; the exact unit reconciliation is **not fully decoded** (see §8). Note
`TRSF2`'s `JMP TRSF1` after `JPL XDRUM` is exactly the "call XDRUM again on BUSY"
loop.

### 5.3 Linear block address → hardware block address **[VERIFIED code + comments]**

TSS1.SYMB:3807–3813:
```
	COPY	SD DT	% SAVE D  (D = linear drum block address)
	SAD	ZIN 13	% A(10-0): TRACK   D(15-11): SECTOR
	SHA	ROT 13	% A(15-11): LEAST- A(5-0): MOST SIGN. BITS OF TRACK
	RADD	SA DD	% D(15-11): MODIFIED SECTOR ADDRESS
	SHA	ROT 5	% A(10-0): TRACK
	SAD	5	% A: HARDWARE BLOCK ADDRESS
	COPY	ST DD	% UNSAVE D
```

What the comments assert (VERIFIED as the author's stated intent), i.e. the
**hardware block-address word format** loaded into LBX:

- **Bits 15–11 (5 bits): SECTOR number** (0–31), 32 sectors per track.
- **Bits 10–0 (11 bits): TRACK number** (0–2047).

and the *input* linear block address `D` is split as track = low 11 bits, sector =
high 5 bits (comment line :3808 *"A(10-0): TRACK D(15-11): SECTOR"*).

**Worked example [INFERRED from the stated formats]:** a hardware block address
word is `(sector << 11) | (track & 03777)`. E.g. track 5, sector 3 →
`(3<<11) | 5` = `014005`₈. The driver then advances the block address by the count
transferred and issues the next segment.

> **Caveat [VERIFIED honesty]:** the *exact* bit-reshuffle performed by
> `SAD ZIN 13` / `SHA ROT 13` / `SHA ROT 5` / `SAD 5` could not be fully decoded
> without precise ND-100 `SAD`/`SHA ROT` micro-semantics for these operands. The
> **field layout above is taken from the driver's own comments**, which are
> authoritative for intent; the literal instruction-by-instruction transform is
> **INFERRED**. The earlier project note of a formula `32*S + ((B+S) mod 32)`
> **could not be confirmed** against this code — the driver's model is the
> sector-in-high-5-bits / track-in-low-11-bits packing shown above. See §8.

---

## 6. Interrupt / completion mechanism

**[VERIFIED]** Completion is **interrupt-driven, with a poll-on-entry guard**. The
driver does **not** spin-wait. Its model (header TSS1.SYMB:3755–3756):
> *"BUSY EXIT: THE ROUTINE MUST BE CALLED AGAIN AT ONCE OR AFTER INTERRUPT WITH
> X,T,A AND D UNCHANGED."*

Mechanism:
1. The control-register write (§2.2, low bits = `7`) enables the completion
   interrupt and starts the transfer. **[VERIFIED]** comment :3836 *"ENABLE
   INTERRUP AND ACTIVATE DEVICE"*.
2. The driver returns via BUSY exit. When the drum finishes it raises an
   interrupt; the OS re-enters `XDRUM` at the top.
3. On re-entry the first thing done is `IOX DRM RSX; BSKP ZRO DVA DA` — the
   **status bit 2 (DVA) is the completion poll**. If DVA=1 the transfer is still
   running (BUSY again); if DVA=0 it has completed and the driver either starts
   the next segment or takes the FINISHED exit. **[VERIFIED]** TSS1.SYMB:3783–3785.

The "all done" test: `JXZ DRDS1` — when the residual block count in X reaches 0,
branch to the FINISHED path `DRDS1` (:3794, :3846). FINISHED re-reads status into
X and returns core address in A (:3846–3850).

**Emulator completion contract [INFERRED]:** keep status bit 2 (DVA)=1 from the
LCR-start until the programmed word count has been transferred; then clear DVA to 0
and raise the drum's interrupt. The driver re-reads status to observe DVA=0.

**Interrupt level and IDENT code: COULD NOT DETERMINE from this driver.** `XDRUM`
enables *the device's* interrupt but does not itself contain the level or an
`IDENT` for device 540. No `IDENT`/level binding for the drum (device 540) was
found (`grep` across `src\TSS*.SYMB`). This must come from the interrupt-vector /
`IIC`/`IDENT` setup and is **not present in the recovered drum driver** — see §8.

---

## 7. Initialisation and error handling

### 7.1 Initialisation — none specific to the drum **[VERIFIED negative]**

- `XDRUM` performs **no reset/clear/seek at startup**; its first action is
  `STT DTREG; STX DXREG; STA DAREG; IOX DRM RSX` — it just reads status
  (TSS1.SYMB:3780–3783).
- `IOINI` (the standard NORD-10 I/O init, TSS1.SYMB:783–794) clears TTY, reader,
  punch, line-printer and the CDC **disk** (`IOX LMR`) — it issues **no `IOX 54x`**
  and does not touch the drum.
- `MINIT.SYMB` (mass-storage init, `E:\Dev\Ronny\TSS\src\MINIT.SYMB`) initialises
  the **CDC disk** bit maps (NCR→CDC addressing, `DKTR`/`DKOP`) only; it contains
  **no drum device access** (`grep` for `54x`/`DRUM`/`XDRUM` finds nothing).

So an emulated drum needs no init handshake to satisfy TSS — it must simply be
ready to accept register loads and report DVA/ERR correctly. **[VERIFIED]** by the
absence of any drum init in the driver, IOINI and MINIT.

### 7.2 Error handling **[VERIFIED]**

- **Transfer/hardware error** (`DERRO`, TSS1.SYMB:3858–3863): reached when status
  bit 4 (ERR)=1. It saves the status word to X (`COPY SA DX`), writes control
  register = `4` (`SAA 4; IOX 545`), restores A,T, and takes the plain `EXIT`
  (ERROR exit). The OS receives the raw drum status word in X.
- **Illegal function code** (`DRILL`, :3854): a function code outside the legal set
  (checked by `AAA 14; JAP DRILL` at :3792–3793 after subtracting `20`) loads
  `A = -1` and falls into the ERROR exit. This is a *software* validity check, not
  a device status.
- The driver branches only on **ERR (bit 4)**; all other diagnostic bits are
  passed up to the OS unexamined.

---

## 8. UNKNOWN / COULD NOT DETERMINE FROM SOURCE

These are genuinely not derivable from the recovered driver. Do **not** invent
them; they need another primary source (a NORD-10 drum controller manual, or the
TSS interrupt-vector setup).

1. **Interrupt level and IDENT code** of device 540. The driver enables the
   interrupt but contains no level/IDENT binding; none was found elsewhere in
   `src\TSS*.SYMB`.
2. **Meaning of status bits other than bit 2 (DVA) and bit 4 (ERR).** The whole
   status word is passed to the OS on error/finish, but the driver never decodes
   individual error bits, so their layout is unknown.
3. **Exact split of control bits 0–2.** Always written together as `7`
   ("enable interrupt AND activate") on start and as `4` (bit 2 only) on error;
   which individual bit is activate vs interrupt-enable vs "master clear" is not
   distinguished by the code. The isolated-`4` write on error is inferred to be a
   stop/clear.
4. **Device registers 542 and 546** (even = read) are never used; their function
   (likely read-back of block address / control) is unknown.
5. **RCX / `IOX 540`** (read core address) is defined but never issued by XDRUM;
   its exact read-back semantics are unconfirmed for the drum.
6. **The precise `SAD`/`SHA ROT` bit-reshuffle** in the block-address computation
   (§5.3) was not decoded instruction-by-instruction; the hardware block-address
   *field layout* (sector in bits 15–11, track in bits 10–0) is taken from the
   driver's own comments and is the reliable part. The old `32*S+((B+S) mod 32)`
   formula could **not** be confirmed and appears not to match this code.
7. **Whether the word counter counts up to 0 or down, and its exact width.** Only
   the loaded value (`blocks*64` words) is known.
8. **Physical geometry beyond 32 sectors/track** (number of tracks actually
   present, RPM/latency) — the address field allows 2048 tracks but the real
   drum's track count is not stated. `DRMSZ`=1024 pages (×2048 words) =
   2,097,152 words is the configured swap capacity, not necessarily the physical
   drum size.

---

## 9. Emulator quick-reference (the octal constants you must match)

- Device block: **540–547**₈; IOX opcode = `164000 + devno`.
- **541 LCX** (write): latch core-address low 16 bits.
- **543 LBX** (write): latch drum block address = `(sector<<11)|(track&03777)`.
- **544 RSX** (read): return status; **bit 2 = ACTIVE/BUSY**, **bit 4 = ERROR**.
- **545 LCR** (write): control word = `(func<<13) | (coreaddr16_17<<5) | 7`;
  low bits set = **start + interrupt-enable**. Value `4` written on error.
- **547 LWX** (write): latch word count = `blocks*64` words.
- Function (control bits 13–14): 0=read, 1=write, 2=read-test, 3=compare.
- Segment size cap: 32 sectors (one track) per hardware start; sector = 64 words;
  track/page = 2048 words.
- Completion: clear status bit 2 and raise interrupt when the word count is
  exhausted; driver polls bit 2 on interrupt re-entry.

---

*Sources: `E:\Dev\Ronny\TSS\src\TSS1.SYMB` (lines 40–106, 783–794, 2803–2886,
3695–3877); `E:\Dev\Ronny\TSS\src\MINIT.SYMB`;
`E:\Dev\Ronny\TSS\derived\DRUM-DRIVER.SYMB`;
`E:\Dev\Ronny\TSS\derived\ASSYS-DRUM-N10-MAC-INPUT.SYMB`;
`E:\Dev\Ronny\TSS\docs\BUILD-TSS.md`;
`E:\Dev\Ronny\nd100-markdown\docs\cpu_documentation.md` (IOX);
`E:\Dev\Ronny\nd100-markdown\nd100-definitions\specs\category\bit_instructions.md`.*
