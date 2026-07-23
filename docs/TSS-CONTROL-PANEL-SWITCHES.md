# NORD TSS 3.0 — Operator's Control-Panel Switches (`TRA OPR`)

This manual documents **every** operator's-panel switch value that NORD TSS 3.0
reads, what each one does, when it is sampled, and how to set it on the `nd100x`
emulator (which has no physical panel).

Everything here is derived from the recovered 1973 source in
`E:\Dev\Ronny\TSS\src`. Each entry cites the exact `file:line`. Where a meaning
is **inferred** rather than directly stated by a source comment, it is marked
**[INFERRED]**.

---

## 1. What the panel switch register is

On a real Norsk Data NORD-1 / NORD-10, the operator's panel has a bank of **16
DATA switches**. Software reads their current setting with the instruction:

```
TRA OPR        ; A := operator's-panel switch register (16 bits)
```

`OPR` is CPU internal register #2. On `nd100x` it is `gReg->reg_OPR`
(`src/cpu/cpu_instr.c` `case 02: /* TRA OPR */ gA = gOPR;`). There is no physical
panel, so the value must be injected — see §4.

TSS never *writes* the panel; it only samples it and branches on the value. The
switches are therefore a pure **operator-to-OS control channel**, used for
first-time system generation, disc diagnostics, and low-level debugging.

---

## 2. The switch values TSS acts on

TSS reads `TRA OPR` in **five** places. Summary table (all values OCTAL):

| Value            | Where it is read              | Hardware gate | Action |
|------------------|-------------------------------|---------------|--------|
| `131313`         | Cold start `S10`              | both          | Create the **SYSTEM** user, then boot |
| `111111`         | Disc driver `XDISK` error path | both (per controller) | **Verbose disc-error diagnostics** |
| `25252`          | Scheduler `LEV5`              | **NORD-1 only** | Panel-driven **memory examine/deposit** monitor |
| *address*, bits 0–14 (bit 15 masked off) | Clock `LEV13` | **NORD-10 only** | Continuous **memory-word display** in the LEV4 register block |
| (any, low 15 bits) | see above                   |               | (same read; the value is used as an address, not compared) |

Each is detailed below.

---

### 2.1 `131313` — Cold start: create the SYSTEM user

**Source:** `src/TSS1.SYMB:3180` (routine `S10`, the cold-start dispatcher):

```
S10, ...
   TRA OPR; SUB (131313; JAF *+3; LDA (SINIT; JMP S99   ; panel == 131313 -> SINIT
   LDA I (AVAIL; JAP *+3;  LDA (NOTUP; JMP S99           ; else if system down -> NOTUP
   LDA (LEV2                                             ; else normal boot -> LOGON
S99, ...
```

`SUB (131313` then `JAF *+3` ("jump if A is filled / non-zero"): if the panel
equals `131313` the subtraction yields 0, the jump is **not** taken, and control
falls through to `LDA (SINIT; JMP S99` — dispatching the **SINIT** routine.

**SINIT** (`src/TSS2.SYMB:858`) creates the first user:

```
SINIT, LDX (SINQ; RCLR DD; JPL I (CRUSE; JMP *+1
   JMP I (LEV2
SINQ,  #SY; #ST; #EM; 0; 0; 0; 0          ; packed chars = "SYSTEM"
```

`SINQ` is `#SY #ST #EM` = the packed 2-char constants spelling **`SYSTEM`**.
`SINIT` calls `CRUSE` (create-user, `src/TSS2.SYMB:3491` → overlay
`src/TSS5.SYMB:61`) to create that user, then jumps to `LEV2` → normal boot →
`LOGON`.

**Meaning:** this is the classic ND "set the switches to a magic number to
generate the system." On a **virgin** disc there are no users, so you *must* set
the panel to `131313` on the first cold start to create the `SYSTEM` account;
after that you log in as `SYSTEM` and create the rest with the `CRUSR` command
(`src/TSS4.SYMB:920`). Not gated by hardware type — works on both NORD-1 and
NORD-10.

**Related dispatch (not a panel switch):** `AVAIL` (`src/TSS1.SYMB:317`) is the
"system availability" flag. If the panel is *not* `131313` and `AVAIL` is
negative, `S10` dispatches `NOTUP` (system down) instead of `LEV2`
(`src/TSS1.SYMB:3181`).

