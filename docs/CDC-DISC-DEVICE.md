# CDC / Cartridge-Disc Device (nd100x, IOX 500-507)

Emulated **CDC 9427 "Hawk" cartridge-disc controller** that serves NORD TSS's
**code overlays** in the nd100x ND-100 emulator. TSS's level-5 reader `S5` loads
each overlay from this disc; without it the overlay read returns nothing and TSS
hangs.

- Emulator repo: `E:\Dev\Emulators\ND\nd100x`
- Device source: `E:\Dev\Emulators\ND\nd100x\src\devices\cdc\deviceCDC.c` / `deviceCDC.h`
- Unit test: `E:\Dev\Emulators\ND\nd100x\tests\test_cdc.c`
- Companion overlay spec: `E:\Dev\Ronny\TSS\docs\OVERLAY-DISC-SPEC.md`
- Device-framework guide: `E:\Dev\Ronny\TSS\docs\ND100X-DEVICE-GUIDE.md`

## Authoritative sources

The register/bit model is now taken from Norsk Data's own programming
specifications, not reverse-engineered from the driver:

- **[MANUAL-N10]** `ND-11.008.01 CARTRIDGE DISC SYSTEM FOR NORD-10` (Dec 1973 /
  Rev.A 1976) — **PRIMARY**, because this is the controller TSS actually drives.
  `E:\Dev\Ronny\NDInsight\Reference-Manuals\10\ND-11.008.01 CARTRIDGE DISC SYSTEM FOR NORD-10.md`
- **[MANUAL-N100]** `ND-06.016.01 NORD-100 Input/Output System` pp.188-190 — the
  later ND-100 generation of the same 500-507 disc; used only for the fuller
  status-word bit names and the 24-bit core-address two-access form.
  `E:\Dev\Ronny\NDInsight\Reference-Manuals\ND-06.016.01_NORD-100_Input_Output_System.md` (~L6138-6270)
- **[TSS]** TSS's own CDC driver — behavioural ground truth for the handful of
  bits it reads (busy / error / on-cylinder). TSS source: `E:\Dev\Ronny\TSS\src\TSSn.SYMB`.

Every claim is tagged **[VERIFIED]** (manual page or TSS file:line) or
**[INFERRED]**. Where a manual bit and what TSS reads could differ, **TSS wins**
and the manual bit is noted.

---

## 1. Bus identity

| property | value | source |
|---|---|---|
| IOX address block | **0500-0507** (system I; +010 for system II) | **[VERIFIED - MANUAL-N10 p.13]**; TSS build DCHN=500 **[VERIFIED - TSS1.SYMB:3336]** |
| IDENT code | **01** (octal) | **[VERIFIED - MANUAL-N10 p.20 / MANUAL-N100 p.190]** "ident number for the first disc system is 1" |
| interrupt level | **11** | **[VERIFIED - MANUAL-N10 p.20 / MANUAL-N100 p.190]** "disc interrupt level is 11" |
| sector size (TSS contract) | **256 words** (512 bytes) | **[VERIFIED - TSS2.SYMB:552]** `LDA (400; IOX LWC` = 0400 = 256 words/transfer |

The real CDC 9427 physical sector is 128 data words + 1 CRC word
(**[MANUAL-N10 p.6]**); TSS's overlay contract uses 256-word transfers into a
linear sector store, so the device models 256-word sectors (the TSS ground
truth), not the raw 128-word physical sector.

This is the **older cartridge-disc controller**, NOT the ECC/SMD "Big Disc"
(nd100x `deviceSMD` @ 1540). Do not copy SMD's CHS/seek/ECC registers.

---

## 2. Register map (address − startAddress)

**[VERIFIED - MANUAL-N10 p.13 "DISC DEVICE REGISTER ADDRESSES"]**, confirmed by
the later **[MANUAL-N100 p.188]** and by the TSS driver **[VERIFIED -
TSS1.SYMB:3356-3364]** (`"CDC N10"` branch, DCHN=500). nd100x routes **even**
device numbers through `Read()` and **odd** through `Write()` (`io.c` parity
split), which matches this map exactly. The C enum is `CdcRegister` in
`deviceCDC.h`.

