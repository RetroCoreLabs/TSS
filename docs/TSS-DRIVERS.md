# TSS 3.0 Device Drivers — Source-Grounded Reference

**Subject:** every device driver in the recovered 1973 NORD TSS 3.0 source
(Bo Lewendal base; NORD-10 additions by NJL = Nils Jakob Langeland).

**Status marking convention (read first):**

- `[VERIFIED]` — read directly from source, with `file:line` citation.
- `[ASSUMPTION]` — my interpretation of verified source; not stated by the source.
- `NOT DETERMINED FROM SOURCE` — I could not establish it from the files I read.

All citations are of the form `src/TSSn.SYMB:LINE` or
`derived/DRUM-DRIVER.SYMB:LINE`. Octal is the MAC default radix (a leading
digit with no `D` suffix is octal). Cross-references: existing docs
`docs/CDC-DISC-DEVICE.md`, `docs/DRUM-DEVICE-SPEC.md`,
`docs/OVERLAY-DISC-SPEC.md`, `docs/TSS-Analysis.md` cover the disc/drum
geometry and boot path in more depth; this document is the *driver-code*
inventory and is deliberately not a duplicate of them.

---

## 1. The NORD-1 vs NORD-10 I/O abstraction

TSS is assembled either for the **NORD-1** (library mark `NN10`, "not N10")
or the **NORD-10** (library mark `N10`). The two machines have completely
different I/O instruction sets, and essentially every driver carries a
`"NN10` / `"N10` split.

- **NORD-1 uses `IOT`.** `IOT` takes a device number plus one or more of the
  strobe sub-fields `ACT` (activate), `PIN` (permit interrupt), `SKA` (skip
  if active/ready), `SNI` (skip if not interrupting/busy). These same
  sub-field symbols double as *register offsets* when added to a channel
  base — e.g. `LCA=DCT+SNI`, `LBA=DCT+ACT`, `LMR=DCT+SKA`, `RST=DCT+PIN`
  `[VERIFIED]` src/TSS1.SYMB:3362-3369.
  `[ASSUMPTION]` `ACT`/`PIN`/`SKA`/`SNI` are MAC permanent symbols (they are
  never defined by an `=` in the TSS source); their exact numeric values are
  `NOT DETERMINED FROM SOURCE` here.
- **NORD-10 uses `IOX`.** Register access is by fixed offsets from the device
  number: `RDR=0` read-data, `WDR=1` write-data, `RSR=2` read-status,
  `WCR=3` write-control `[VERIFIED]` src/TSS1.SYMB:435-438.
- **`EXRG`/`RGDIV` are NORD-10 hardware instructions** that the NORD-1 build
  supplies as software-emulation routines. `EXRG SX` assembles to
  `0140600|SX` and `RGDIV ST` to `0141660` on N10; on NN10 they are labels of
  emulation routines `[VERIFIED]` src/TSS1.SYMB:108-119, src/TSS1.SYMB:3602-3616.
  These appear directly inside driver I/O sequences (e.g. `EXRG SA` in TTYDI,
  src/TSS1.SYMB:1472).

### Interrupt-level model
`INIT` wires the level vectors differently per machine
`[VERIFIED]` src/TSS1.SYMB:234-306:

- **NN10 (NORD-1):** each level has its own hardware line. Input driver runs
  on **LEV11** (`STA I (224`), output on **LEV7** (`STA I (160`); LEV11 body
  at src/TSS1.SYMB:1391, dispatches by polling `IOT SNI <dev>` for each
  device `[VERIFIED]` src/TSS1.SYMB:1391-1420. The output-side poll block is
  at src/TSS1.SYMB:1862-1880.
- **N10 (NORD-10):** vectored via `IDENT`. **LEV12** input
  (`LDA (LEV12; IRW 140 DP`) uses `IDENT PL12` to get the interrupting device
  code and dispatches through a jump table `LT1..LT4/LTR/LCR/LSM`
  `[VERIFIED]` src/TSS1.SYMB:1429-1456.

---

## 2. Teletype / terminal driver (TTYDI / TTYDO)