---

### 2.2 `111111` — Verbose disc-error diagnostics

**Source:** `src/TSS2.SYMB:577` (NCR controller variant) and
`src/TSS2.SYMB:585` (CDC controller variant), both inside the disc driver
`XDISK`'s error path:

```
"NCR
T1,   TRA OPR; SUB (111111; JAF T0                  ; panel != 111111 -> retry (T0)
   LDX (MS1; JPL I 7MSG                             ; else print error text ...
   LDA ERR,B; SAT 10; JPL I 9IOUT                   ; ... error code
   LDX (MS2; JPL I 7MSG
   LDA TREG,B; SAT 10; JPL I 9IOUT                  ; ... disc address
   LDX (MS4; JPL I 7MSG; JMP T0
"
"CDC
T1,   TRA OPR; SUB (111111; JAF T1A                 ; panel != 111111 -> silent (T1A)
   LDX (MS1; JPL I 7MSG
   LDA ERR,B; SAT 10; JPL I 9IOUT                   ; error code
   LDX (MS2; JPL I 7MSG
   LDA TREG,B; SAT 10; JPL I 9IOUT                  ; logical disc address
   LDX (MS3; JPL I 7MSG
   LDA TREG,B; JPL I (DKADR; JMP *+1; SAT 10; JPL I 9IOUT  ; physical disc address
   LDX (MS4; JPL I 7MSG
T1A,  JMP T0
"
```

**Meaning:** when a disc transfer fails, TSS normally **retries silently**
(`JAF` branches straight to the retry entry `T0`/`T1A`). If the operator sets the
panel to `111111`, TSS instead **prints the error details** to the console — the
error code, the logical disc address, and (CDC variant) the physical disc
address computed by `DKADR`. This is a per-controller verbose-diagnostics toggle
for chasing disc faults, active on both hardware types (each controller build has
its own copy).

---

### 2.3 `25252` — Panel-driven memory examine/deposit (NORD-1 only)

**Source:** `src/TSS1.SYMB:2724`, inside the scheduler `LEV5`, gated `"NN10`
(assembled **only** for NORD-1, *not* NORD-10):

```
"NN10
L1, TRA OPR; LDT (25252; SKP IF DA EQL ST; JMP YY    ; panel != 25252 -> normal (YY)
    IOT SKA ACT RKE; JMP XX; AND (177; SAT 15         ; read a console character
    SKP IF DA EQL ST; JMP *+7; LDA VAL                ; char == CR -> deposit
    STA I VALP; STZ VAL; MIN VALP; 0; JMP XX; SAT ##/
    SKP IF DA EQL ST; JMP *+6                          ; char == '/' -> examine
    LDA VAL; STA VALP; LDA I VALP; STA VAL; JMP XX
    AAA -60; ... LDA VAL; SHA 3; RADD DA SD; STA VAL    ; digit -> VAL = VAL*8 + digit
...
"N10
L1, JMP ZZ                                             ; NORD-10: feature absent
"
```

**Meaning:** on a **NORD-1**, setting the panel to `25252` (the alternating
bit pattern `0b010 101 010 101 010`) turns the console, driven from the
scheduler, into a resident **memory examine/deposit monitor**:

* type **octal digits** — they accumulate into a working value `VAL`
  (`VAL := VAL*8 + digit`);
* type **`/`** — set the address pointer `VALP := VAL` and **examine**
  (`VAL := mem[VALP]`, i.e. show that word);
* type **carriage return** — **deposit** `VAL` into `mem[VALP]` and advance
  `VALP`.

This is a developer/operator low-level debugging tool. On **NORD-10** the whole
block is compiled out (`"N10 L1, JMP ZZ`), so **`25252` does nothing on the
NORD-10 (DRUM) build** that `nd100x` runs.

---

### 2.4 Panel value as a display address (NORD-10 only)

**Source:** `src/TSS1.SYMB:4031`, inside the clock interrupt handler `LEV13`,
gated `"N10` (assembled **only** for NORD-10):

```
%STATUS INFORMATION FOR LEVEL 4 REGISTER BLOCK
   IOX 306; IRW 40 DA                     ; console output status -> LEV4 A
   TRA OPR; AND (77777; COPY DX SA        ; panel & 077777 -> address in X
   LDA 0,X; IRW 40 DD                      ; mem[address] -> LEV4 D
```