| reg | IOX | dir | name | meaning |
|---|---|---|---|---|
| 0 | 500 | Read  | `CDC_REG_RCA`   | Read Core Address register (24-bit: low-16 then high-8) |
| 1 | 501 | Write | `CDC_REG_LCA`   | Load Core Address register |
| 2 | 502 | Read  | `CDC_REG_RSECT` | Read Sector Counter (bits 0-4 relevant) |
| 3 | 503 | Write | `CDC_REG_LBA`   | Load Block (disc) Address |
| 4 | 504 | Read  | `CDC_REG_RST`   | Read Status Register (the register `DWAIT` polls) |
| 5 | 505 | Write | `CDC_REG_LCW`   | Load Control Word — **writing it starts the transfer** |
| 6 | 506 | Read  | `CDC_REG_SEEK`  | Seek; with control bit 3 (test) = Read Block Address |
| 7 | 507 | Write | `CDC_REG_LWC`   | Load Word Count Register |

> The old model called register 5 `LMR` ("load modus"). It **is** the manual's
> **Load Control Word**; renamed accordingly.

### The transfer sequence (the "GO")

**[VERIFIED - TSS2.SYMB:548-557]** (`XDISK`, `"CDC N10"`):

```
T0, IOX RST; JMP *+1; BSKP ONE 160 DA; JMP *-3   % poll status until READY (bit 14)
    LDA XREG,B; IOX LCA                            % load core address        (501)
    LDA TREG,B; JPL I (DKADR; JMP XDF1; IOX LBA    % load disc address        (503)
    LDA (400; IOX LWC                              % load word count = 256     (507)
    LDA AREG,B; SHA SHR 1; SHA 13; AAA 4; IOX LCW  % load control word -> STARTS xfer (505)
    JPL I (DWAIT; JMP T0                           % wait for completion
    IOX RST; JMP *+1; BSKP ZRO 40 DA; JMP T2       % read status; branch on ERR (bit 4)
```

There is **no separate ACT/GO strobe** on the N10 path: the transfer is started
by the `IOX LCW` write with control bit 2 (activate) set. A live boot trace
(`--trace`) shows exactly this per overlay: `501 → 503 → 507 → 505 → (poll 504)`.

---

## 3. Control Word (LCW = IOX 505)

**[VERIFIED - MANUAL-N10 p.14-15 "Load Control Word (CW)"]**. Modelled as a
bit-field union `CdcControlRegister` (like `deviceSMD.h`'s `SMDControlRegister`;
all fields `uint16_t` so the `raw` overlay survives MinGW `-mms-bitfields`).

| bit | field | meaning |
|---|---|---|
| 0 | `enableInterruptReady` | enable interrupt on device ready-for-transfer |
| 1 | `enableInterruptError` | enable interrupt on errors |
| 2 | `activate` | **ACTIVATE device — starts the transfer** |
| 3 | `testMode` | test mode (pre-wired self-test, §6) |
| 4 | `deviceClear` | device clear (clears active flip-flop + controller error) |
| 5 | `addressBit16` | core address bit 16 (extension, §5) |
| 6 | `addressBit17` | core address bit 17 (extension, §5) |
| 7-8 | `unassigned7_8` | not assigned |
| 9-10 | `unitSelect` | unit select (0..3) |
| 11-12 | `deviceOperation` | **00 read / 01 write / 10 read-parity / 11 compare** |
| 13-14 | `unassigned13_14` | not assigned |
| 15 | `writeFormat` | write format (write sector address tags) |

> **Correction of the old model.** The earlier nd100x derivation decoded transfer
> direction from **bit 13** (from `SHA 13` in `XDISK`). That was **wrong**: the
> device operation is **bits 11-12** per the manual. It happened to work for the
> READ path only because for a READ, `AREG=1`, `(1 SHR 1)=0`, so `SHA 13` shifts
> zero and the READ control word is just `04` (activate) with bits 11-12 = 0.
> The device now decodes the operation from bits 11-12; the READ path is
> unchanged, and WRITE / read-parity / compare are now correct per the manual.

Device operation codes are the enum `CdcOperation` (`CDC_OP_READ=0`,
`CDC_OP_WRITE=1`, `CDC_OP_READ_PARITY=2`, `CDC_OP_COMPARE=3`).

**Device clear** — **[VERIFIED - TSS2.SYMB:557 / TSS1.SYMB:3543]** `SAA 20; IOX
LCW`: writing `0o20` alone (bit 4, no activate) clears the controller (active +
all error bits). This is the manual's bit-4 device-clear.

---

## 4. Status Register (RST = 504)

**[VERIFIED - MANUAL-N100 p.190 "Status Word"]**, which agrees with
**[MANUAL-N10 p.17]**. Modelled as the union `CdcStatusRegister`.