The console/terminal subsystem is the largest driver. It models up to **24
terminals** (`NTTY`, configurable; the golden build uses a specific count —
`NTTY` is `=`'d several times per configuration at src/TSS1.SYMB:128-150).
There are `NTY` non-modem terminals (default 4, src/TSS1.SYMB:58).

### Device-number tables
Per-terminal device numbers are configuration-dependent and differ NCR / CDC
/ N10 `[VERIFIED]` src/TSS1.SYMB:776-836:

- NCR: `TTY1=2, TTY2=4, TTY3=72, …`
- CDC (NN10): `TTY1=2, TTY2=104, TTY3=120, …` up to `TTY14=156`
- N10: `TTY1=300, TTY2=310, …` (step 10), then `TTY9=1300…TTY16=1370`

Input/output device numbers are derived: on NN10 `TTYnO=TTn+1`
(src/TSS1.SYMB:960-1030); on N10 `TTYnO=TTn+4` (src/TSS1.SYMB:1033-1080).
The dispatch tables are `DEVTB` (input+output device numbers,
src/TSS1.SYMB:1147-1154), `TDEVI`/`TDEVO` (src/TSS1.SYMB:1361-1370).

### Ring/buffer model — BUFTB
`BUFTB` is the master buffer table, one entry per device
`[VERIFIED]` src/TSS1.SYMB:1335-1345. Layout for each terminal buffer
control block (documented in-source, src/TSS1.SYMB:1161-1175):

```
word 0-5   buffer descriptor
word 6     busy flag
word 7     break pointer
word 10    echo pointer
word 11-20 break table
word 21-30 echo table
word 31    link to TTY(31)
word 32    special break/echo flag
word 33    echo buffer pointer
word 34    echo flag (negative if on)
word 35    echo buffer count
```

Input control blocks `TBI1..TBI24` and output blocks `TBO1..TBO24` are
allocated with `BSS 36` / `BSS 7` and the per-terminal data buffers `BB1..`
sized by `9TTI`/`9TTO` (=140 octal) `[VERIFIED]` src/TSS1.SYMB:672-769,
src/TSS1.SYMB:1177-1332.

### The four buffer primitives (each `)PCL`'d as its own scope)
`[VERIFIED]`:

- **`WBUF`** (src/TSS1.SYMB:351-382) — put a character into a device buffer;
  begins `AND 9377` masking the char (the `9377` all-digit-octal parse was
  the login-hang bug per MEMORY.md; here it is a symbol, value elsewhere).
- **`RBUF`** (src/TSS1.SYMB:391-404) — take a character from a buffer.
  `9BUFT, BUFTB` (src/TSS1.SYMB:408) is the buffer-table pointer.
- **`SBUF`** (src/TSS1.SYMB:485-529) — status of a buffer (indexed via
  `9BUFT`).
- `REBUF` (src/TSS1.SYMB:417-428) is an echo-buffer variant.

### TTYDI — teletype input driver
`[VERIFIED]` src/TSS1.SYMB:1465-1538. Entry: `A`=char, `T`=teletype number.
- Reads status/data register: NN10 `IOT ACT`/`IOT PIN` built dynamically by
  `RORA DA SX; STA *+1` self-modifying code (src/TSS1.SYMB:1468, 1476); N10
  path uses `IOX RSR`/`IOX RDR` with `EXRG` and `BSKP ZRO 40 DA` ready test,
  error to `TERR` (src/TSS1.SYMB:1470-1472).
- Parity check via `CKPAR` (src/TSS1.SYMB:1485), break/echo handling
  (`BRKCH`, `ECHCH`, `ECFST`), fills buffer via `WBUF`/`SBUF`.
- `"DIAB` block special-cases the Diablo device (src/TSS1.SYMB:1478-1482).
- `"FROG` flow-control block (X-ON/X-OFF style via `YSTOP`/`L7TBL`)
  src/TSS1.SYMB:1491-1499.

### TTYDO — teletype output driver
`[VERIFIED]` src/TSS1.SYMB:1930-1974. Entry same regs. Pulls a character with
`RBUF` (`AAT NTTY` selects the output half of the tables), and either writes
data (`IOX WDR` on N10, self-modified `IOT` on NN10, src/TSS1.SYMB:1946) or
marks buffer empty. `SBUF` re-arms.

`INBT`/`OUTBT`/`TCI`/`TCO` **NOT DETERMINED FROM SOURCE** — these names were
in the task brief but I did not find them in `src/`; the character-level
entry points that exist are `WBUF`/`RBUF`/`SBUF`/`ECHCH`/`BRKCH` and the
higher-level `CINF`/`COUTF` in TSS2 (src/TSS2.SYMB:1265,1375).

---

## 3. CDC disc driver (DKOP / DKTR / DKADR)

The primary mass-storage device. Library marks `CDC` (vs `NCR`, an
alternative disc). Cross-ref `docs/CDC-DISC-DEVICE.md`.

### Channel and register map
`[VERIFIED]` src/TSS1.SYMB:3348-3379:

- Disc channel base `DCHN`: **NN10 = 100**, **N10 = 500** (src/TSS1.SYMB:3350-3352).
- NN10 (IOT): `DISC=DCHN+44` (start transfer), `DCT=DCHN+45` (control),
  and register strobes `LCA=DCT+SNI`, `LBA=DCT+ACT`, `LMR=DCT+SKA`,
  `RST=DCT+PIN`, `RCA=DCT+PIN+ACT`, `RSECT=DCT+PIN+SKA`,
  `RDC=DCT+PIN+SKA+ACT`, `SEEK=DCT+SKA+ACT`.
- N10 (IOX): `RCA=DCHN+0, LCA=DCHN+1, RSECT=DCHN+2, LBA=DCHN+3, RST=DCHN+4,
  LMR=DCHN+5, SEEK=DCHN+6, LWC=DCHN+7`.

Register mnemonics: LCA=load core-address, LBA=load block-address, LMR=load
modus register, LWC=load word-count (N10 only), RST=read status, RCA=read
core address, RSECT=read sector counter, RDC=reset controller, SEEK=position.

### DKOP — 256-word disc operation
`[VERIFIED]` src/TSS1.SYMB:3386-3413. Inputs documented in-source: `X`=core
addr, `T`=NCR disc addr, `A`=operation (0 read, 1 write, 2 parity test,
3 compare), `D`=unit 0-3. Wraps `DKTR`.

### DKTR — disc transfer engine
`[VERIFIED]` src/TSS1.SYMB:3417-3571. `A[1,0]`=operation, `A[7,6]`=unit,
`D`=extra 256-word blocks (up to 3K words). Sequence: wait ready
(`DWAIT`), reset (`IOT RST`/`IOX RST` + `BSKP ONE 160 DA`), load core
addr (`LCA`), convert address via `DKADR`, load block addr (`LBA`), compute
extra blocks, load modus (`LMR`; N10 also `LWC`), start (`IOT ACT DISC` on
NN10; N10 relies on LMR write), wait completion, check status, loop for
multi-block. Retries via `RTRY` (init `-2`, src/TSS1.SYMB:3433). Error
returns `A=-1` for illegal address (`DKF1`) or the status register (`DKF2`).

### DKADR — NCR→CDC address conversion
`[VERIFIED]` src/TSS1.SYMB:3578-3654. Converts an NCR-style disc address into
the CDC controller's cylinder/track/sector form. `DKLIM=626` cylinders
(313 for CDC 9425, 626 for CDC 9427, src/TSS1.SYMB:3586-3587). Divide-by-12
gives the CDC `[14,5]` field: NN10 does it with a reciprocal multiply
(`MPY 952`, `952=52525`, src/TSS1.SYMB:3598,3652); N10 uses the real hardware
`RGDIV` (register divide) — the reconstruction note at src/TSS1.SYMB:3602-3616
is the load-bearing fix. Unit-1 displacement `52600` added, legality checked
against `DKLIM`.

### Utility-level disc paths
- `XDISK` (interactive disc utility) — its own NCR/CDC/N10 transfer loop with
  operator error messages `$DISK ERROR AT NCR(..) CDC(..)$`
  `[VERIFIED]` src/TSS2.SYMB:530-598. Calls `DKADR` (src/TSS2.SYMB:556,568).
- `TRXX` (2K file-system transfer) `[VERIFIED]` src/TSS1.SYMB:3657 ff and the
  extracted `derived/DRUM-DRIVER.SYMB:9-19`.

### Boot-time disc (HDKOP)
Cold-boot loader has its own minimal disc reader `HDKOP`
`[VERIFIED]` src/TSS3.SYMB:142-171: NCR variant (src/TSS3.SYMB:142) and CDC
variant with inline address conversion and `8DKLM=616` (src/TSS3.SYMB:154-170).
`8WORD` reads the boot stream via `IOT ACT SKA REA` (paper-tape reader),
src/TSS3.SYMB:136. Also `RDKOP`/`DBOOT` in TSS3 (src/TSS3.SYMB:244-293).

---

## 4. Swapping drum (XDRUM / TRSFR)

NORD-10 only; NJL, 17/4/73. Compiled **out** of both archived golden builds
(needs `DRUM`+`N10` marks). Extracted to `derived/DRUM-DRIVER.SYMB`.
Cross-ref `docs/DRUM-DEVICE-SPEC.md`.

### Device and registers
`[VERIFIED]` derived/DRUM-DRIVER.SYMB:75-86:
- `DRM=540` main device number (IOX 540).
- Register offsets: `RCX=0` read-core-addr, `LCX=1` load-core-addr,
  `LBX=3` load-block-addr, `RSX=4` read-status, `LCR=5` load-control,
  `LWX=7` load-word-count.
- Status bits: `DVA=020` device-active, `ERR=040` OR-of-errors.
- Drum size `DRMSZ=2000` pages of 256 words (src/TSS1.SYMB:46,100).

### TRSFR / TRXX front end
`[VERIFIED]` derived/DRUM-DRIVER.SYMB:9-40. `"CDC-DRUM` aliases
`TRSFR=TRXX`; the real `"DRUM N10` `TRSFR` decides between disc (`TRXX`→
`DKTR`) and drum (`XDRUM`) based on the block address vs `DKA1`/`DRMSZ`
windows (src/DRUM-DRIVER.SYMB:26-34).

### XDRUM — drum transfer routine
`[VERIFIED]` derived/DRUM-DRIVER.SYMB:92-183. Calling sequence
`JPL I (XDRUM` + 3 return points (error/busy/finished). Inputs:
`X`=#blocks, `T`=function (0 read,1 write,2 read-test,3 compare,20 read-status;
bits[8,9]=core-addr bits[16,17]), `A`=core addr, `D`=block addr. Reads status
(`IOX DRM RSX`), tests `DVA`/`ERR`, computes sectors-left-in-track (32-sector
tracks), loads word-count/core/block/control registers, activates with
interrupt-enable (`AAA 7`). Error path `DERRO` issues `IOX 545`
(=`DRM+RSX+ACT`, src/DRUM-DRIVER.SYMB:170-175).