**Meaning:** on **NORD-10**, every clock tick the `LEV13` handler reads the
panel, **masks off the top bit** (`AND (77777` keeps bits 0–14), treats the
result as a **memory address**, loads the word stored there, and copies it into
the **level-4 register block** (the `D` register of interrupt level 4). The
level-4 register block is what the operator sees on the panel's register-display
lights.

So on the NORD-10 build the panel switches act as a **live memory monitor**: dial
in an address (bits 0–14) and the panel display continuously shows the contents
of that word. **[INFERRED]** bit 15 is reserved/ignored by this path (it is
masked off); it is *not* one of the compared magic values above, so leaving it
set or clear only affects which of two 32K halves… actually the mask keeps 15
bits = a full 0–32767 word address, so bit 15 simply has no effect here.

Because this read happens **unconditionally every tick**, the panel is *always*
being interpreted as a display address on NORD-10 — it is harmless when you don't
care (address 0 is shown when the switches are 0).

---

## 3. Quick reference card

```
OCTAL     ACTION                                              HARDWARE
-------   -------------------------------------------------   -----------------
131313    Cold start: create the SYSTEM user (first sysgen)   NORD-1 + NORD-10
111111    Verbose disc-error diagnostics (else silent retry)  NORD-1 + NORD-10
025252    Console memory examine/deposit monitor              NORD-1 only
<addr>    Low 15 bits = memory word shown in LEV4 display      NORD-10 only
000000    Normal run (LEV4 display shows word at address 0)   -
```

`nd100x` runs the **NORD-10 / DRUM** build, so of the special *compared* values,
only **`131313`** and **`111111`** are active; `25252` is compiled out, and the
`LEV13` address-display path is active but cosmetic.

---

## 4. Setting the panel switches on nd100x

`nd100x` has no physical panel, so the `OPR` register is provided two ways
(both added specifically for TSS; see `docs/TSS-BRING-UP-FROM-SCRATCH.md`):

### 4.1 Command line — `--opr=OCTAL`

Presets `gReg->reg_OPR` before the CPU starts (applied after `program_load`, so
`cpu_reset`'s zeroing does not wipe it). The value is parsed in **octal**:

```
nd100x --mms1 --opr=131313 --boot=bpun --image=…/tss-drum.bpun \
       --drum=…/tss-drum.img --cdc=…/tss-cdc.img --start=000301
```

Invalid (non-octal or > `177777`) values are rejected with an error.

### 4.2 Interactive — F12 menu, option **[6] Control Panel Switches**

Press **F12** to open the menu, then **6**. The window shows the current `OPR`
in octal / hex / decimal and a 16-bit switch display, decodes the TSS meaning of
the current value, and lets you edit it live:

* **`0`–`7`** — shift an octal digit in from the right (exactly like keying the
  physical panel switches);
* **`C`** — clear to `000000`;
* **`S`** — set `131313` (cold start: create SYSTEM user);
* **`D`** — set `111111` (verbose disc-error diagnostics);
* **ESC** — back.

Edits take effect the next time TSS executes `TRA OPR`.

---

## 5. Caveats / open items

* Setting `--opr=131313` makes `SINIT`/`CRUSE`/`FCRUS` **run** (VERIFIED: with
  `--opr=131313`, `TRA OPR` returns `131313` and the `S10` branch falls through to
  the create-SYSTEM dispatch). It does **not yet** yield a usable login, because
  the mac-c CDC image is **overlay-only** — it has no MIB / bit table / user table
  / system image. Create-user writes the MIB user table at `DKBIT` (~physical
  sector 104, in range) but there is no valid MIB there to update, and `LOGON`
  reads the login directory `USRDK` at physical sector **2176**, past the
  459-sector image and the 512-sector CDC device surface. A real bring-up must
  also lay down the disc substrate (`SYSSV` at bootstrap addr 7, or a pre-formatted
  MIB) on a larger disc — see `docs/TSS-BRING-UP-FROM-SCRATCH.md` §6.1 and §7.
  These are **[VERIFIED]** findings, separate from the switch mechanism itself.
* The exact `TRA OPR` addresses shift by ±1 word after any `src/` edit
  (symbol-table shift); resolve them from `Build/drum/DSYMB.SYMB`, never
  hard-code.