| bit | field | meaning |
|---|---|---|
| 0 | `readyIntEnabled` | ready-for-transfer, interrupt enabled |
| 1 | `errorIntEnabled` | error interrupt enabled |
| 2 | `active` | **DEVICE ACTIVE (BUSY)** — *TSS reads this* |
| 3 | `readyForTransfer` | device ready / finished |
| 4 | `errorOr` | **inclusive OR of errors (bits 5-11)** — *TSS reads this* |
| 5 | `writeProtect` | write protect violate |
| 6 | `timeOut` | time out |
| 7 | `hardwareError` | missing clock / disk fault / seek error |
| 8 | `addressMismatch` | address mismatch |
| 9 | `parityError` | parity error |
| 10 | `compareError` | compare error |
| 11 | `dmaError` | DMA error / missing read clocks |
| 12 | `transferComplete` | transfer complete (WC = 0) |
| 13 | `transferOn` | transfer on |
| 14 | `onCylinder` | **ON CYLINDER (READY)** — *TSS reads this* |
| 15 | `loadedByPrevCw` | bit 15 loaded by previous control word |

### The three bits TSS actually reads (behavioural ground truth)

The driver tests status with ND bit-skip instructions whose operand is
**(bit-number << 3)**:

| bit | `CDC_STATUS_*` | meaning | source |
|---|---|---|---|
| 2  | `BUSY`  (0o4)     | active — DWAIT loops while set | **[VERIFIED - TSS1.SYMB:3906]** `BSKP ONE 20 DA` |
| 4  | `ERR`   (0o20)    | inclusive OR of errors | **[VERIFIED - TSS2.SYMB:555]** `BSKP ZRO 40 DA` |
| 14 | `READY` (0o40000) | on cylinder / ready | **[VERIFIED - TSS2.SYMB:549]** `BSKP ONE 160 DA` |

Idle status returned by the device = `onCylinder` (READY) set, everything else
clear. On completion `Cdc_End` clears `active`/`transferOn` and sets
`transferComplete`/`readyForTransfer`; error bits set by the engine are left in
place until a device clear.

---

## 5. Core address width — 18-bit (this controller) vs 24-bit (ND-100)

- **[VERIFIED - MANUAL-N10 p.14; TSS]** On the NORD-10 CDC 9427 the core address
  is **18 bits**: a single 16-bit Load Core Address (IOX 501) plus **control-word
  bits 5-6** as address bits 16-17. TSS drives exactly this — one `IOX LCA` write,
  and it never sets control bits 5-6 (overlay core addresses are small), so the
  effective address == the 16-bit LCA value. `cdc_effective_core()` assembles it.
- **[INFERRED / not used by TSS - MANUAL-N100 p.188]** The later ND-100 generation
  widened this to **24 bits via two consecutive accesses**: Load Core Address =
  high-8 then low-16; Read Core Address = low-16 then high-8, the sequence
  re-initialised by a read-status / device-clear / master-clear.
  - The device **models the two-read form of `Read Core Address`** (a lone read
    yields the low 16, a second consecutive read yields the high 8) — harmless to
    any single-read caller. A read-status resets the phase.
  - The device **deliberately does NOT** model a blind two-**write** Load Core
    Address: TSS issues a single LCA write, so treating the first write as
    "high-8 only" would corrupt the address. Load Core Address is therefore a
    single 16-bit write, which is **[VERIFIED]** correct for the controller TSS
    drives.

## 5b. Addressing and the DKADR gap

- The overlay contract is a **linear 256-word sector**: `sector = OVLAY*2 + OVDK`
  (OVDK = 0160 octal). **[VERIFIED]** reader `TSS1.SYMB:3071`, writer
  `TSS3.SYMB:38`.
- **[VERIFIED]** The running TSS driver converts the logical overlay sector to a
  PHYSICAL disc address via **`DKADR`** (`TSS1.SYMB:3563-3623`) **before** loading
  LBA (`TSS2.SYMB:551`). So the LBA value that arrives is already the physical
  sector. This device therefore treats the LBA value **directly as the linear
  physical sector** (`cdc_lba_to_sector` is the identity); DKADR is applied once,
  at overlay-WRITE time, by `mac-as -c` (mac-c `cdc_dkadr`). **Do NOT add DKADR
  here** — that double-converts.
- The backing image is likewise **linear**: sector `S` occupies bytes
  `[S*512, +512)`, raw big-endian ND word order.

