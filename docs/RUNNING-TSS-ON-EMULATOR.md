# Running NORD TSS 3.0 on an ND-100 Emulator — Feasibility

Investigation date: 2026-07-20. Scope: what it would actually take to boot the
recovered NORD TSS 3.0 (`E:\Dev\Ronny\TSS\src\TSS1..5.SYMB`) on an ND-100
emulator. Every claim is tagged **[VERIFIED]** (file + line cited),
**[INFERRED]** (reasoned from verified facts, not directly observed), or
**NOT FOUND / CANNOT DETERMINE**.

Bottom line up front: **NO-GO as a drop-in**, and the single biggest blocker is
NOT the instruction encoding — it is that **neither emulator emulates the mass
storage TSS drives (CDC/NCR disk controller, or the swapping DRUM at IOX 540)**,
and the default TSS build uses the NORD-1 `IOT` instruction which **both
emulators trap as illegal**. A path exists (Route B), but it is a multi-week
device-emulation + build-environment effort, not a "load and run".

---

## 1. The two emulators, and how to run them

### 1a. nd100x — C, native ND-100, the active BSD-porting emulator

- **Canonical path:** `E:\Dev\Emulators\ND\nd100x\` **[VERIFIED]** (dir listing;
  also the build referenced by `E:\Dev\Ronny\TODO\BSD-211-ND100\nd100x-debug-commands.md:5`).
- **What it is:** a from-scratch C99 ND-100/ND-110 CPU + I/O emulator with a
  DAP (Debug Adapter Protocol) debugger, a Glass web UI frontend and a native
  frontend. `README.md:57-70` **[VERIFIED]**. It is NOT a build target of
  RetroCore and NOT merely a DAP front-end — it is a separate, standalone
  emulator project (own git repo, `src/cpu`, `src/devices`, `src/machine`).
  **[VERIFIED]** (`src/` tree).
- **Build (Windows):** `E:\Dev\Emulators\ND\nd100x\build.bat` (w64devkit) or
  `build.sh`/`Makefile`/CMake on Linux/WSL. `README.md:179-183` notes BPUN,
  SMD, floppy boot and telnet "all work natively on Windows". **[VERIFIED]**.
  A pre-built binary exists on this machine under `build/bin/` **[INFERRED]**
  (the WSL path `~/repos/nd100x/build/bin/nd100x` is used in the BSD doc;
  the Windows `build/` dir is present but I did not run it).
- **Run a BPUN:** `nd100x -b bpun -i <file>.BPUN`. `README.md:213,237-239`
  **[VERIFIED]**. Boot types: `bp, bpun, aout, floppy, smd`
  (`README.md:213,249-253`).

### 1b. RetroCore — C#, the multi-machine emulator (also has an ND-100)

- **Canonical path:** `E:\Dev\Repos\Ronny\RetroCore\` **[VERIFIED]**
  (the `E:\Dev\Ronny\RetroCore` path in the task does not exist on this machine
  — **[VERIFIED]** `ls` failed). The legacy ND-100 code is under
  `Emulated.HW\ND\CPU\` **[VERIFIED]** (NDBus device files enumerated below).
- **What it is:** a large C# retro-emulation framework (C64, 68K, SPARC, …)
  with an ND-100/ND-500 machine and a rich DAP + chip-CLI debug surface.
- **Build/run:** standard `dotnet build`; the ND CPU + buses live in
  `Emulated.HW`. (The repo's `CLAUDE.md` is about doc placement + C# style,
  not a run recipe — **[VERIFIED]** I read it; no ND-100 run command there.)

### Which is the better host for TSS?

**nd100x [INFERRED].** Reasons, all verified below: (1) it already boots BPUN
paper tapes from the command line and has the DAP session prior-art with a real
ND MAC BPUN; (2) its paper-tape reader sits at IOX 400-403, exactly where TSS's
own boot loader `HLOAD` expects it; (3) it is the emulator with an active
low-level bring-up workflow (BSD kernel). RetroCore has *more* disk controllers
but none of them is TSS's controller either, so that advantage is moot.

---

## 2. Device support — the core question

### 2a. What TSS actually drives  **[VERIFIED from `src\TSS1.SYMB`]**

TSS's definable device numbers (header comment block, `src\TSS1.SYMB:22-42`):

| TSS symbol | CDC default (octal) | Meaning |
|---|---|---|
| `TT1..TT12` | 2,104,120,122,124,126,136,140,146,150,154,156 | console + 11 terminals |
| `DLP` | 167 | line printer |
| `DCR` | 42 | card reader |
| `DFP` / `DSP` | 7 / 17 | fast / slow paper-tape punch |
| `MT` | 134 | mag tape |
| `DRUM` device `DRM` | **540** | swapping drum (only if `DRUM`+`N10` marks set) — `docs\TSS-Analysis.md:145-154` |
| CDC/NCR disk | via `DVN` + `IOX RSR/RDR/WDR/WCR` offsets | mass storage — `src\TSS1.SYMB:1455-1511` |

Two I/O dialects in one source **[VERIFIED]** (`src\TSS1.SYMB:1453-1455`,
`1581-1590`): the default **NN10** build issues NORD-1 **`IOT`** for every
device; the **`"N10`**-guarded blocks issue ND-100 **`IOX`** instead. Counts:
TSS1 has 112 `IOT` vs 61 `IOX` **[VERIFIED]** (grep). So only the **N10 build**
speaks the ND-100 I/O the emulators understand.