---

## 5. Paper-tape reader (READR) and punches (PUNKX / SPUNK)

### High-speed paper-tape reader — REA
`[VERIFIED]` src/TSS1.SYMB:1590-1613. Device `REA` (N10 `REA=400`,
src/TSS1.SYMB:1111; NN10 uses `9PPT=600` buffer sizing, src/TSS1.SYMB:618).
NN10 arms with `IOT ACT REA`/`IOT PIN REA`; N10 uses `IOX REA RDR` and
`IOX REA WCR`. Buffer via `WBUF`, slot `READV=NTTY+NTTY+1` (src/TSS1.SYMB:1351).
Interrupt-side entry `LL2→READR` on NN10 (src/TSS1.SYMB:1408),
`LTR→READR` on N10 (src/TSS1.SYMB:1442).

### Fast punch — DFP
`[VERIFIED]` `PUNKX` src/TSS1.SYMB:1977-1992. Device `DFP` (NN10 `DFP=7`
src/TSS1.SYMB:1132; N10 `DFP=410` src/TSS1.SYMB:1112). N10:
`IOX DFP WDR; IOX DFP WCR`; NN10: `IOT PIN ACT DFP`. Buffer slot `PNCHD`.

### Slow punch — DSP (NN10 only)
`[VERIFIED]` `SPUNK` src/TSS1.SYMB:1999-2008. `DSP` (NN10 `DSP=17`
src/TSS1.SYMB:1137; N10 `DSP=0` src/TSS1.SYMB:1140 — effectively disabled).
Slot `SPNCD`. `9SP=5` buffer (src/TSS1.SYMB:632).

