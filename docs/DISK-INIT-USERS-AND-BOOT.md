# Disk initialization, user creation, and booting TSS

**Status:** the create/login *mechanism* below is verified from the source and by
live DAP tracing of the running system. The one open item is the exact
**MINIT run procedure** on the nd100x emulator (how the formatter binary is
booted and which IOX channel it addresses) — see [Open items](#open-items).

This document explains, end to end, how a bare NORD TSS 3.0 disc becomes a
running, loginnable system:

1. **Format the disc** with the standalone `MINIT` program.
2. **Cold-boot** TSS with the operator's-panel switches set to `131313₈`, which
   runs `SINIT` to create user **SYSTEM**.
3. **Normal boot** thereafter drops every terminal at the `@ENTER` login prompt.

---

## 1. The operator's-panel switches (`OPR`) at cold-start

TSS reads the front-panel switch register with `TRA OPR` early in `INIT`.
There is exactly **one** special value; everything else is a normal start.

**[VERIFIED]** `src/TSS1.SYMB:3180-3182`:

```
S10, ...
     TRA OPR; SUB (131313; JAF *+3; LDA (SINIT; JMP S99   ; OPR = 131313 -> run SINIT
     LDA I (AVAIL; JAP *+3; LDA (NOTUP; JMP S99            ; system not available -> NOTUP
     LDA (LEV2                                             ; otherwise: normal boot
S99, ...  (start the chosen routine as the first process, via S5)
```

| `OPR` (octal) | effect |
|---|---|
| **`131313`** | Cold init: start `SINIT`, which **creates user SYSTEM**. Use this once, on a freshly formatted disc. |
| anything else | Normal boot: start `LEV2` (the running system). Every terminal ends up at `@ENTER`. |

`131313` is the "operator is bootstrapping a new system" switch. On the nd100x
emulator this is set with `--opr=131313` (the front-end presets the `TRA OPR`
value). Omit the flag for a normal boot.

> Note: `SINIT` only *creates* SYSTEM's directory entry and tracks. It does **not**
> format the disc's free-track map — that must already exist (step 1). On a raw,
> unformatted disc `SINIT`'s track allocation silently fails and no user is
> created (see [Why order matters](#why-order-matters)).

---

## 2. Step 1 — format the disc with MINIT

`MINIT` — the **"MASS STORAGE INITIALIZATION PROGRAM"** — is a standalone MAC
program (`src/MINIT.SYMB`), *not* part of the TSS OS image. It has its own
terminal I/O (`TCI`/`TCO`) and talks to the CDC disc directly. The operator runs
it once to lay down the **MIB** (Master Information Block — the free-track
bitmap) before the first TSS boot.

It is interactive. **[VERIFIED]** the prompts, `src/MINIT.SYMB:228-234`:

```
MS1, '$MASS STORAGE INIT$$FIRST DISK ADDRESS (NCR): \ '
MS2, '$LAST DISK ADDRESS (NCR): \ '
MS3, '$INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): \ '
```

So a MINIT session is:

```
MASS STORAGE INIT
FIRST DISK ADDRESS (NCR): <first track, octal>
LAST  DISK ADDRESS (NCR): <last track, octal>
INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): I
FINISHED
```

- **NCR** = the logical ("Norsk Data ...") disc addressing MINIT/TSS use; the
  driver maps NCR → physical CDC sector.
- **I**nitialize writes a fresh MIB marking the given track range **free**.
  **U**pdate / **R**egenerate adjust an existing MIB.
- On success it prints `FINISHED`; on trouble, `UNABLE TO WRITE MIB`, `DISK
  ERROR …`, or `ILLEGAL ADDRESSES` (`src/MINIT.SYMB:236-243`).

After MINIT, the disc has a bitmap in which the initialized tracks are marked
free, which is exactly what `GTRK` scans for.

---

## 3. Step 2 — cold-boot with `OPR = 131313` (create SYSTEM)

With a formatted disc, boot TSS with the switches at `131313`. `SINIT` runs as
the first process.

**[VERIFIED]** `src/TSS2.SYMB:858-860`:

```
SINIT, LDX (SINQ; RCLR DD; JPL I (CRUSE; JMP *+1
       JMP I (LEV2
SINQ,  #SY; #ST; #EM; 0; 0; 0; 0        ; the name "SYSTEM", packed 2 chars/word
```

`SINIT` calls `CRUSE` with the name **SYSTEM** and device 0, then hands control
to `LEV2` (the running system). `CRUSE`/`FCRUS` (`src/TSS5.SYMB:74`) does the
real work:

1. `RUTBL` — read the per-device **user/track table** `USTBL` from disc.
2. find a free slot, `GTRK` — **allocate a free track** (needs the MIB from
   step 1).
3. store the **name** and the track into the `USTBL` entry, `WUTBL` — write the
   table back to disc.
4. `IUSER` — initialize the new user's tracks.

The result: user SYSTEM exists in `USTBL`, owns a track, and has user number 1.
No password is set.

### The two on-disk user tables

TSS keeps user information in **two** disc structures, indexed by the same user
number. This split is the key to understanding login:

| table | disc address | holds | written by |
|---|---|---|---|
| **`USTBL`** | `DKBIT+idx` (`DKBIT=50₈`, so ≈ disc **51₈**) | user **names** + track ownership | `CRUSE`/`FCRUS` — i.e. **`SINIT`** and `CRUSR` |
| **`USRDK`** | `1460₈` (`=MAILD+MAILS+400`) | the **password** word per user (entry offset 7) | `CRUSR`, `PASWD`, `CPASW` only |

**A brand-new disc's `USRDK` is empty, and that is correct** — an empty password
word means **passwordless**, exactly what the primordial SYSTEM needs. There is
no "seed USRDK" step; the login *name* comes from `USTBL`, which `SINIT` writes.

---

## 4. Step 3 — normal boot, and how login authenticates

For every subsequent boot, leave `OPR` at anything other than `131313`
(on nd100x: omit `--opr`). TSS starts at `LEV2`, arms the terminals, and each
prints:

```
@ENTER <username>
```

Type `SYSTEM` and press return. **[VERIFIED]** login flow, `LOGON`
(`src/TSS4.SYMB:557-562`):

```
L4, LDX (USRTB; LDT (USRDK; SAA 1; JPL I (XDISK; JMP *-4   ; read USRDK -> USRTB
    LDX CPTR,B; LDT (USARR; SAA 1; COPY DD SA; JPL I (ABLKP; JMP L2   ; match the name
    JMP L2; STA UNO,B; AAA -1                              ; UNO = user number
    RCLR DD; SAD SHR 5; COPY DX SA; SAA 0
    SAD 10; ADD (USRTB; RSUB DA SX; STA OBJ,B              ; entry = USRTB + UNO*8
    SAX 7; LDA I OBJ,B,X; JAZ LOGS                         ; offset 7 = password; 0 -> LOGS
```

The crucial detail: the **name match** (`ABLKP`) uses `USARR` as its
string-array accessor, and `USARR` reads each name from **`USTBL`**, not `USRDK`.
**[VERIFIED]** `src/TSS3.SYMB:660-661`:

```
LDA CNTX,B; SHA 3; ADD (USTBL; COPY DX SA     ; entry = USTBL + idx*8
LDT (USTRG; SAA 7; JPL I (BCOPY               ; copy the 7-word name out of USTBL
```

So:

1. `LOGON` reads `USRDK` (passwords) into `USRTB`.
2. `ABLKP` → `USARR` finds the typed name in **`USTBL`** (written by `SINIT`) and
   returns the user number.
3. `LOGON` reads the password at `USRTB[user][7]`. For SYSTEM on a fresh system
   that word is **0** → `JAZ LOGS` → **passwordless login succeeds**.

A user gets a password later via the `PASSWORD` command (`PASWD`), which writes
`USRDK`. `SYSTEM` can create more users with `CRUSR` (which writes *both* tables)
and clear passwords with `CPASW`.

---

## Why order matters

`SINIT`'s `CRUSE` calls `GTRK`, which allocates a track by scanning the MIB for a
**free** bit. On a disc that has **not** been through MINIT, the MIB is all
zeros (no free tracks), so `GTRK` fails, `CRUSE` returns "no more tracks", and
`SINIT` falls through to `LEV2` having created **nothing**. `LOGON` then finds no
SYSTEM in `USTBL` and silently re-prompts. This is exactly the failure seen when
cold-booting `--opr=131313` on the raw, overlay-only build image.

**Therefore the sequence is mandatory:** MINIT (format) **first**, then cold-boot
`OPR=131313` (create SYSTEM), then normal boots (login).

---

## Prerequisite: the mac-c assembler bug

None of the above works with a TSS image built by the *old* `mac-c` assembler.
A tokenizer bug (`mac.c:289-306`) mis-assembled the all-digit symbol `9377` (the
`377` byte-mask used by the teletype ring routines `WBUF`/`RBUF`) into `AND 0`,
which zeroed **every console character** on the way into the type-ahead buffer —
so no username could ever be typed, and no user could be created either. This is
fixed; see the memory note `tss-login-char-path` and the regression test in
`mac-c/test_mac.c` §[7]. Rebuild the TSS image with the fixed `mac-as` before
attempting any of this.

---

## Open items

- **MINIT run procedure on nd100x** — MINIT is a standalone program with its own
  boot/entry. Still to work out: assemble `src/MINIT.SYMB` to a BPUN with the
  fixed `mac-as`, determine its entry/start address and which IOX channel it
  drives (`CDC` vs `N10` build marks in `MINIT.SYMB`), and run it against the CDC
  disc image the emulator will then boot TSS from. No build/run script exists in
  `mac-c/` yet.
- **NCR address range** — the correct FIRST/LAST NCR values to give MINIT for the
  emulated CDC disc geometry (512 sectors/surface in the current nd100x CDC
  device) need to be derived from the driver's NCR→physical mapping.
- **End-to-end live login** — not yet demonstrated; blocked only on the two items
  above. The character path and the create/login logic are both verified working.