### 2b. What nd100x emulates  **[VERIFIED from `src\devices\`]**

| IOX (octal) | Device | Source |
|---|---|---|
| 0300 / 0310 / 0320 … | Console + up to 11 terminals | `terminal\deviceTerminal.c:37-47` |
| 0400-0403 | Paper-tape reader | `README.md:294`, `papertape\devicePapertape.c:229` |
| 0410-0413 | Paper-tape punch | `papertapewriter\devicePaperTapeWriter.c:254`, `README.md:297` |
| 0430-0433 | Line printer (CDC 9380) | `lineprinter\deviceLinePrinter.c:182`, `README.md:300` |
| 01540 (or 0540 alt thumbwheel) | **SMD hard disk** | `smd\deviceSMD.c:1330-1342` |
| (thumbwheel) | RTC, Floppy PIO+DMA, HDLC | `devicemanager.c:159-224` |

**No CDC/NCR disk controller. No drum. No card reader. No mag tape.** [VERIFIED]
(device list is exactly the folders in `src\devices\`).

### 2c. What RetroCore emulates  **[VERIFIED from `Emulated.HW\ND\CPU\NDBUS\`]**

Terminal (IOX 0300+ groups, `NDBusTerminal.cs:27-35`), disc controllers
**Hawk (IOX 0500-0503)**, **SMD (IOX 01540)**, **Winchester (IOX 0500-0506)**,
**SCSI**, Floppy PIO+DMA, PapertapeReader (0400-0403, `NDBusPapertapeReader.cs:46`),
PaperTapeWriter (0410), LinePrinter, MagTape, RTC, HDLC, EthernetII, ND500IF,
Nord50, Octobus. Still **no CDC/NCR disk and no drum** — Hawk/SMD/Winchester/SCSI
are different controllers with different register models than TSS's `DVN`+RSR/RDR
scheme.

### 2d. Device verdict — "is HDD support missing?"

**Yes, effectively.** [VERIFIED reasoning]
- **Mass storage: NOT emulated.** TSS drives a CDC (or NCR) disk with its own
  register offsets and, optionally, a drum at IOX 540. nd100x has only SMD;
  RetroCore has Hawk/SMD/Winchester/SCSI. None matches TSS's controller. The
  "SMD at 540" coincidence is a **false match**: 540 is an SMD *disk* in
  nd100x (`deviceSMD.c:1340-1342`), whereas TSS 540 is a *drum* with a
  totally different register set (RCX/LCX/LBX/RSX/LCR/LWX, `TSS-Analysis.md:145-154`).
- **Terminals: present but wrong numbers.** Both emulators put the console at
  IOX 0300; TSS's CDC console is device 2, terminals at 104-156. A TSS build
  would have to be re-marked to the emulator's terminal numbers, or the
  emulator reconfigured. [INFERRED]
- **Paper-tape reader: genuine match.** TSS's own hardware boot loader `HLOAD`
  uses N10 `IOX 400/402/403` (`TSS-Analysis.md:280-281`), which is exactly
  nd100x's paper-tape reader at 0400-0403. This is the one clean overlap and
  it is the loader path, which matters for booting.

---

## 3. Loading and running a BPUN  **[VERIFIED]**

- **Mechanism:** `nd100x -b bpun -i FILE.BPUN`. The BPUN paper-tape format is
  self-booting: it carries an octal bootstrap that reads the binary block
  (base, count, words, checksum) into memory and then jumps to a start cell
  *inside the loaded image*. `README.md:237-239,294-295`;
  mac-c's writer documents the identical convention
  (`mac-c\mac.c:2568-2615` — 204 leading NULs, `BPUN_OCTAL_LOADER`, big-endian
  base/count, 16-bit additive checksum, loader ends `JMP I 164361`). **[VERIFIED]**
- **Prior art — a real ND MAC BPUN was booted and single-stepped on nd100x:**
  `docs\MAC-BPUN-Analysis.md:249-260` **[VERIFIED]** records a DAP session
  (2026-07-19, `nd100x --boot=bpun`) that found MAC's cold-start entry at octal
  **173724 (0xF7D4)**. Important caveat from that same note: `MAC.BPUN` is a
  **SINTRAN subsystem** — it drives the console via **`MON 2`**, not IOX, and
  "the MON shim is required". So the MAC.BPUN prior-art proves the *BPUN
  load+run+DAP loop works*, but MAC.BPUN itself needs SINTRAN MON services,
  which TSS (a standalone OS) does not.
- **`MAC.BPUN` location:** `D:\ND\BPUN\MAC.BPUN` (28389 bytes) **[VERIFIED]**.
  No saved emulator session / DAP config file was found beyond the prose record
  in `MAC-BPUN-Analysis.md` — **NOT FOUND** (searched TSS, NDInsight, nd100x).

---

## 4. Route A vs Route B

### Route A — mac-c emits TSS as a BPUN, load it, run it

mac-as **can** emit both a memory image and a BPUN today. Ran
`mac-c\check_bootable.sh` (read-only) **[VERIFIED, 2026-07-20]**:
- Image covers **000000-033761 (14322 words)**, 64% non-zero; **location 7 =
  050003** (`LDT *+3`, the correct boot vector). So the resident core assembles
  and the boot cell is right.
- **Blocker (ii) is real and fatal as-is:** `)9MOVE`, `)SOVER`, `)8DUMP` are
  **"ACCEPTED AND IGNORED"** by mac-c (`check_bootable.sh` step 5, confirmed by
  `grep` of `mac.c` — those command names are not implemented). TSS3 uses
  `)9MOVE`/`)SOVER` 5× and TSS5 2× **[VERIFIED]**. In the real build these
  *write the TSS3-5 overlays to disk at assembly time* (`docs\BUILD-TSS.md:74-78`).
  mac-c never writes them, so **the overlays do not exist anywhere** — not on a
  disk, and (because the overlay bodies share addresses via `ROVER`) not
  reliably in the linear image either. At runtime TSS's overlay loader would
  fetch garbage.
- **Blocker (iii):** the self-save `7/ LDT*+3; SYSSV; CORLD` writes the 16K
  image to disk then falls into `INIT` (`BUILD-TSS.md:99-103`). Starting the
  BPUN at 7 would run `SYSSV`, which immediately does disk I/O to a controller
  **that isn't emulated** → trap/hang. [INFERRED from device gap + verified
  boot vector].
- **Blocker (i) — instruction bits — is worse than "unvalidated": see §5.**
- **Could the resident core run without overlays?** **CANNOT DETERMINE** from
  static analysis. The resident TSS1+TSS2 monitor might reach its idle loop,
  but `INIT`/`SYSSV` touch the disk on the very first path, so a bare boot
  almost certainly faults before a usable prompt. [INFERRED]

**Route A verdict:** not viable without (a) implementing overlay writes in
mac-c or booting past them, (b) an emulated disk TSS can talk to, and (c)
closing the instruction-bit gap. It converts "run TSS" into "finish the
assembler + write a disk device". Low confidence.

### Route B — run real MAC/FMAC on the emulator and reproduce the build in-emulator

This is the historically faithful path: MAC assembles TSS *into memory on the
target machine*, `)SOVER` writes overlays to disk naturally, and `SYSSV` saves
the image — all "for free" because the real assembler is executing.
- **Pro:** we have `D:\ND\BPUN\MAC.BPUN`, it is byte-analysed, and it has been
  booted on nd100x with the cold-start entry pinned (§3). [VERIFIED]
- **Blocker 1:** MAC.BPUN is a SINTRAN subsystem needing a **MON shim** (MON
  0/1/2/43/65) — `MAC-BPUN-Analysis.md:257-260`. Feeding it the `ASSYSA`
  stream needs `)9ASSM` to open *named files*, and "How `)9ASSM` opens named
  files (no MON 50 in the image)" is an **open question** in that same doc
  (line 247). So even getting sources into MAC is unsolved. [VERIFIED]
- **Blocker 2 (the same device gap):** `)SOVER` and `SYSSV` write to TSS's disk
  — still not emulated. Route B only removes the *assembler* problem; the
  *mass-storage* problem is identical to Route A. [INFERRED]
- **Blocker 3:** need FMAC specifically? The scripts start with `FMAC` (a MAC
  variant); we have `MAC.BPUN`, not `FMAC.BPUN`. Whether MAC.BPUN suffices is
  **NOT FOUND**.

**Route B verdict:** more faithful and it reuses real, already-booting
software, but it is gated on (a) a working MAC MON/file-I/O shim and (b) the
same emulated disk. Medium-low confidence, higher fidelity than A.

### Recommendation

**Neither route yields a running TSS soon; the critical-path work is identical
for both: emulate a disk controller TSS can drive.** Given that, prefer the
groundwork that serves both:

1. **First concrete step (unblocks everything): decide the disk story.** The
   cheapest is to add a small **CDC/NCR-compatible disk device** (TSS's
   `DVN`+RSR/RDR/WDR/WCR register model, `src\TSS1.SYMB:1455-1511`) to nd100x's
   `src\devices\`, OR build TSS with `DRUM N10` and add a **drum at IOX 540**
   (register model fully documented, `TSS-Analysis.md:145-154`). The drum is
   the smaller, better-documented controller — likely the least-effort target.
2. Then pursue **Route B** (real MAC) for fidelity, falling back to **Route A**
   (mac-c BPUN) only if the MAC MON/file-I/O shim proves too costly — in which
   case mac-c must first learn `)SOVER`/`)9MOVE` overlay writes.

---

## 5. The instruction-bit validation gap — and a correction

**The premise that `reference\LIST1..5.SYMB` are "full MAC assembly listings
with code words" is FALSE.** **[VERIFIED]**

- I sampled the LIST files and searched for a location+word column
  (`^[0-7]+\s+[0-7]{6}`): **zero matches** in LIST1. The only 6-digit octals
  in the file are **inline data constants in the source** (e.g. `104311`,
  `160000` at `reference\LIST1.SYMB:1550,2499`), not a generated-code column.
- The list stream is a **verbatim echo of the source** — compare
  `reference\LIST1.SYMB:62` (`0/ JMP I *+3`) with `src\TSS1.SYMB:62`
  (identical). MAC's `)9ASSM …,LISTn,…` list output does not carry generated
  words. [VERIFIED]

**Consequence:** there is **no instruction-bit oracle anywhere in the archive.**
`ASYMB/BSYMB` are symbol *addresses* (word counts) only; the LIST files add
nothing on encoding. So the instruction-bit gap **cannot be closed by scoring
against LIST1-5** — that script cannot be written because the data isn't there.
I therefore did **not** create a scorer (it would be a non-functional
throwaway, against repo policy).

The only ways to validate TSS's emitted bits are: (a) run the real MAC on the
emulator and diff its resulting memory image against mac-as's image
(Route B doubles as the oracle), or (b) trust mac-c's per-instruction encoding
tests in `test_mac.c`, which already assert encodings for the MAC instruction
set but not against a TSS-specific golden. [VERIFIED: `CLAUDE.md` "instruction's
encoding, not just the corpus totals"; the corpus itself has no bit oracle.]

---

## 6. Concrete next steps, ranked

1. **Emulate a disk TSS can drive** (blocks both routes). Smallest target: a
   **drum at IOX 540** for a `DRUM N10` build (register model fully documented).
   Add it to `nd100x\src\devices\`. Everything else is downstream of this.
2. **Prove the resident core boots to *something*** on nd100x under DAP: build
   the **N10** variant with mac-as, emit a BPUN, `nd100x -b bpun` it, break at
   entry, single-step `INIT`, and see exactly where it first touches an
   unemulated device. This is a read-only DAP experiment and directly measures
   how far Route A gets. (Reuse `check_bootable.sh` to regenerate the image.)
3. **Resolve the MAC file-I/O question** (`MAC-BPUN-Analysis.md:247`) — how
   `)9ASSM` opens named files — to know whether Route B's real-MAC path is even
   openable. Trace the `cmd_9ASSM` handler in `MAC.BPUN` (Ghidra).
4. **Only if Route A is chosen:** implement `)SOVER`/`)9MOVE`/`)8DUMP` overlay
   writes in mac-c so TSS3-5 overlays actually reach a disk image.

## Reusable tooling

No new script was warranted (§5 explains why a LIST-scorer is impossible, and
the disk/device work is emulator-side, not analysis). The existing
**`E:\Dev\Ronny\TSS\mac-c\check_bootable.sh`** already reports image extent,
boot vector, and the ignored-overlay problem, and was used for §4 — that is the
reusable Route-A probe.

---

## First execution attempt — DRUM-N10 BPUN on nd100x (2026-07-20)

**Milestone [VERIFIED]:** the DRUM-N10 TSS image built by mac-c
(`Build/drum/tss-drum.bpun`, via `mac-c/build_tss_drum.sh`) loads into the
native-Windows nd100x and the CPU executes it. This is the first time the
recovered 1973 TSS code has run.

**It does NOT reach the drum (IOX 540).** No IOX-540 access occurs, so the
drum device is NOT the current blocker and has not been implemented yet.

**Where it diverges [VERIFIED from --trace]:**
- nd100x `-b bpun` starts at PC=0 and does NOT honour `--start` (tested
  `--start=025101`; still began at 0).
- Location 0 = `125003` = `JMP I 3` -> mem[3] = `016462`, landing inside the
  monitor save/restore routine at `GOVX=016456` (register-save block
  `SAVA/SAVX/SAVL/SAVST` at 016451-016455, from the DRUM symbol dump).
- The routine does `STX -22` (016477) writing `125003` into `SAVST`, then
  `JMP I SAVST` (016501) -> `125003`, which is far above the image top
  (033636). That region was never loaded, so execution runs off through
  zero memory (`STZ 0`) indefinitely.

**Diagnosis [INFERRED]:** starting a cold BPUN at address 0 makes TSS execute
its trap/monitor-return path with page-zero and runtime state uninitialised —
not TSS's cold-start sequence. The `125003` "return address" is just location
0's own vector word used as data. Entry point and boot method are wrong, and/or
page-zero is not set up the way the original bootstrap did it.

**Candidate real entry points (DRUM symbol dump):** `START=025101`,
`INIT=000301`. Could not test them: bpun mode ignores `--start`.

**Open, ranked next steps:**
1. Build nd100x under WSL with `WITH_DEBUGGER` to get DAP, then set PC to the
   real cold-start entry and single-step (the native Windows build has no DAP).
2. Establish TSS's actual boot/entry contract: how the absolute binary was
   loaded and at what address it was started (the "1560& bootstrap"), and what
   page-zero must contain before entry.
3. Only after it reaches a drum transfer: implement the drum device at IOX 540
   per docs/DRUM-DEVICE-SPEC.md.
4. Instruction-encoding correctness remains unvalidated (no LIST oracle exists);
   a divergence could also be a mac-c encoding bug, not just a boot-state issue.

## DAP bring-up rig working (2026-07-20, WSL build)

**[VERIFIED] The WSL nd100x build has DAP.** Built with
`./build.sh --build-dir build-linux` (strip CRLF from build.sh first);
binary `build-linux/bin/nd100x`; build log confirms
"_DEBUGGER_ENABLED_ flag". Run:
`nd100x --boot=bpun --image=tss-drum.bpun --debugger --port=4711`.
Connected via the dap-debugger MCP (debug_connect 127.0.0.1:4711).
Full control confirmed: read/write memory (phys:), registers, disassembly,
instruction breakpoints, continue, step.

**[VERIFIED] The image loads faithfully.** phys word 0 = 0125003 (JMP I 3),
word 3 = 0016456 (GOVX), word 7 = 0050003 (LDT *+3) - all match the mac-c
image and the DRUM symbol dump (GOVX=016456).

**[VERIFIED] --debugger does NOT halt at entry.** It free-runs from PC=0 and
only freezes when a DAP client attaches; on attach the CPU is already at
PC=016456 (GOVX), i.e. it has already executed location 0's `JMP I 3` -> GOVX.
Consequence: patching page-zero (e.g. word 3 -> START=025101 to redirect the
entry) AFTER attaching is too late - the jump already happened. The redirect
theory is therefore UNTESTED, not disproven.

**Next step to get a clean PC=0 start (ranked):**
1. Make nd100x actually halt at entry before executing (a stop-at-load), or
   drive the DAP restart request and patch before the first continue.
2. Better: have mac-c emit the BPUN with an autostart/start address of
   START=025101 (if nd100x honours the BPUN start field), so location 0's
   trap-vector path is never taken as the cold start. Verify what start
   address mac_write_bpun encodes and whether nd100x reads it.
3. Establish TSS's real boot contract from the source: is the absolute binary
   entered at START, and what must page-zero contain first.

Still [INFERRED]: that START=025101 is the cold-start entry (it is IOF/ION +
subroutine-call init code, which fits). Not yet executed.

## BOOT-ENTRY FIXED — TSS now runs (2026-07-21)

**Root cause [VERIFIED].** nd100x boots a BPUN by loading its single block and
setting the start PC from the loader. For BOOT_BPUN, `program_load()`
(machine.c) overwrites `STARTADDR` with `LoadBPUN()`'s return (the block base =
0 for TSS), then `gPC = STARTADDR`. So TSS was entered at **address 0**, which
is a trap vector (`JMP I 3` -> `GOVX`, the monitor return) executed with an
uninitialised save block -> ran off into zero memory. The BPUN's own autostart
(the octal bootstrap ends `JMP I 164361`) is useless here: TSS's image
(0-033636) does not cover cell 164361, and nd100x does not run that bootstrap
anyway. `--start` was silently ignored because `program_load` clobbered it.

**TSS's real cold-start [VERIFIED].** `ISTRT=025076` (TSS2): `SAA 1; STA I
(IDEV; STA I (ODEV` then falls into `START=025101` -> `LEV2` -> INTDS / init
buffers / INTEN -> ... -> `LOGON=032735`. `LEAVE, LDA (START` (TSS1:4342)
confirms START is the system entry.

**The fix.** nd100x now honours an explicit start address over the BPUN default:
after `program_load()`, `if (config.startAddress != 0) { STARTADDR = ...; gPC =
... }` (src/frontend/nd100x/nd100x.c). Set it via `--start=025076` or the config
`start = 025076`. `tss.cfg` carries it.

**Result [VERIFIED].** Starting at 025076, TSS executes its real cold-start:
`025076 SAA 1`, `025077 STA I (IDEV`, ..., writes its page-zero vectors from
PC=025302/025275, and after 1,000,000 instructions is running in the
monitor/interrupt region (PC=13, live registers A=7 D=060140 X=016710 B=016706
L=026444) -- NOT running off into zeros. This is the first time TSS boots and
executes its own system code.

**Not yet reached:** a visible `LOGON` prompt. Open questions for the next phase:
where it settles/loops, the `RMODE` cold-vs-warm branch in START, console I/O,
and whether it needs the drum. The drum device is in place but not yet exercised.

**Build note [VERIFIED]:** `tools/mkptypes/mkptypes` is a committed Windows PE;
the WSL build cannot exec it, and CMake's configure-time `make -C tools/mkptypes`
does not rebuild it because the PE is newer than the source. Rebuild the Linux
one when a WSL build fails with "Exec format error":
`cd tools/mkptypes && cc -O2 -o mkptypes mkptypes.c`.

## BPUN GENERATION FIXED — autostart in the tape (2026-07-21)

The nd100x `--start` override (previous section) was a host-side workaround.
The proper fix is in the tape: mac-c now records TSS's cold-start as the BPUN
autostart, so nd100x enters TSS correctly from the image alone.

**How nd100x picks the start [VERIFIED].** `LoadBPUNStream` (src/ndlib/load_bpun.c)
parses the octal-ASCII preamble: `header->start` = last `/`-opened location;
on `!`, `boot = (loadAddress == start) ? lastValue : loadAddress`. The archived
loader mac-c reproduces ends `164316!`, and `164316` also equals `start`, so
`boot = lastValue = 0` — TSS ran from 0. (Verified: `Start: 164316 Boot: 000000`.)

**The fix [VERIFIED].** `mac-as -e ENTRY` (ENTRY = octal address or symbol name)
makes `mac_write_bpun_range` replace the loader's final `164316!` autostart token
with `<entry>!` (even-parity octal ASCII). Since `entry != start`, nd100x reads
`boot = entry`. `build_tss_drum.sh` now passes `-e ISTRT` (025076). With entry==0
the tape stays byte-identical to the archived MAC.BPUN.

**Result [VERIFIED].** `nd100x --boot=bpun --image=tss-drum.bpun` (NO `--start`)
reports `Boot: 025076` and executes `025076 SAA 1 ...` — TSS enters at its
cold-start from the tape alone, and runs identically to the `--start` case
(page-zero writes from PC=025302/025275). `tss.cfg` (now in `mac-c/`) needs no
`start=`. mac-c: 617/617 tests pass.

Caveat: this preamble autostart is what nd100x's loader consumes; it is not the
same mechanism as a real ND-100 running the octal bootstrap (which jumps through
cell 164361, absent from TSS's low image). For the emulator target this is the
correct fix; real-ND tape autostart for a low-loaded image is a separate concern.