---

## 6. Line printer (PNTDR) — DLP

`[VERIFIED]` src/TSS1.SYMB:2011-2035. Device `DLP=DLPX`; `DLPX` is
NCR `67`, CDC (NN10) `167`, N10 `430` (src/TSS1.SYMB:1098-1118).
- NN10: `IOT ACT DLP; IOT SKA DLP` to send, `IOT PIN DLP` to re-arm
  (src/TSS1.SYMB:2021-2024).
- N10: `IOX DLP RSR` ready check (error→`TERR`, sets `SER10`),
  `IOX DLP WDR; IOX DLP WCR` (src/TSS1.SYMB:2016-2028).
- Buffer via `RBUF`, slot `PNTDV`. Buffer size `8LP=100` (src/TSS1.SYMB:639).
- Interrupt dispatch: NN10 `LL1→` printer/`DLP` poll (src/TSS1.SYMB:1864).

The demand loader also drives a line printer directly (`IOX DLP+1/+3/+5`,
src/TSS1.SYMB:1841-1843).

---

## 7. Card reader (CARD1) — DCR

`[VERIFIED]` src/TSS1.SYMB:1623-1680. Device `DCR` (NN10 `42`,
src/TSS1.SYMB:1123; N10 `420`, src/TSS1.SYMB:1127). Buffer size `9CR=52`
(src/TSS1.SYMB:646). Intermediate buffer `CTBB` (121 words, src/TSS1.SYMB:1618).
- NN10: `IOT ACT DCR`, ready mask `7400`, re-arm `IOT PIN DCR` /
  `IOT PIN ACT DCR+1`.
