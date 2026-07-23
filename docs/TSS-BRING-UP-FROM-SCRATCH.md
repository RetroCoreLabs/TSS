# Bringing up NORD TSS 3.0 from scratch

A complete, ordered walk-through of taking the recovered 1973 NORD TSS 3.0
source to a running system on the `nd100x` ND-100 emulator: build the assembler,
build the OS, build the emulator, apply the fixes required to *run* (not just
assemble), boot, and drive it toward the `LOGON` prompt and first-user creation.

This is the "if you started today, do this" document. It ties together
`docs/BUILD-TSS.md`, `docs/RUNNING-TSS-ON-EMULATOR.md`, `docs/CDC-DISC-DEVICE.md`,
`docs/LOGON-PATH.md`, `docs/SYMBOL-CHANGES.md`,
`docs/TSS-CONTROL-PANEL-SWITCHES.md`, and the `tss-boot` skill.

Everything is **VERIFIED** against source/live traces unless marked
**[INFERRED]**. Source references are `file:line`.

---

## 0. The mental model

Two facts drive everything:

1. **The 1978 golden builds were never executed** — they only produced
   *symbol-table dumps* (`reference/ASYMB.SYMB`, `BSYMB.SYMB`). Running TSS on
   `nd100x` is the **first execution ever**, so it exposes bugs a dump can't:
   undefined variables written through null literals, a wrong paging model, and
   device-encoding mistakes. "Correct for the dump" ≠ "correct for a run."

2. There are **two programs in `archive/`** and two hardware targets. `nd100x`
   runs the **NORD-10 / DRUM** build. Conditional assembly marks `"N10` /
   `"NN10` select NORD-10 vs NORD-1 code throughout; get the target right or you
   run the wrong half.

Layout: `archive/` originals (never edit) → `src/` clean source → `reference/`
golden oracle → `Build/` output (disposable) → `mac-c/` the C99 MAC assembler.

---

## 1. Prerequisites

* A POSIX toolchain. On this machine everything runs through **WSL Ubuntu**;
  this is Windows — drive it from PowerShell or the Bash tool, never assume
  Linux paths for editing. **PowerShell→WSL quoting mangles `$vars`/awk in inline
  one-liners — always write a script file and run it.**
* `nd100x` checkout. **The authoritative copy is the WSL-native one:**
  `~/repos/nd100x` (`= /home/ronny/repos/nd100x`). A second copy exists at
  `E:\Dev\Emulators\ND\nd100x` but it **diverges** (`config.c`, `nd100x.c`
  differ) — build and edit the WSL copy, which is where the CDC/OPR work lives.
* `nd100-dis` (`/usr/local/bin/nd100-dis`) for disassembling compiled overlays.

---

## 2. Build the pieces (in order)

All commands from WSL.

```bash
# 2a. mac-c: the C99 reimplementation of the ND MAC assembler + its test suite
cd /mnt/e/Dev/Ronny/TSS/mac-c && make test          # must be 0 failures

# 2b. Assemble the runnable NORD-10/DRUM TSS + bootable BPUN + CDC overlay disc
cd /mnt/e/Dev/Ronny/TSS/mac-c && ./build_tss_drum.sh
#   -> Build/drum/{tss-drum.bpun, tss-drum.img, tss-cdc.img, DSYMB.SYMB}

# 2c. Re-score against the golden oracle after ANY src/ edit (must not regress)
cd /mnt/e/Dev/Ronny/TSS/mac-c && ./build_tss_assysa.sh \
  && ./compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB | grep 'exact matches'
#   Baseline: 679/693 (A), 675/689 (B); the unmatched are macro names + QOV1C.

# 2d. nd100x emulator (WSL copy)
cd ~/repos/nd100x/build && cmake --build . -j4 && ctest
#   If mkptypes is a stale Windows PE ("Exec format error"):
#     cd ~/repos/nd100x/src/cpu/tools/mkptypes && cc -O2 -o mkptypes mkptypes.c
```

Outputs used by the boot:

| File | Role |
|------|------|
| `Build/drum/tss-drum.bpun` | bootable TSS image (BPUN), entry `INIT = 000301` |
| `Build/drum/tss-drum.img`  | swapping-drum backing image (IOX 540) |
| `Build/drum/tss-cdc.img`   | CDC system-disc image (IOX 500-507) — **overlays only** (see §7) |
| `Build/drum/DSYMB.SYMB`    | symbol→address table; **always resolve addresses from here** |

> **Address caveat:** any `TSS2+` source edit shifts every later symbol by +1
> word. Never hard-code octal addresses across a rebuild — read them from
> `DSYMB.SYMB`.

---

## 3. The fixes required to RUN (not just assemble)

Before these, TSS never reached user code. All three were needed **together**.
They are recorded in `docs/SYMBOL-CHANGES.md`, `docs/LOGON-PATH.md`,
`docs/CDC-DISC-DEVICE.md`.

### 3.1 `EXRGP` — a resident variable missing from the recovered source
`EXRGP` (an EXR register-save pointer, LEV14 family) is referenced but **never
defined** in `src/`, so its literal `(EXRGP` resolved to **0**. The unconditional
`STZ I (EXRGP` at `S10` (`TSS1.SYMB:3181` region) then stored 0 **to address 0**,
destroying the GOVER page-0 dispatch vector — so no overlay ever loaded.
**Fix:** restore `EXRGP, 0` in the `%LEV14 VARIABLES` block of `TSS2.SYMB`
(`src/TSS2.SYMB:106-124`), **before `SAVEX`**, gated `"N10` so the NN10 golden
dumps stay byte-identical. Value 0 (a pointer, cleared per context at `S10`).

### 3.2 MMS1 paging (not MMS2)
TSS is NORD-10 code: it programs **Paging System I** — single 16-bit page-table
words to `0177400`, then `PON` (`TSS1.SYMB:306-318`). `nd100x` defaulted to MMS2
(16 page tables, 32-bit entries). **Fix:** `nd100x` has a per-machine `mms`
setting; use **`--mms1`** for TSS. SINTRAN keeps MMS2.

### 3.3 CDC disc control-word operation decode — the subtle one
The overlay read is issued by `DKOP`/`DKTR` (`TSS1.SYMB:3458`, "CDC N10").
With the `mac-c` **SHR shift bug fixed**, `DKTR` now builds the *manual-correct*
read control word **`000004`**: the device **operation** is a plain **2-bit field
at control-word bits 11-12** (00 read / 01 write / 10 read-parity / 11 compare),
per the ND manual (ND-11.008.01 / ND-06.016.01).

`nd100x`'s CDC device (`src/devices/cdc/deviceCDC.{c,h}`) decodes exactly that:
`op = control.bits.deviceOperation` (bits 11-12). A read (`000004`) → op 0 →
disc→core DMA into `ROVER`.

> **History / trap:** an earlier device build decoded the op as an `(op+1)<<12`
> field at bits 12-14 (read word `010004`). That was an **artifact of the SHR
> compiler bug**, which had made `DKTR` emit `010004`. With SHR fixed, `DKTR`
> emits `000004`, and the `(op+1)<<12` model wrongly decodes it as op 3
> (compare) — the device sets `compareError`, never DMAs, and the boot hangs in
> `DWAIT` (status `062024`). The correct, current model is **bits 11-12**.

`docs/CDC-DISC-DEVICE.md` and the `deviceCDC.h` NOTE carry the full derivation.

---

## 4. Boot it

```bash
ND=~/repos/nd100x/build/bin/nd100x
D=/mnt/e/Dev/Ronny/TSS/Build/drum
T=$(mktemp -d); cp "$D/tss-drum.img" "$T/drum.img"; cp "$D/tss-cdc.img" "$T/cdc.img"

"$ND" --mms1 --boot=bpun --image="$D/tss-drum.bpun" \
      --drum="$T/drum.img" --cdc="$T/cdc.img" \
      --start=000301          # add --trace 2>trace.txt for diagnostics
```

* `INIT = 000301` (`--start=000301`) is the cold-start entry; there is no usable
  autostart cell in the BPUN.
* Always run on **copies** of the images (the run may write to them).
* `--trace` writes a per-instruction disassembly to **stderr**. Register values
  are the state at instruction **fetch**.

