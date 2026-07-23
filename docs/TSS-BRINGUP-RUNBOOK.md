# NORD TSS 3.0 — verified bring-up runbook (bare disc → SYSTEM account)

**Status:** every step below was executed on the nd100x emulator and verified
2026-07-23. The end state — **user SYSTEM created on a freshly initialised disc,
reaching the `@ENTER` login prompt** — is reproducible. One open item remains
(the interactive login stalls after the name; see [§8](#8-open-item--the-login-read-stall)).

Facts are marked **[VERIFIED]** (observed live or read from source/image) or
**[OPEN]**. Companion docs: [`TSS-BRINGUP-AND-MINIT.md`](TSS-BRINGUP-AND-MINIT.md)
(MINIT internals), [`DISK-INIT-USERS-AND-BOOT.md`](DISK-INIT-USERS-AND-BOOT.md)
(the operator how-to), [`TSS-USERS.md`](TSS-USERS.md) (login mechanism).

---

## 0. The one-paragraph summary

Build the assembler and images, pre-size a CDC disc image large enough for the
user area, run **MINIT** to lay down the free-track bitmap, then cold-boot the TSS
BPUN **at address 7** (the real cold-start) with the operator switches at
`131313₈`. TSS runs `SINIT`, which creates user **SYSTEM**, and every terminal
reaches `@ENTER`. The single non-obvious gotcha: the BPUN's built-in autostart
(`ISTRT`) is the *normal-running* entry and skips the operator-switch test — you
must enter at **address 7** instead.

---

## 1. Prerequisites [VERIFIED]

- The **mac-c `9377` tokenizer fix** must be in place (`mac.c`), or every console
  character is zeroed and login is impossible. Confirm `make test` = 677/0.
- A POSIX toolchain (this project builds under WSL).
- The nd100x emulator with the `--cdc`, `--drum`, `--opr`, `--start`, `--debugger`
  options (all present in the current build).

---

## 2. Build the tool and the images [VERIFIED]

From `mac-c/`:

```bash
make                 # mac-as + test_mac        -> 677 passed, 0 failed
./build_tss_drum.sh  # -> Build/drum/tss-drum.bpun  + Build/drum/tss-cdc.img
./build_minit.sh     # -> Build/minit/minit.bpun
```

- `tss-drum.bpun` — the bootable TSS core image (NORD-10 + drum variant).
- `tss-cdc.img` — the CDC system disc carrying the overlays (overlays live at
  NCR 160₈+, physical sector ~296+).
- `minit.bpun` — the standalone disc formatter (entry `MINIT`=000206).

Both TSS and MINIT build with **0 assembly errors**.

---

## 3. Prepare the CDC disc image [VERIFIED]

The emulated CDC device **does not grow on writes** — it bounds every transfer
against the backing-file size (`deviceCDC.c`, the `wordOff > surfaceWords` check).
The TSS user area (FSYS, see §4) maps to high physical sectors (NCR `4470₈` →
CDC sector ≈ 6288), so the raw `tss-cdc.img` (~459 sectors) is far too small and
MINIT gets `DISK ERROR` on every user track. **Pre-size the backing file:**

```bash
cp Build/drum/tss-cdc.img  <work>/cdc.img
truncate -s 4M             <work>/cdc.img          # 8192 sectors, covers the user area
head -c 1048576 /dev/zero > <work>/drum.img        # blank swap drum
cp Build/drum/tss-drum.bpun <work>/tss.bpun
```

Padding only appends zeros; the overlays (sector ~296) are untouched.
**[VERIFIED]** an 8192-sector image lets MINIT format NCR `4470₈`–`4670₈` cleanly.

---

## 4. Initialise the disc with MINIT [VERIFIED]

Boot MINIT against the CDC disc and run its dialogue. The free-track pool must
start at **FSYS = `4470₈`** — the documented start of user storage; everything
below it (MIB `50`, overlays `160`/`360`, passwords `1460`, the 12 swap areas
`1470`–`4270`) is reserved and must stay marked "used".

```bash
nd100x --boot=bpun --image=Build/minit/minit.bpun --cdc=<work>/cdc.img \
       --debugger --port=1777
```

Drive the console (over the DAP debugger, terminal 192; send CR as `hex:0D`):

```
MASS STORAGE INIT
FIRST DISK ADDRESS (NCR): 4470
LAST  DISK ADDRESS (NCR): 4670
INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): I
INITIALIZE
FINISHED
```

`4470₈`–`4670₈` stepping `10₈`/track = **16 free tracks** (a functional range;
widen LAST for more users). **[VERIFIED]** after `FINISHED`, the MIB at disc `50₈`
(physical sector 104) holds word 18 = `177600₈`, word 19 = `177₈` = **16 set bits
= 16 free tracks**; MINIT writes a *set* bit for each good/free track (see the
polarity note in `TSS-BRINGUP-AND-MINIT.md` §2.4).

> Terminate the MINIT instance to flush the CDC surface back to the backing file
> (the nd100x CDC device writes back on exit).

---

## 5. Cold-start TSS and create SYSTEM [VERIFIED]

**Enter at address 7 — the authentic disk-boot vector — NOT at the BPUN's own
autostart.** The BPUN autostart is `ISTRT` (`src/TSS2.SYMB:1863`), which is the
*normal-running* re-entry (`START`→`LEV2`→`LOGON`) and never runs the operator-
switch test. Address 7 runs the real cold-start that reaches it.

```bash
nd100x --boot=bpun --image=<work>/tss.bpun --cdc=<work>/cdc.img \
       --drum=<work>/drum.img --opr=131313 --start=7 --debugger --port=1777
```

- `--opr=131313` presets the operator's-panel switch register (`TRA OPR`).
- `--start=7` overrides the BPUN autostart with the real cold-start entry.
- **Do NOT use `--start=301` (INIT alone):** it self-loops at INIT's `PON; JMP I 1`
  paging handoff because it skips the page-zero vector setup the address-7 path
  performs. **[VERIFIED]** — 301 hangs, 7 works.

### What happens (the verified chain, addresses OCTAL)

| step | addr | source | action |
|---|---|---|---|
| disk-boot vector | **7** | `TSS1.SYMB:70` | `LDT *+3; JMP I *+1; SYSSV; CORLD` |
| SYSSV | 010603 | `TSS1.SYMB:4108` | saves core image to disc, → INIT |
| INIT | 000301 | `TSS1.SYMB:252` | vectors, tables, N10 paging, `ION` |
| LEV5 → SWAPR → SYY | 006443 / 07270 | `TSS1.SYMB:2794/3173` | schedules the first (SYSTEM) process |
| S10 → OPR test | 07271 / **07307** | `TSS1.SYMB:3175/3180` | `TRA OPR; SUB (131313; JAF *+3; LDA (SINIT` |
| SINIT | 022057 | `TSS2.SYMB:858` | create SYSTEM (calls CRUSE) |
| CRUSE / GTRK / WUTBL | 031272 | `TSS5.SYMB:74` | allocate a track, write name into USTBL |
| LEV2 → LOGON | — | `TSS2.SYMB:1865` | every terminal prints `@ENTER` |

`S10`/`SYY`/`S99` are `)PCL`-killed local labels (absent from the symbol dump);
their addresses were recovered by decoding `tss-drum.img` for the `TRA OPR`
(=150002₈) opcode. **[VERIFIED]** live: the `CRUSE` breakpoint (031272) fires and
`@ENTER` prints on the console.

---

## 6. Verify SYSTEM was created [VERIFIED]

Terminate the emulator (flushes the CDC surface) and inspect `cdc.img`:

```python
d = open("cdc.img","rb").read(); b = 104*512
mib = d[b:b+512]
print("MIB set bits:", sum(bin(x).count("1") for x in mib))   # 15  (was 16)
print("SYSTEM on disc:", b"\x53\x59\x53\x54\x45\x4D" in d[106*512:107*512])  # True
```

**[VERIFIED] result:** MIB set bits **16 → 15** (`GTRK` claimed one track by
clearing its free bit) and the name **`SYSTEM`** appears in the USTBL user-table
region (physical sector 106). That is the account, on disc.

---

## 7. Log in (the intended flow) [VERIFIED prompts / OPEN completion]

On a normal boot (omit `--opr`, or after SINIT hands off to LEV2), each terminal
shows the login dialogue. For SYSTEM (passwordless), the intended sequence is
(`src/TSS4.SYMB:539-585`):

```
@ENTER SYSTEM            <- MS1, type the user name
PROJECT NUMBER P-1       <- MS4 (LOGS); requires a POSITIVE number (0/blank re-prompts)
OK                       <- MS3
@                        <- the command processor (XSTAR)
```

- `@ENTER` and the `SYSTEM` echo are **[VERIFIED]** live.
- Password is skipped for SYSTEM: `TSS4.SYMB:562` `LDA I OBJ,B,X; JAZ LOGS`
  (password word 0 → passwordless).
- The project-number prompt needs a **positive** value; a bare CR loops back.

---

## 8. Open item — the login read stall [OPEN]

After `@ENTER SYSTEM` echoes, LOGON does **not** advance to `PROJECT NUMBER P-`.
Traced with DAP: the CPU sits in a tight idle/disc-wait loop (`006106 ↔ 006107`)
reached from LOGON **L4** (`src/TSS4.SYMB:557`):

```
L4, LDX (USRTB; LDT (USRDK; SAA 1; JPL I (XDISK; JMP *-4
```

i.e. LOGON's **`XDISK` read of the USRDK password table from the paged user-process
context does not complete** — the process blocks before the name is even matched.
This is distinct from the cold-start disc I/O (SINIT/CRUSE), which works. It is
**NOT DETERMINED** yet whether the cause is the disc-completion wakeup for a paged
process, the DMA address translation under paging, or the `JMP *-4` retry path.

**Next step to close it:** break at LOGON `L4`, single-step into `XDISK`/`DWAIT`
for the USRDK read, and watch the CDC device status + the process block/wakeup —
comparing against the working SINIT disc path. (The idle loop's instructions read
as zeros under the debugger's default page view; use the executing PIL context.)

The **account creation is complete and verified regardless** — this open item is
only the interactive login handshake after the name.

---

## 9. Quick reference

**Working command (creates SYSTEM):**
```bash
nd100x --boot=bpun --image=tss.bpun --cdc=cdc.img --drum=drum.img \
       --opr=131313 --start=7 --debugger --port=1777
```

**Key addresses (octal):** SINIT 022057 · CRUSE 031272 · LOGON 032736 ·
S10 07271 · OPR test 07307 · INIT 000301 · SYSSV 010603 · ISTRT 025077 (wrong for
cold-start) · MINIT 000206 · DKBIT/MIB 50 · USRDK 1460 · FSYS 4470.

**DAP console:** terminal 192; send CR as `hex:0D` (a literal `\r` is echoed as
backslash-r). "SYSTEM"+CR = `hex:53595354454D0D`.

**Build recommendation:** change `build_tss_drum.sh` from `-e ISTRT` to `-e 7`
so the BPUN's own autostart is the real cold-start and no `--start=7` override is
needed on the emulator.