- N10: `IOX DCR RSR` with status bits (`BSKP ZRO 40`, `BSKP ONE 50/110`),
  `IOX DCR RDR` read, `IOX DCR WCR` with control codes 3/7/20
  (src/TSS1.SYMB:1629-1670).
- Card-code↔ASCII translation via `CONV` and tables `CTB1`/`CTB2`
  (revised 1971-08-04), `[VERIFIED]` src/TSS1.SYMB:1685-1708.
- Interrupt dispatch: NN10 `LL1→CARD1` (src/TSS1.SYMB:1407),
  N10 `LCR→CARD1` (src/TSS1.SYMB:1443). Slot `CRDDV`.

---

## 8. Diablo terminal/printer (DIABD / DIABT)

`[VERIFIED]`. `DIABD`/`DIABT` are **not** defined in `src/TSSn.SYMB`; they are
supplied by the CVS build-input streams: `DIABD=156` (device number),
`DIABT=4` (terminal slot), guarded by library mark `DIAB`
`[VERIFIED]` src/ASSYSA.SYMB:9, src/ASSYSB.SYMB:9, src/TDUMP.SYMB:4.
- TTYDI special-case: if the device equals `DIABD`, characters `<6` clear
  `DIABS` state (src/TSS1.SYMB:1478-1482).
- Diablo output uses `IOT ACT DIABD+2` / `IOT ACT DIABD+3` with a `177774`
  control word (src/TSS1.SYMB:2672-2676).
- Higher-level Diablo handling `SECHS`/`ODC`/`ODX` and `DIABS` state in TSS2
  (src/TSS2.SYMB:516, 3456-3480); it is treated as a terminal slot `DIABT`
  in the CINF/COUTF character path (src/TSS2.SYMB:1275,1383).
- `DIABS` state cell at src/TSS1.SYMB:4541.

---

## 9. Modem / ACM (8MDM / 8ACM) — DMSI / SADR

`[VERIFIED]`. Conditional on marks `8MDM` (modem) and `8ACM` (ACM); **off** in
the golden builds. Devices: `DMSI=170` (modem read-status), `SADR=160`
(ACM set-address) `[VERIFIED]` src/TSS1.SYMB:1089-1095, comment table
src/TSS1.SYMB:39-40.
- Input driver `MIDX` on level LEV11 via `LL4`/`LL7`
  (src/TSS1.SYMB:1410-1412,1558-1575): `IOT ACT DMSI+2`, status words
  `8ST1=5772` (send status), `8ST2=1772` (receive char-mode),
  `8ST3=772` (receive binary-mode) `[VERIFIED]` src/TSS1.SYMB:1581-1583.
- Output driver `KMN`/`OLS` (src/TSS1.SYMB:2043-2050): `IOT ACT/PIN DMSI+3`.
- ACM address-set interrupt `LL3` sets `AATIM` (src/TSS1.SYMB:1414-1415);
  device `SADR+4` polled (src/TSS1.SYMB:1399).
- Modem slots `MODIN`/`MODOT` (src/TSS1.SYMB:1356-1357).

---

## 10. Other devices referenced

- **Plotter — PLOT=153** `[VERIFIED]` src/TSS1.SYMB:1083-1085, comment
  src/TSS1.SYMB:41. State cell `LPLOT` (src/TSS1.SYMB:4020). No dedicated
  driver body found beyond the device equate and buffer state.
  `[ASSUMPTION]` plotter output rides the generic buffered-output path.