---

## 6. Transfer / test-mode / interrupt behaviour (emulator)

1. On an `LCW` write with **activate** (bit 2): decode operation from bits 11-12,
   clear the error bits, set `active`+`transferOn`, compute
   `wordOffset = sector * 256`, bounds-check the surface (out-of-range → `errorOr`
   + `addressMismatch`), then perform the operation:
   - **read** (00): disc → core (`Device_DMAWrite`) — the overlay load path.
   - **write** (01): core → disc (`Device_DMARead`), persisted via the backing file.
   - **read-parity** (10): no memory transfer; CRC re-check — always succeeds here
     (no injected faults).
   - **compare** (11): compare core vs disc word-by-word; mismatch sets
     `compareError` (bit 10) + `errorOr` (bit 4). No memory transfer.
2. **Test mode** (control bit 3) — **[VERIFIED - MANUAL-N10 p.14]**: a Read
   Transfer with Test returns **pre-wired** data — **even words 125252** (octal,
   `0xAAAA`), **odd words 052525** (octal, `0x5555`) — and requires the block
   address register to be **125252**. A test read with a different block flags
   `addressMismatch`. `IOX 506` in test mode returns the loaded block address
   (Read Block Address, **[MANUAL-N10 p.13 / MANUAL-N100 p.189]**).
3. Queue a delayed completion (`Device_QueueIODelay`). The callback clears `active`
   and, returning `true`, raises the level-11 completion interrupt. TSS mostly
   *polls* `RST` via `DWAIT`; the interrupt is modelled for correctness (and
   asserted by the test). **[INFERRED]** the manual gates the CPU interrupt on
   control bits 0/1; TSS never enables them, so we always signal completion
   (matching drum/SMD) — invisible to TSS.
4. `Ident` returns IDENT `01` and clears its pending interrupt bit.

---

## 7. Backing image (`--cdc=FILE`)

- CLI: `--cdc=FILE`, plumbed like `--drum`: `Config_t.cdcFile`,
  `CdcDevice_SetBackingFile()` called before device creation.
- Raw big-endian 16-bit words; sector `S` at byte `S*512`. Loaded on attach; a
  larger file grows the surface. WRITE transfers are persisted on exit
  (`Cdc_Destroy`), like the drum.
- Default in-memory surface = **16384 sectors (8 MiB)**, covering the max overlay
  physical sector (`72 * 0o255 = 12456`).

---

## 8. Registration — CONDITIONAL on `--cdc`

The device is only instantiated when a `--cdc` image is given
(`src/frontend/nd100x/nd100x.c`, `initialize()`). It is deliberately **not** in
`DeviceManager_AddAllDevices()`. With no `--cdc`, address 0500 stays free —
reserved for a future Winchester controller sharing the 500-507 slot.

---

## 9. Validation results

- **Build** (WSL, DAP `_DEBUGGER_ENABLED_` enabled): links warning-free; only
  the pre-existing `screenmenu.c` HDLC format-truncation warnings remain (not in
  the CDC files). Command:
  `sed 's/\r$//' build.sh | bash -s -- --build-dir build-linux`.
- **ctest**: `5/5` pass; `cdc_tests` = **67 assertions passed, 0 failed** (grew
  from 36). New coverage: register offsets/names, control-word decode (activate /
  test / operation 00-11 / unit select / core-address bits 5-6), status-word
  union bits (active/onCylinder/errorOr/transferComplete), RCA two-read
  (low-16 then high-8), test-mode self-test (even 125252 / odd 052525, wrong-block
  error, SEEK-reads-block-address), COMPARE match/mismatch, READ-PARITY no-op —
  plus every prior assertion (register writes, OVLAY*2+OVDK read, byte-offset
  math, busy→clear + level-11 interrupt + Ident, WRITE round-trip, device clear,
  bounds/word-count-overrun → ERR, backing-file persistence).
- **Boot regression** (`--trace`, `--start=000301`, `--max-instr=500000`,
  `--cdc=…/tss-cdc.img`): disc reads still fire — `grep -c 'IOX 50'` = **27**,
  distributed as `501×3 503×3 507×3 505×3 504×15` = **3 clean overlay transfers**
  (load core/block/word-count, activate, then status polling). No regression into
  the old retry loop.
- **Runtime**: `CDC disc device created: CDC DISC 500 ident 1 level 11 (16384
  sectors, 4194304 words surface)`.