### What a healthy boot does (VERIFIED live)
`INIT` → scheduler `LEV5` → `S10` → dispatches level 2 `LEV2` → `LEV2` runs
`ISTK; IBUF; SXBRK`, prints a start-up banner via `ERMSG` (= `GOVER OV15`), then
falls through to `LOGON` (`032736`). Overlays load from the CDC disc into
`ROVER` (031400): the read control word is `IOX 505 = 000004`, and OV15's body
(`031400 005003 STA I 3`) executes. Multiple distinct `IOX 503` sectors are read
as more overlays load; execution reaches `LOGON`.

### Diagnostic greps (from `trace.txt`)
```
 IOX 505       control word (000004 = read)     IOX 503   disc block address
 IOX 501       DMA core address (031400=ROVER)   IOX 504   CDC status
 IOX 305       console character out             P=032736  LOGON reached
```
`MACTRACE=1` (mac-c side) reports which source stream ended each build — the tool
for "the build silently stopped early."

---

## 5. The `@ENTER` prompt and first-user creation

Once `LOGON` is reached, TSS prints its login prompt and the console idles in the
background loop `LEV0` (`006104`) waiting for a keystroke (a PIL-0 wait, not an
error).

**The login prompt is `@ENTER `** — the message `MS1` in the LOGON overlay
(`src/TSS4.SYMB:539`, `MS1, '$$@ENTER \ '`; `$` = CR/LF, `\` = terminator). Press
a key and TSS reads a user name (`src/TSS4.SYMB:546-559`), looks it up in the
disc user directory (`USRTB`/`USRDK` via `XDISK`), and — if not found — re-prints
`@ENTER `.

On a **virgin** system there are no users, so every attempt re-prompts. Users are
created at runtime, not by the disc writer:

**Set the operator's panel to `131313` (octal) at cold start** → `S10` dispatches
`SINIT` → `CRUSE` creates user **`SYSTEM`** (see
`docs/TSS-CONTROL-PANEL-SWITCHES.md` §2.1). On `nd100x`:

```bash
"$ND" --mms1 --opr=131313 --boot=bpun --image="$D/tss-drum.bpun" \
      --drum="$T/drum.img" --cdc="$T/cdc.img" --start=000301
```

or interactively: **F12 → [6] Control Panel Switches → `S`** (sets `131313`),
then let it run through cold start.

**VERIFIED:** with `--opr=131313`, `TRA OPR` returns `131313` at the `S10`
cold-start site and the `SUB (131313` yields 0, so the branch falls through to
the SINIT (create-SYSTEM) dispatch.

---

## 6. The overlay-on-disc subsystem (reference)

* Overlays live on the **CDC system disc** (IOX 500-507), **not** the drum (540).
  Registers: `500` RCA, `501` LCA(core), `502` RSECT, `503` LBA(block), `504`
  RST(status), `505` LMR/LCW(control), `506` SEEK, `507` LWC(count). IDENT 1,
  level 11.
* Logical sector `= OVDK + 2*OVLAY` (OVDK `0160` non-DEBUG); each overlay = two
  256-word sectors, read into `ROVER` and `ROV4 = ROVER+400`, op READ.
* **Physical placement = `DKADR(logical)`**, the driver's logical→physical map.
  As-built (SHR fixed): `DKADR(L) = 32·floor(L/12) + 2·(L mod 12)` — a base-12
  "track·32 + 2·sector" repack (`mac-c/mac.c cdc_dkadr`, verified live). `mac-c`
  writes each overlay at that physical sector; the `nd100x` CDC device is a dumb
  linear-by-physical-sector store.

Details: `docs/OVERLAY-DISC-SPEC.md`, `docs/CDC-DISC-DEVICE.md`.

### 6.1 What actually writes the CDC disc during bring-up (VERIFIED)

The overlay-image writer in `mac-c` (`mac_write_cdc_disc`, `mac.c:2965`) writes
**overlays only**. The disc's *system* structures (system image, free-space bit
table / MIB, user table, per-user tracks) are written by **TSS itself at
runtime**, by four distinct routines writing four distinct disc regions:

| Routine | Source | Trigger | Disc region written |
|---------|--------|---------|---------------------|
| `SYSSV` (SAVE SYSTEM) | `TSS1.SYMB:4108` | **bootstrap entry at absolute addr 7** (`TSS1.SYMB:70` `7/ ...; SYSSV; CORLD`) | 0o100 blocks of the resident system image (`XDISK` op 3), from disk addr `SYST` |
| `FCRUS` (create user) | `TSS5.SYMB:74` (overlay OV10) | `OPR=131313` → `SINIT` → `CRUSE` | **MIB user table** at `DKBIT+index` (`RUTBL`/`WUTBL`, `TSS2.SYMB:3048/3070`) |
| `IUSER` (init user) | `TSS5.SYMB:127` | called by `FCRUS` | the new user's **private directory track** (`RBLOC`/`WBLOK`, addr from `UNDK`) |
| `GTRK` (get track) | `TSS2.SYMB:2665` | called by `FCRUS` | the **free-space bit table (MIB)** at `DKBIT` |

Note: `FINIT` (`TSS1.SYMB:4714`), called unconditionally at every cold start,
initialises the **in-core** file-system tables only (`BITBL`, `USTBL`, `PCTBL`,
`VCTBL`, …) via `SETB` — it performs **no disc I/O**.

Key disc addresses (`DKADR(L)=32·⌊L/12⌋+2·(L mod 12)`):

| Structure | Logical | Physical | In current image? |
|-----------|---------|----------|-------------------|
| Overlays | `0160`–`0257` | ~`0450`–`0716` | yes (that is all mac-c writes) |
| MIB / bit table + user table `DKBIT` | `050` | ~`0150` (104) | **address in range**, but region is **zero/uninitialised** |
| Login directory `USRDK` | `01460` | `04200` (2176) | **NO — past the 459-sector image & 512-sector surface** |

So the **create-user MIB write lands in-range**, but there is no valid MIB there
to read/update (the image is zero except for overlays), and the **login lookup
reads `USRDK` at a sector that does not exist**. That is why `SINIT` can trigger
yet no usable login appears.

**[UNRESOLVED]** create-user (`FCRUS`/`RUTBL`/`WUTBL`) operates on the MIB user
table at `DKBIT` (~sector 104), while `LOGON` reads the login directory at
`USRDK` (~sector 2176). Whether these are meant to be the same structure at
different addresses, or `IUSER`/another path bridges them, has **not** been
resolved from static reading — it needs a live trace of a successful create. Do
not assume they are consistent.

---

## 7. Bring-up to a working login (the real sysgen)

The disc's system substrate is **not** written by `mac-c` (it writes overlays
only). It is laid down by TSS's own tools. The complete recipe:

### 7.1 Format the disc with `MINIT` (the real formatter — it EXISTS)

`src/MINIT.SYMB` is the **"MASS STORAGE INITIALIZATION PROGRAM"** — the standalone
CDC-disc MIB formatter (found 2026-07-21; previously undocumented). It is a
self-contained program with its own console + disc I/O. Behaviour
(`MINIT.SYMB:135-243`): prompts **FIRST DISK ADDRESS (NCR)**, **LAST DISK ADDRESS
(NCR)**, and **I/U/R**. `INITIALIZE` walks each track in the range, write/read-
verifies it, **sets its bit in the MIB (set = free/good)**, and writes the MIB
bit table to disc track **50 (`DKBIT`)**. `REGENERATE` rebuilds the map from the
existing user/UIB/file/track chains; `UPDATE` is a stub (`NOT IMPLEMENTED`). It
uses the **same `MASKS`/`DKBIT`** as the running system, so it is consistent by
construction. It builds **only the free-space bit table** — no users, no `USRDK`.

Assemble it (VERIFIED — 0 errors; the only "undefined" are the library marks
`&L`/`N10`/`CDC`):
```
cd mac-c
./mac-as -m CDC -m N10 -d IOT=160000 -d ACT=400 -d SKA=1000 -d PIN=2000 \
         -d SNI=0 ../src/MINIT.SYMB -b ../Build/MINIT.bpun -e MINIT -u
```
Run it over the **file-system range only** (`FIRST=FSYS=04470` … `LAST`=disc
end), so it marks file-system tracks free without freeing the system areas.
Track math: track = 8 sectors, MIB bit index = `disk_address / 8`, so
`FIRST=04470` marks track `0o4470/8 = 0o447` (295) onward free — exactly what
`GTRK` allocates from.

### 7.2 Create the first user (`SYSTEM`)

Cold-start with **`OPR=131313`** (`--opr=131313`) → `SINIT` → `CRUSE` allocates
SYSTEM's accounting entry + a track (`GTRK` now succeeds) + `IUSER`.

**Known gap (VERIFIED):** `SINIT`/`CRUSE`/`FCRUS` write the `DKBIT` accounting
table but **never write the `USRDK` login directory**, which is what `LOGON`
reads. Only the `CRUSR` *command* writes `USRDK` (`TSS4.SYMB:954`), and that needs
an existing SYSTEM login (chicken-and-egg). So a faithful bring-up must also seed
SYSTEM's `USRDK` entry — recommended fix: **patch `SINIT` to mirror `CRUSR`'s
`USRDK` write** (`SETB` the name into `USRTB`, `XDISK` op 3 to `USRDK`), gated
`"N10`, re-scored against the golden dumps. (This is likely an incompleteness in
the recovered `SINIT`, of the same class as the missing `EXRGP`.)

### 7.3 Log in

Reboot normally; `LOGON` reads `USRDK`, finds `SYSTEM` (password word 0 =
passwordless), and you are in.

### 7.4 Disc sizing

The CDC device **auto-sizes its surface to the backing image**
(`deviceCDC.c:124-142`) — so just provide a large enough zero-filled image (start
from `tss-cdc.img`, which has the overlays, and pad to cover the file-system
range; `FSYS` alone is at physical sector ~6288, so ~7-8k sectors). No device or
test change is needed.

### 7.5 Status / open risks

`MINIT` assembles to a working BPUN (done). Not yet executed end-to-end: running
`MINIT` against a sized disc, the `SINIT`→`USRDK` patch, and the create→login
debug. This is 1973 code being executed for the first time, so expect further
latent bugs at each step.
* **An interactive console is needed to see the prompt.** Use `--pipe` (keyboard
  from stdin, automation) or type into the local console; the TSS console is
  terminal 1 (IOX 300-307), which is **not** exposed via `--telnet`
  (telnet registers terminals 5-11 only).
* The trailing octal after `@ENTER ` seen in some runs is **not** part of the
  `MS1` string; its exact source is **not yet pinned** — do not assume it.

---

## 8. Fast index of the boot's key symbols (from `DSYMB.SYMB`)

| Symbol | Addr (oct) | Role |
|--------|-----------|------|
| `INIT` | `000301`  | cold-start entry (`--start`) |
| `LEV0` | `006104`  | idle/background wait loop |
| `LEV5` | `006324`* | scheduler/swapper |
| `S10`  | (resident)| cold-start dispatcher (reads panel `131313`) |
| `GOVX` | `016457`  | overlay dispatcher |
| `DKTR` | `007651`  | CDC overlay-read builder (control word `000004`) |
| `DKADR`| `010006`  | logical→physical disc address |
| `DWAIT`| `010313`  | disc-wait poll |
| `ROVER`| `031400`  | overlay load target (OV15 body runs here) |
| `LOGON`| `032736`  | login procedure (prints `@ENTER `) |
| `SINIT`| (resident)| create-SYSTEM cold-start routine (panel `131313`) |

\* addresses marked resident/approximate: **always** re-read from
`Build/drum/DSYMB.SYMB` after a rebuild.

---

## 9. Emulator features added for TSS bring-up

* `--mms1` — NORD-10 Paging System I.
* `--drum=FILE` / `--cdc=FILE` — TSS swapping drum (540) / CDC system disc (500).
* `--opr=OCTAL` — preset the operator's-panel switch register (`TRA OPR`); e.g.
  `--opr=131313` to create the SYSTEM user. F12 → **[6] Control Panel Switches**
  edits it live. See `docs/TSS-CONTROL-PANEL-SWITCHES.md`.
* `--pipe` — keyboard from stdin (expect-style automation of the console).
* `--trace` — per-instruction disassembly to stderr.