- **Magnetic tape — MT=134** (mark `MT`, NN10) `[VERIFIED]` src/TSS1.SYMB:1104-1106,
  comment src/TSS1.SYMB:38. A dedicated mag-tape driver body was
  **NOT DETERMINED FROM SOURCE** in the files I read; marks `HP`/Kennedy
  select tape hardware (src/TSS1.SYMB:57) but the routine was not located.
- **Real-time clock / interval timer** — `IOT 6` / `IOT PIN 6` at INIT
  (src/TSS1.SYMB:236,290-292); N10 `IOX 11`/`IOX 13` clock setup
  (src/TSS1.SYMB:303-304). `[ASSUMPTION]` these are the RTC device;
  exact device identity not labelled in source.

---

## 11. Summary table

| Device | Channel / device no. | Driver routine(s) | N1 (NN10) / N10 | Source |
|---|---|---|---|---|
| Teletype input | per-TTY `TDEVI` tbl (NCR/CDC/N10 sets) | `TTYDI`, `WBUF`/`SBUF` | both (IOT self-mod / IOX+EXRG) | src/TSS1.SYMB:1465-1538 |
| Teletype output | per-TTY `TDEVO` tbl | `TTYDO`, `RBUF`/`SBUF` | both | src/TSS1.SYMB:1930-1974 |
| Buffer primitives | `BUFTB` | `WBUF`,`RBUF`,`SBUF`,`REBUF` | both | src/TSS1.SYMB:351-529 |
| CDC disc | `DCHN` NN10=100 / N10=500; `DISC=DCHN+44` | `DKOP`,`DKTR`,`DKADR` | both (IOT / IOX+LWC) | src/TSS1.SYMB:3386-3654 |
| Disc (utility) | same | `XDISK`,`TRXX`,`TDISK` | both | src/TSS2.SYMB:530-598 |
| Disc (boot) | same | `HDKOP`,`RDKOP`,`8WORD` | both | src/TSS3.SYMB:142-171 |
| Swapping drum | `DRM=540` (IOX) | `XDRUM`,`TRSFR` | N10 only | derived/DRUM-DRIVER.SYMB:92-183 |
| Paper-tape reader | `REA` (N10=400) | `READR` | both | src/TSS1.SYMB:1590-1613 |
| Fast punch | `DFP` (NN10=7 / N10=410) | `PUNKX` | both | src/TSS1.SYMB:1977-1992 |
| Slow punch | `DSP` (NN10=17 / N10=0) | `SPUNK` | NN10 only | src/TSS1.SYMB:1999-2008 |
| Line printer | `DLP` (NCR=67/CDC=167/N10=430) | `PNTDR` | both | src/TSS1.SYMB:2011-2035 |
| Card reader | `DCR` (NN10=42 / N10=420) | `CARD1`,`CONV` | both | src/TSS1.SYMB:1623-1708 |
| Diablo | `DIABD=156`,`DIABT=4` (CVS input) | TTYDI/TTYDO + `SECHS`,`ODC` | both, mark `DIAB` | src/TSS1.SYMB:1478-1482; src/TSS2.SYMB:3456-3480 |
| Modem | `DMSI=170` | `MIDX`,`KMN`,`OLS` | mark `8MDM` (off) | src/TSS1.SYMB:1558-1575,2043-2050 |
| ACM | `SADR=160` | `LL3` handler | mark `8ACM` (off) | src/TSS1.SYMB:1399,1414-1415 |
| Plotter | `PLOT=153` | (buffered path) | — | src/TSS1.SYMB:1083-1085 |
| Mag tape | `MT=134` | NOT DETERMINED | mark `MT` | src/TSS1.SYMB:1104-1106 |

---

## Open items (honestly unresolved)

1. `INBT`/`OUTBT`/`TCI`/`TCO` from the brief were **not found** in `src/`;
   the actual char-level API is `WBUF`/`RBUF`/`SBUF`/`ECHCH`/`BRKCH`.
2. Numeric values of MAC permanent IOT strobe symbols `ACT`/`PIN`/`SKA`/`SNI`
   are not in the TSS source (they come from MAC's permanent table).
3. No standalone **magnetic-tape** driver body was located despite `MT=134`
   and the `HP`/Kennedy configuration marks.
4. The **RTC/interval-timer** device identity (`IOT 6`, `IOX 11/13`) is used
   but not named with a device equate.
