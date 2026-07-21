# Swapping-DRUM device for nd100x — implementation plan

Target: emulate the NORD TSS swapping drum at **IOX 540–547** in nd100x so the
DRUM-N10 TSS build can swap. Grounded in two specs already written:

- `docs/DRUM-DEVICE-SPEC.md` — what the TSS `XDRUM` driver programs (the wire protocol).
- `docs/ND100X-DEVICE-GUIDE.md` — how nd100x devices are structured/registered.

**Key realisation:** the drum is a *cut-down SMD controller*. Its register map
(541 load-core, 543 load-block, 544 read-status, 545 load-control, 547
load-word-count) is offset-for-offset identical to `deviceSMD`, and its two
status bits (DVA = bit 2, ERR = bit 4) are a subset of SMD's (`active` = bit 2,
`hardwareError` = bit 4). **`src/devices/smd/deviceSMD.c` is the template.**

---

## 0. Sequencing — this is independent of the boot-entry bug

TSS currently dies at the location-0 → GOVX entry issue *before* it reaches any
IOX 540 (see `RUNNING-TSS-ON-EMULATOR.md`). That does **not** block this work:
the drum device is implemented and **unit-tested against the wire protocol**
with no dependency on TSS booting. The two tracks run in parallel; they meet at
the integration test once the entry issue is fixed.

---

## 1. Files to create / edit

New (mirroring `src/devices/smd/`):

- `src/devices/drum/deviceDrum.h` — registers, status/control unions, device struct.
- `src/devices/drum/deviceDrum.c` — factory + handlers + transfer engine.

Edit:

- `src/devices/devices_types.h` — add `DEVICE_TYPE_DRUM`; include `deviceDrum.h`.
- `src/devices/devicemanager.c` — add a `CreateDevice` case and a
  `DeviceManager_AddAllDevices` line for the drum at `startAddress=0540,
  endAddress=0547`.
- `src/devices/CMakeLists.txt` — the three edits the guide lists (GLOB / mkptypes
  line / include dir) so `drum/` is compiled.
- `src/frontend/nd100x/nd100x.c` — a `--drum=FILE` CLI option (see §5).

## 2. Register model (from DRUM-DEVICE-SPEC.md, [VERIFIED] against TSS1.SYMB)

Dispatch in `Drum_Read`/`Drum_Write` on `address - startAddress` (0..7):

| off | IOX | dir | name | action |
|---|---|---|---|---|
| 0 | 540 | read  | RCX | read core address (defined, never issued by XDRUM) |
| 1 | 541 | write | LCX | load core (memory) address |
| 3 | 543 | write | LBX | load drum block address |
| 4 | 544 | read  | RSX | read status register → A |
| 5 | 545 | write | LCR | load control word → **triggers the transfer** |
| 7 | 547 | write | LWX | load word count |

Direction rule (verified): IOX device LSB = 0 → input (read), LSB = 1 → output
(write). Matches every register's parity above.

**Status register (RSX / 544)** — implement as a `uint16_t` union like SMD's,
but only these bits are load-bearing:
- bit 2 `DVA` (0o020) — device active/busy.
- bit 4 `ERR` (0o040) — inclusive OR of errors.
Everything else 0 for now. Return the full 16-bit word (TSS reads it into X).

**Control word (LCR / 545)** — decode per the drum spec (NOT SMD's layout):
`(function << 13) | (coreAddrBits16_17 << 5) | 7`
- bits 0–2 = `7` → enable interrupt + activate.
- bits 5–6 = core address bits 16–17 (high bits of the DMA address).
- bits 13–15 = function: 0 read, 1 write, 2 read-test, 3 compare.
On error the driver writes control = `4` (device clear) — handle as a reset.

## 3. The transfer engine (`ExecuteGO`, on LCR write)

Mirror SMD's `ExecuteGO`. On a control-word write with the activate bits set:

1. Assemble the 18-bit core address: `coreAddr = LCX | (ctrl.bits16_17 << 16)`.
2. Decode the drum block address (LBX): **sector = bits 15–11, track = bits
   10–0** ([VERIFIED] from the driver's comments TSS1.SYMB:3808–3812). Linear
   word offset into the drum image:
   `wordOffset = (track * SECTORS_PER_TRACK + sector) * WORDS_PER_SECTOR`
   with `SECTORS_PER_TRACK = 32`, `WORDS_PER_SECTOR = 64` ([INFERRED] — 32×64 =
   2048 words = one TSS page; flag in a comment, confirm via DAP).
3. `wordCount = LWX`. One hardware start moves ≤ one track (2048 words); TSS
   splits larger requests itself, so no splitting needed in the device.
4. Function 0 (read): for i in 0..wordCount-1, `Device_DMAWrite(coreAddr+i,
   drumImage[wordOffset+i])`. Function 1 (write): `drumImage[wordOffset+i] =
   Device_DMARead(coreAddr+i)`. Function 2 (read-test): read, no DMA store.
   Function 3 (compare): read + compare, set ERR on mismatch.
5. Set `DVA` (busy), then `Device_QueueIODelay(self, IODELAY_*, DrumEnd, unit,
   self->interruptLevel)` exactly like SMD — the delayed callback clears `DVA`
   and raises the completion interrupt.

## 4. Completion / interrupt (from spec: interrupt-driven, poll-on-entry)

- `DrumEnd` callback (like `SMDReadEnd`): clear `DVA`, call
  `Device_GenerateInterrupt` on `self->interruptLevel` so `Drum_Tick` returns
  the bit; TSS's handler is re-entered and polls `DVA` (now clear) → done.
- `Drum_Tick` returns `self->interruptBits` (copy SMD).
- `Drum_Ident(self, level)` returns `self->identCode` and clears the bit (copy SMD).

**THE TWO UNKNOWNS — interrupt level and IDENT code for device 540.** The TSS
source enables the interrupt but binds no level/IDENT that either spec agent
could find. Plan:
- Make `DRUM_INT_LEVEL` and `DRUM_IDENT_CODE` named constants in `deviceDrum.h`
  with a documented best-guess default (candidate: level 11 or 13, the ND mass-
  storage levels; IDENT to be confirmed).
- **Determine them empirically via DAP** once the boot-entry fix lets TSS reach
  the drum: set an instruction breakpoint on the XDRUM interrupt-enable path,
  read which level `MST PID`/the control word arms and what IDENT the handler
  expects, then set the constants to match. Do NOT ship a guess as fact — the
  constants carry a `// PROVISIONAL until confirmed on DAP` comment.

## 5. Backing store (avoid the FLOPPY-branch gotcha)

The guide warns the machine block-image callbacks key the backing file on
`device->type`, so a new type falls into the FLOPPY branch. **The drum owns its
own `FILE*`** instead:
- `--drum=FILE` CLI option; if omitted, an in-memory zeroed image.
- Size = `DRMSZ` pages × 2048 words × 2 bytes. `DRMSZ = 0o2000 = 1024` pages
  (default from the build) → 4 MiB. Zero-filled is correct: the swap drum starts
  empty and TSS writes pages before reading them.
- Keep the image in a `malloc`'d `uint16_t[]`; flush to the file on close if a
  file was given. No parity, raw 16-bit words, host-endian internally.

## 6. Testing — independent of TSS booting

nd100x has `tests/`. Add `tests/test_drum.c` (or extend the device tests)
asserting the wire protocol directly, no TSS image needed:
1. Write LWX/LCX/LBX, then LCR with function 0; assert the emulated DMA wrote
   the expected drum bytes to the expected core addresses.
2. Function 1 (write) round-trips: write core→drum, read drum→core, compare.
3. Addressing: a known (sector,track) maps to the expected linear offset.
4. Status: DVA set on start, cleared after the queued completion; ERR on a
   compare mismatch (function 3).
5. Completion raises an interrupt on `DRUM_INT_LEVEL`; `Drum_Ident` returns the
   code and clears the bit.

Every claim in comments cites `DRUM-DEVICE-SPEC.md`; provisional values
(WORDS_PER_SECTOR, level, ident) are marked as such.

## 7. Build & validate loop (WSL, DAP port 1777)

```
# build (separate dir, keeps the Windows build intact)
wsl -d Ubuntu -- bash -lc "cd /mnt/e/Dev/Emulators/ND/nd100x && \
  sed 's/\r$//' build.sh | bash -s -- --build-dir build-linux"
# unit tests
wsl ... ctest / the drum test binary
# integration (after boot-entry fix): boot DRUM-N10 BPUN with DAP on 1777
build-linux/bin/nd100x --boot=bpun --image=.../tss-drum.bpun --debugger --port=1777
```
DAP on **port 1777** (project convention, not 4711). Must stay warning-free.

## 8. Risks / open items

- **Interrupt level + IDENT are provisional** until DAP-confirmed (§4). Highest-
  risk unknown; but the read/write DMA path is testable without them.
- `WORDS_PER_SECTOR = 64` and the exact block-address bit split are [INFERRED]
  from driver comments — confirm against a live XDRUM transfer.
- The drum is reached only after the boot-entry bug is fixed; until then,
  validation is unit-test-only (which is sufficient for the device itself).
- `Device_QueueIODelay` timing constant: reuse an SMD-like delay; TSS polls DVA
  so exact timing is not critical, but too-fast completion before the driver
  takes its BUSY exit could race — match SMD's delay.

---

## STATUS: implemented + unit-tested (2026-07-20)

**Done and validated:**
- `src/devices/drum/deviceDrum.{c,h}` — register protocol (541/543/544/545/547),
  block-DMA transfer engine, sector/track addressing, DVA/ERR status,
  interrupt-on-completion. Modelled on `deviceSMD.c`.
- Wired in: `devices_types.h` (DEVICE_TYPE_DRUM), `devicemanager.c` (create case
  + AddAllDevices), `devices_protos.h`, `src/devices/CMakeLists.txt`.
- Builds warning-free under WSL (`build-linux`) with DAP. Registers at runtime:
  `DRUM 540 ident 24 level 11 (2097152 words surface)`.
- `tests/test_drum.c` — 16 assertions (write/read/compare/bounds/status/
  interrupt/extended-address), all pass. Full `ctest` suite 4/4 green.
- Build-infra fix: compiled a **Linux `mkptypes`** from `tools/mkptypes/mkptypes.c`
  (the committed one was a Windows PE) so the WSL build can regenerate
  `devices_protos.h`. Windows unaffected (it uses `mkptypes.exe`).

**Interrupt level = 11 [CONFIRMED by user]** (all ND HDD share SMD's level).

**Still PROVISIONAL (confirm on DAP once TSS reaches the drum):**
- IDENT code `024` — chosen unique, not derived from TSS.
- `DRUM_WORDS_PER_SECTOR = 64` — inferred (32×64 = one 2048-word page).
- Control bit 0-2 split — treated as "==7 → go+interrupt" per spec; exact split
  not distinguished by the source.
- The drum has NOT yet been exercised by real TSS code — blocked on the
  boot-entry issue (`RUNNING-TSS-ON-EMULATOR.md`). Validation so far is
  unit-test (wire protocol) only.
