# NORD TSS Overlay-on-Disc Implementer's Spec

How TSS stores code overlays on disc and loads them back, derived **strictly
from the TSS source** at `E:\Dev\Ronny\TSS\src\TSS1.SYMB … TSS5.SYMB` and the
golden symbol dump `E:\Dev\Ronny\TSS\reference\ASYMB.SYMB`.

Every claim is tagged **[VERIFIED]** (file+line+quote) or **[INFERRED]**.
Items I could not settle from the source are in section 8.

All numeric values are **OCTAL** unless marked "dec" (that is how MAC prints
and how the source is written).

> **HEADLINE CORRECTION — read this first.**
> The premise "reproduce the SOVER routine" does **not** hold for the build
> you are reproducing. `SOVER` and the `)SOVER` command live **only** in the
> `"NMACF` (NOT-MACF) code path. Every golden build — ASSYSA, ASSYSB, and the
> derived `CDC MACF DIAB K14 TEL4 DRUM N10` build — sets the **MACF** mark, so
> `"NMACF` is FALSE and **`SOVER` is never assembled**. In a MACF build each
> overlay is instead staged by the `"MACF` variant of the `OVERX` macro using
> **`)9MOVE ROVER VOR VORS`** + `)WRITE`. See section 1 for the mark proof.
> The good news: the **run-time disc contract is identical** either way
> (`disc = OVDK + 2*OVLAY`, 2×256-word sectors at core `ROVER`), so to place
> overlays where the running dispatcher reads them you reproduce that contract
> directly (section 7), regardless of which assembly-time path built them.

---

## 1. Which write path + device (with the mark evaluation)

### 1.1 Mark negation: `NMACF` means "NOT MACF" — proof

`eval_mark` in the reimplementation negates a term only with a leading `-`;
`N`-prefixed names are **ordinary symbols**, made to mean "not X" by an
explicit source construct. **[VERIFIED]** `TSS1.SYMB:108-111`:

```
NMACF            % references NMACF -> present-but-undefined -> mark TRUE
"MACF            % if MACF is a set mark ...
)KILL NMACF      %   ... remove NMACF -> its mark becomes FALSE
"
```

So `NMACF` is a **true library mark iff MACF is NOT set**. The same idiom
defines `NN10 = NOT N10` **[VERIFIED]** `TSS1.SYMB:93-96`, and `NCR = NOT CDC`
**[VERIFIED]** `TSS1.SYMB:76-79`.

### 1.2 The build sets MACF (and CDC, N10, DRUM)

**[VERIFIED]** DRUM build marks — `derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB:13`:
`CDC MACF DIAB K14 TEL4 DRUM N10`.
**[VERIFIED]** ASSYSA marks — `src/ASSYSA.SYMB:4`: `CDC MACF DIAB K14 TEL4`.
Therefore in every build of interest: **MACF set → `"NMACF` FALSE, `"MACF`
TRUE**; **CDC set → `"NCR` FALSE**; **N10 set → `"NN10` FALSE, `"N10` TRUE**.

### 1.3 Consequence: SOVER is compiled OUT; OVERX-MACF is the real path

`SOVER` is wrapped in `"NMACF` **[VERIFIED]** `TSS3.SYMB:35` (`"NMACF`) …
`TSS3.SYMB:43` (`"NMACF`) … `TSS3.SYMB:49` (`"` close). With MACF set the whole
block (lines 36-48, `SOVER, LDA OVADR …`) is skipped. Likewise the `"NMACF`
definition of the `OVERX` macro (`TSS3.SYMB:67-79`, which contains the only
`)SOVER` in the tree at line 78) is not defined.

The macro that **is** defined and invoked is the `"MACF` `OVERX`
**[VERIFIED]** `TSS3.SYMB:80-93`:

```
"MACF
)MCDEF OVERX $A1
$A1 =*-ROVER            % $A1 := overlay size in words
)9MOVE ROVER VOR VORS   % copy VORS(=1000) words ROVER -> VOR  (block image move)
GLOB=VOR
)KILL VOR
VOR=GLOB+VORS           % advance VOR window by one overlay slot
)KILL GLOB
OVLAY/ RQR-1            % preset the three overlay cells to the last overlay #
OVLAX/ RQR-1
SYSOV/ RQR-1
)WRTM                   % enter write mode
)WRITE $A1              % dump the size symbol to the object/list stream
   ]
```

`)9MOVE` semantics **[VERIFIED]** MAC User's Guide `ND-60.096.01 … §C.1.1.1`:
"used to move a block of image from one place to another … three standard MAC
symbols … 1. source address 2. destination address 3. word count." It is a
**raw block copy of assembled words — no relocation/patching**. So the bytes
staged are the ROVER image verbatim.

### 1.4 Physical device for the overlays

Overlays do **not** live on the swap drum (IOX 540). They live on the **CDC
system disc**. Evidence, both directions:

* **Write path (NMACF/SOVER, for reference)** calls `XDISK` **[VERIFIED]**
  `TSS3.SYMB:44-45` (`… JPL I (XDISK; …`).
* **Run-time read path** (the one that always exists — section 4) reads via
  `SDISK` **[VERIFIED]** `TSS1.SYMB:3072-3073, 3136` (`9SDK, SDISK`).
* Both `XDISK` and `SDISK`, in the **`"CDC N10`** branch, drive the CDC disc
  controller with `IOX` to register group **DCHN**, and `DCHN = 500` in the N10
  build.

**`DCHN` resolution [VERIFIED]** `TSS1.SYMB:3333-3338`:

```
DCHN                 % -> mark TRUE
"DCHN NN10           % NORD-1 build
DCHN=100
"DCHN N10            % NORD-10 build
DCHN=500
"
```

DRUM build is N10 → **DCHN = 500**. CDC disc register codes for N10
**[VERIFIED]** `TSS1.SYMB:3356-3364`:

| reg | expr | value (DCHN=500) |
|---|---|---|
| RCA | DCHN+0 | 500 |
| LCA | DCHN+1 | 501 |
| RSECT | DCHN+2 | 502 |
| LBA | DCHN+3 | 503 |
| RST | DCHN+4 | 504 |
| LMR | DCHN+5 | 505 |
| SEEK | DCHN+6 | 506 |
| LWC | DCHN+7 | 507 |

The active `XDISK` transfer core for this build **[VERIFIED]**
`TSS2.SYMB:548-558` (`"CDC N10` branch), which issues `IOX RST/LCA/LBA/LWC/LMR`
and `LDA (400; IOX LWC` (256-word transfer).

> **Golden-dump caveat:** `ASYMB.SYMB` is the **NN10** (NORD-1) A build, so its
> device symbols are the `DCHN=100` set (`DISC=000144, LCA=000145, LBA=000545,
> …` **[VERIFIED]** `reference/ASYMB.SYMB:128-136`). The DRUM build is N10 →
> add 400 to the channel base (500-507).

**Answer to Q1/Q2:** in the DRUM build the write path is the **OVERX-MACF /
`)9MOVE`** path (SOVER/8DUMP are not present), and the overlays are stored on
the **CDC system disc, IOX device channel 500 (registers 500-507)** — *not* on
the drum (540). The `8DUMP`/`8DKA/8CADR/8NWD/8FCN` path is `"TSBIN` binary-tape
bootstrap code **[VERIFIED]** `TSS3.SYMB:39-42` (`"TSBIN`) and the TDUMP utility
`src/TDUMP.SYMB`; TSBIN is FALSE in these builds (`% TSBIN FALSE`
`TSS1.SYMB:47`), so it does not apply.

---

## 2. Disc-address formula with resolved numbers

Both the (reference) SOVER writer and the run-time reader compute the disc
address the same way:

```
disc_sector = OVLAY * 2 + OVDK          (unit = 256-word sector)
```

**[VERIFIED]** writer `TSS3.SYMB:38`: `LDA I (OVLAY; SHA 1; ADD (OVDK`
(`SHA 1` = shift-left-1 = ×2). **[VERIFIED]** reader `TSS1.SYMB:3071`:
`LDA I 9OVL; STA I (SYSOV; SHA 1; ADD (OVDK`.

### Resolved constants (DRUM build: CDC set, DEBUG *not* set)

| symbol | expression | value | source |
|---|---|---|---|
| DKS | start of disc | `000000` | `TSS1.SYMB:2578` **[V]** / `ASYMB:96` |
| DKBIT | CDC file-system MIB | `000050` | `TSS1.SYMB:2584` **[V]** / `ASYMB:97` |
| CORL1 | DKBIT+DKS+10 | `000060` | `TSS1.SYMB:2586` **[V]** / `ASYMB:98` |
| CORL2 | CORL1+200 | `000260` | `TSS1.SYMB:2587` **[V]** / `ASYMB:99` |
| OVDK1 | CORL1+100 | `000160` | `TSS1.SYMB:2593` **[V]** / `ASYMB:105` |
| OVDK2 | CORL2+100 | `000360` | `TSS1.SYMB:2594` **[V]** / `ASYMB:106` |
| **OVDK** | OVDK1 (DEBUG→OVDK2) | **`000160`** | `TSS1.SYMB:2611-2618` **[V]** / `ASYMB:113` |

OVDK selection **[VERIFIED]** `TSS1.SYMB:2611-2618`: default `OVDK=OVDK1`, and
only `"DEBUG` switches it to `OVDK2`. DRUM/ASSYSA builds do **not** set DEBUG →
**OVDK = 160**. (ASSYSB *does* set DEBUG → OVDK=360, so the debug build's
overlays occupy a disjoint disc region — `src/ASSYSB.SYMB:4`.)

The disc address is a **linear 256-word-sector number** in the same space as
all other TSS disc structures (CORL1/CORL2, ACCTD, USRDK, DKA1..12, FSYS —
`TSS1.SYMB:2586-2608`). At I/O time the CDC driver converts this linear sector
number to physical cylinder/track/sector via **`DKADR`** **[VERIFIED]**
`TSS1.SYMB:3563-3623` ("CONVERT NCR DISK ADDRESS TO CDC DISK ADDRESS",
`DKLIM=626` cylinders for a CDC 9427) before loading `IOX LBA`. **[VERIFIED]**
`TSS2.SYMB:551`: `LDA TREG,B; JPL I (DKADR; JMP XDF1; IOX LBA`.

### OV19 (command processor XSTAR)

**[VERIFIED]** `TSS5.SYMB:1786` `OVERL OV19`, `TSS5.SYMB:1808`
`GOVER OV19,S1,XSTAR` → XSTAR is overlay OV19. **[VERIFIED]** `ASYMB:682`
`OV19=000036`.

```
OV19 disc sectors = OVDK + 2*OV19 = 160 + 2*36 = 254  and  255  (octal)
```

---

## 3. ROVER area + overlay sizes

* **ROVER** — overlay core base. **[VERIFIED]** `TSS3.SYMB:3` `ROVER, BSS 1000`
  (reserves 1000 octal = 512 dec words). Address in the golden A build:
  `ROVER=031522` **[VERIFIED]** `ASYMB:504`. `ROV4=ROVER+400` **[VERIFIED]**
  `TSS3.SYMB:4` / `ASYMB:505` `ROV4=032122` (= 031522+400 ✓).
  **[INFERRED]** the ROVER *address* differs between the NN10 and N10 builds
  (different code precedes it); read it from the DRUM build's own dump (DSYMB),
  but `BSS 1000` and the 2-sector split are build-independent.
* **Size / max size** — each overlay is written/read as exactly **1000 octal
  (512) words = two 256-word sectors**: sector D ← `ROVER..ROVER+377`,
  sector D+1 ← `ROV4..ROV4+377`. **[VERIFIED]** two fixed 256-word transfers,
  reader `TSS1.SYMB:3072-3073` (`LDX (ROVER … ; AAT 1; LDX (ROV4 …`), transfer
  count `LDA (400; IOX LWC` `TSS2.SYMB:552`.
* **Actual used size per overlay** = `*-ROVER` at the `OVERX`, stored in the
  `QOVn` symbol. **[VERIFIED]** `TSS3.SYMB:82` `$A1 =*-ROVER`. Values are in the
  dump, e.g. `QOV19=000725` (= 469 dec words used of 512) **[VERIFIED]**
  `ASYMB:692`; `QOV1=000574`, `QOV9A=000762` (max seen), etc. All ≤ 1000.
* **Size guard** (reference SOVER only): halt `WAIT 47` if
  `OVADR - (ROVER+1000) ≥ 0`, i.e. overlay ≥ 512 words **[VERIFIED]**
  `TSS3.SYMB:37`.

---

## 4. Read-back path (run time) — the inverse contract

The GOVER macro **[VERIFIED]** `TSS3.SYMB:15-26` expands a call site to
`STX I *+5; SAX 0; SWAP DP SX; OVn ; seg ; SAVX`, and dispatch goes through
page-zero word 0 → word 3 = `GOVX` **[VERIFIED]** `TSS1.SYMB:62-65`
(`0/ JMP I *+3` … `3/ GOVX; USTK; STK`).

**`GOVX` (user-side dispatcher)** **[VERIFIED]** `TSS2.SYMB:181-189`:

```
GOVX, SWAP DX SL; STA SAVA; STX SAVL; COPY DX SL
   LDA 0,X; SUB I 9OVL; JAZ *+7          % requested OVn == current OVLAY ? skip
   LDA I 9OVL; STA I (OVLAX; LDA 0,X; STA I 9OVL   % save old, set OVLAY := OVn
   SAA 40; MST PID; …                    % raise level-40 interrupt to swap it in
9OVL, OVLAY
```

GOVX itself does **not** touch the disc; it records the wanted overlay in
`OVLAY` and triggers the scheduler level. The actual disc read is done by the
level-5 system routine **S5** when it notices `SYSOV ≠ OVLAY` **[VERIFIED]**
`TSS1.SYMB:3069-3073`:

```
%CHECK IF AN OVERLAY SHOULD BE READ IN
S5,   LDA I (SYSOV; SUB I 9OVL; JAZ S5X            % already loaded ? skip
   LDA I 9OVL; STA I (SYSOV; SHA 1; ADD (OVDK      % A := OVLAY*2 + OVDK
   COPY DT SA; LDX (ROVER; SAA 1; JPL I 9SDK       % READ sector -> ROVER
   AAT 1;      LDX (ROV4;  SAA 1; JPL I 9SDK       % READ sector+1 -> ROV4
S5X, …
```

This is the exact inverse of the SOVER writer:

| | writer (SOVER, `"NMACF`) | reader (S5, always) |
|---|---|---|
| disc addr | `OVLAY*2+OVDK` `TSS3:38` | `OVLAY*2+OVDK` `TSS1:3071` |
| op code A | `SAA 3` = WRITE `TSS3:44` | `SAA 1` = READ `TSS1:3072` |
| core, sec0 | `ROVER` | `ROVER` |
| core, sec1 | `ROV4` | `ROV4` |
| device | XDISK (CDC 500) | SDISK→DKOP (CDC 500) |

`SDISK` A-register convention **[VERIFIED]** `TSS1.SYMB:3927` "A = OPERATION
(1=READ, 3=WRITE)". SDISK CDC variant loops `DKOP` (256-word CDC disc op)
**[VERIFIED]** `TSS1.SYMB:3946-3954`, `DKOP` at `TSS1.SYMB:3371-3398`
("PERFORM A 256 WORD DISK OPERATION").

There are two overlay cells: **OVLAY** (user request, addr `015222`
**[V]** `ASYMB:278` / decl `TSS2.SYMB:78`) and **SYSOV** (what is physically
loaded, addr `011350` **[V]** `ASYMB:203` / decl `TSS1.SYMB:4539`); `OVLAX`
holds the previous overlay `TSS2.SYMB:80`.

**Answer to Q4:** yes — the reader uses the identical `OVLAY*2+OVDK` address,
reads from the CDC disc (device 500 in the N10 build) into `ROVER`/`ROV4`, two
256-word sectors, op=READ. Overlays written at `OVDK+2n` will be found.

---

## 5. Overlay layout table (DRUM build, OVDK=160)

Numbers verified from `reference/ASYMB.SYMB` via `mac-c/overlay_layout.py`
(overlay numbers and OVDK are build-independent; ROVER core address shown is
the NN10 golden value — re-read from the DRUM dump for the N10 build).
Sectors are **octal 256-word sector numbers on the CDC disc**; each overlay
occupies the two consecutive sectors `sec0, sec1`. Overlays **are** packed at
`OVDK + 2*n`.

| label | OVn (oct) | sec0 | sec1 | core load | used words (QOVn) |
|---|---|---|---|---|---|
| OV1  | 000000 | 000160 | 000161 | ROVER | 000574 |
| OV1A | 000001 | 000162 | 000163 | ROVER | 000704 |
| OV1B | 000002 | 000164 | 000165 | ROVER | 000426 |
| OV1C | 000003 | 000166 | 000167 | ROVER | 000642 |
| OV2  | 000004 | 000170 | 000171 | ROVER | 000725 |
| OV2A | 000005 | 000172 | 000173 | ROVER | 000514 |
| OV3  | 000006 | 000174 | 000175 | ROVER | 000447 |
| OV4  | 000007 | 000176 | 000177 | ROVER | 000551 |
| OV4A | 000010 | 000200 | 000201 | ROVER | 000572 |
| OV5  | 000011 | 000202 | 000203 | ROVER | 000620 |
| OV6  | 000012 | 000204 | 000205 | ROVER | 000702 |
| OV6A | 000013 | 000206 | 000207 | ROVER | 000527 |
| OV6B | 000014 | 000210 | 000211 | ROVER | 000547 |
| OV7  | 000015 | 000212 | 000213 | ROVER | 000533 |
| OV8  | 000016 | 000214 | 000215 | ROVER | 000713 |
| OV9  | 000017 | 000216 | 000217 | ROVER | 000663 |
| OV9A | 000020 | 000220 | 000221 | ROVER | 000762 |
| OV9B | 000021 | 000222 | 000223 | ROVER | 000707 |
| OV9C | 000022 | 000224 | 000225 | ROVER | 000714 |
| OV9D | 000023 | 000226 | 000227 | ROVER | 000265 |
| OV10 | 000024 | 000230 | 000231 | ROVER | 000561 |
| OV11 | 000025 | 000232 | 000233 | ROVER | 000717 |
| OV12 | 000026 | 000234 | 000235 | ROVER | 000670 |
| OV13 | 000027 | 000236 | 000237 | ROVER | 000602 |
| OV14 | 000030 | 000240 | 000241 | ROVER | 000713 |
| OV15A| 000031 | 000242 | 000243 | ROVER | (QO15A) |
| OV15 | 000032 | 000244 | 000245 | ROVER | 000672 |
| OV16 | 000033 | 000246 | 000247 | ROVER | 000713 |
| OV17 | 000034 | 000250 | 000251 | ROVER | 000545 |
| OV18 | 000035 | 000252 | 000253 | ROVER | 000715 |
| **OV19 (XSTAR)** | **000036** | **000254** | **000255** | ROVER | **000725** |

Total 31 overlays (numbers 0..30 dec), disc sectors 160..255 (octal). RQR ends
at `000037` **[VERIFIED]** `ASYMB:683`, confirming 31 overlays. Cross-check:
the MACF `VOR` staging pointer starts 40000 (`TSS3.SYMB:5`) and advances
`VORS=1000` per overlay; final `VOR=077000` **[VERIFIED]** `ASYMB:693` →
(077000-040000)/1000 = 37 octal = 31 overlays. ✓

---

## 6. Where the overlay write fires + `)9MOVE` effect

### Overlay bracketing

Each overlay body is bracketed by a macro pair:

* **`OVERL OVn`** opens it: assigns `OVn := RQR`, kills+bumps `RQR`, and resets
  the location counter to `ROVER`. **[VERIFIED]** `TSS3.SYMB:56-61`:
  `$A1 =RQR; )KILL RQR; RQR=$A1 +1; ROVER/`. RQR starts at 0 **[VERIFIED]**
  `TSS2.SYMB:171` `RQR=0`.
* **`OVERX QOVn`** closes it: in the MACF build → `)9MOVE ROVER VOR VORS` then
  `)WRTM` / `)WRITE` (§1.3). **This is the snapshot point.**

Invocation sites (all `OVERL … OVERX` pairs) **[VERIFIED]**:
`TSS3.SYMB` OV1..OV3 (7 overlays, e.g. `OVERL OV1`@302 … `OVERX QOV1`@638);
`TSS4.SYMB` OV4..OV9D (13, `OVERL OV4`@1 … `OVERX QOV9D`@2038);
`TSS5.SYMB` OV10..OV19 (11, `OVERL OV10`@1 … `OVERX QOV19`@1969).

### `)SOVER`, `)8DUMP` occurrences

* **`)SOVER`** appears exactly once, inside the **`"NMACF`** `OVERX` macro body
  **[VERIFIED]** `TSS3.SYMB:78`. Not expanded in a MACF build. No `)SOVER` in
  any command-stream file (`derived/*.SYMB`).
* **`)8DUMP`** appears in `TSS5.SYMB:1987,1992` (guarded, `"TSBIN` bootstrap
  region) and in the standalone `src/TDUMP.SYMB` utility. Not on the MACF path.
* The command streams `derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB` and
  `ASSYSA-MAC-INPUT.SYMB` contain **only** `)9ASSM TSSn,…` + `)LIST` + `)9EXIT`
  **[VERIFIED]** — the overlay writes are driven from inside the source by
  `OVERX`, never from the stream.

### What `)9MOVE ROVER VOR VORS` does to the bytes

It is a **verbatim block copy** of `VORS` (1000 octal) assembled words from
core `ROVER` to core `VOR`, no relocation **[VERIFIED §1.3]**. Effect on the
image: each overlay, all assembled at the same `ROVER`, is preserved at a
distinct `VOR` slot (40000, 41000, … 76000) so all 31 survive in MAC's memory
image. It does **not** alter the overlay's own words — internal references stay
`ROVER`-relative because the overlay both assembles and executes at `ROVER`.
So the bytes you must place on disc = the raw `ROVER..ROVER+777` image captured
at each `OVERX`.

---

## 7. To reproduce SOVER in a host assembler — checklist

You are writing the overlay image to an emulated CDC disc so the running TSS
dispatcher (S5, §4) finds it. Do **not** try to re-run the `SOVER` machine
code; reproduce its disc contract:

1. **Resolve the constants from the build's own symbol table** (not hard-coded):
   `OVDK` (= 160 for CDC + non-DEBUG; 360 for DEBUG), `ROVER` (its address),
   `VORS`/overlay size = `1000` octal = 512 words, and every `OVn` number.
2. **Bracket each overlay.** On `OVERL OVn`: record `base := current location`
   (it is reset to `ROVER`) and `num := value of OVn`. On `OVERX`: the overlay
   is complete.
3. **Snapshot 512 words** of the assembled image starting at `ROVER`
   (`image[ROVER .. ROVER+0o1000)`), at the `OVERX` point, before the next
   `OVERL` overwrites `ROVER`. (Equivalently snapshot the `VOR` slot if you
   implement `)9MOVE`.)
4. **Enforce the guard**: reject/flag any overlay whose used size
   (`*-ROVER` = `QOVn`) is ≥ `0o1000`.
5. **Compute the disc target**: `sec0 = OVDK + 2*num`, `sec1 = sec0 + 1`
   (256-word sectors).
6. **Write** words `[0..0o400)` of the snapshot to `sec0`, words
   `[0o400..0o1000)` to `sec1`, on the CDC system-disc image (the same linear
   sector space as DKS/CORL1/ACCTD/…; sector size 256 words). If your disc
   image is physical-geometry addressed, apply the `DKADR` linear→CDC
   cyl/track/sector mapping (`TSS1.SYMB:3574-3612`, `DKLIM=626` for a 9427).
7. **Do not relocate** the overlay words — copy them verbatim (matches
   `)9MOVE` and matches the reader loading straight into `ROVER`).
8. **Device sanity**: overlays go on the CDC disc (IOX channel 500 in the N10
   build / 100 in NORD-1), **never** the swap drum (IOX 540).
9. Reuse `mac-c/overlay_layout.py` to regenerate the target sector for every
   `OVn` from a `)LIST` dump and diff against what you wrote.

Minimal per-overlay result: overlay `num` → CDC-disc sectors `160+2*num` and
`160+2*num+1`, holding the 512-word `ROVER` image.

---

## 8. COULD NOT DETERMINE (from the TSS source)

1. ~~**How the MACF image's `VOR` window (40000..77000) is transferred onto the
   OVDK disc sectors at generation time.**~~ **RESOLVED (2026-07-21).** For the
   reproduction it is `mac_write_cdc_disc` (`mac-c/mac.c`): for each overlay *n*
   it copies the 512-word `VOR` window (`VOR_base + n*VORS`) to the CDC-disc
   **physical** sectors `cdc_dkadr(OVDK+2n)` and `cdc_dkadr(OVDK+2n+1)` — where
   `cdc_dkadr` is the corrected `DKADR` map (see "DKADR physical addressing"
   below; NOT the superseded `72*L` or `077300` forms). Overlay 032 (window
   072000, logical `0244`) lands at physical sector `0o660`, matching the running
   reader's `IOX 503` exactly. The overlay bodies in the `VOR` windows are **byte-verified** fully
   fixed-up (see next paragraph).

   The `VOR` windows themselves hold the **fully fixed-up** overlay bodies: an
   instrumented build showed **zero** forward-reference or literal fixups still
   unresolved inside any overlay's source window at the `)9MOVE` snapshot
   point (every overlay's internal labels and its `)FILL`'d literals are
   defined before the closing `OVERX`). So the "snapshot captures un-patched
   words" hazard does **not** occur for this corpus; the disc image is
   faithful. Pinned by `test_mac.c [12]` (forward-ref-vs-`)9MOVE`-snapshot).
   The exact MACM mechanism on real ND hardware is still not in these files
   (MACM manual `ND-60.009.02`), but it is no longer needed.
2. **Runtime value of `DKBAS`.** Declared `DKBAS,0` (`TSS1.SYMB:171`; address
   `000015` in the dump). The NCR/SDISK paths add `DKBAS` to the block address;
   the CDC `XDISK`/`DKADR` path does not use it for overlays. Whether anything
   patches `DKBAS` to non-zero at boot was not traced. For the CDC overlay path
   it does not enter the address computation.
3. **Exact `DKADR` linear→physical output.** ~~COULD NOT DETERMINE~~ ~~`72*L`~~
   ~~`8*floor(L*65537/12)+64*L`~~ **RESOLVED (corrected 2026-07-21)** — see the
   section "DKADR physical addressing" below. The real map is
   `DKADR(L) = 32*floor(L/12) + 2*(L mod 12)` (verified per-instruction from the
   live machine, `DKADR(0244)=0o660`). Both earlier formulas came from tracing a
   broken `DKADR`: `72*L` (the `RGDIV` divide was undefined) and `077300` (mac-c
   miscompiled the `SHR` shift modifier). The `SHR` assembler bug is now fixed in
   `mac-c/mac.c` `eval_expr`, so `DKADR`'s three `SHR` shifts assemble correctly
   and its legality clamp passes; the mac-c overlay writer and the nd100x CDC
   device are aligned to the corrected map.
4. **The N10-build address of `ROVER`** (and other labels). The golden dump is
   the NN10 A build (`ROVER=031522`); the N10/DRUM build shifts addresses. Read
   `ROVER` from the DRUM build's own `)LIST`/symbol dump (`DSYMB`). Overlay
   numbers and `OVDK` are unaffected.
5. **`OV15A` used size (`QO15A`)** — present in the dump as `QO15A` (not
   `QOV15A`); value not transcribed into the table above. Read from the dump if
   needed.

---

## DKADR physical addressing

> **CORRECTED (2026-07-21): `DKADR(L) = 32*floor(L/12) + 2*(L mod 12)`.**
> Two earlier formulas are superseded:
> * `72*L` — traced while `DKADR`'s divide-by-12 (`RGDIV ST`) was UNDEFINED, so
>   the divide never ran (fixed by `RGDIV=RDIV`, `TSS1.SYMB:3616`).
> * `8*floor(L*65537/12)+64*L = 077300` — traced while the mac-c assembler
>   MISCOMPILED the `SHR` shift modifier as an additive LEFT shift. That bug is
>   now fixed (see below), so `DKADR`'s three `SHR` shifts assemble correctly and
>   the routine runs its TRUE algorithm.

### The `SHR` assembler fix (root cause of the second wrong formula)

Per `ND-60.096.01` §2.3.8 ("`SHR` — Shift right, gives negative shift counter.
Note that `SHR` must precede the specified shift counter"), `SHR` makes the
7-bit shift **counter negative**. MAC's permanent-symbol value for `SHR` is
`0200` (verified in `MAC.BPUN`'s permsym table at `0xE9B3`), which is
bit-identical to `SHD`'s register-select bit — so treating `SHR` as a plain
additive `0200` turned every `… SHR n` right shift into a LEFT shift of a
different register (`SHA SHR 6` → `0154606` = `SAD 6`).

mac-c now handles `SHR` in `eval_expr` (`mac-c/mac.c`): `SHR` still contributes
its `0200`, then the **following** shift-count term is SUBTRACTED, forming a
7-bit two's-complement negative counter. `ROT`/`ZIN`/`LIN` (the shift-type bits
9–10) stay additive. Pinned by `test_mac.c` (`SHA SHR 6`=`0154572`,
`SHA ZIN SHR 1`=`0156577`, `SAD ZIN SHR 20`=`0156760`, …).

Inside `DKADR` this repairs three shifts:

| source (`TSS1.SYMB`) | PC | wrong (additive `SHR`) | correct (`SHR` negates) |
|---|---|---|---|
| `SAD ZIN SHR 20` (3617) | 010016 | `0157020` (left, no-op) | `0156760` (AD right 16) |
| `SHT ZIN SHR 5` (3624)  | 010026 | `0156205` (left) | `0156173` (T right 5) |
| `SHA SHR 6` (3637)      | 010046 | `0154606` = `SAD 6` (left) | `0154572` (A right 6) |

### The corrected mapping (unit 0), verified per-instruction from the live machine

The nd100x boot trace of the DRUM build (`--mms1 --start=000301 --trace`) shows
the running disc driver call `DKADR` at address **010006**. Every register value
below was read off the live `--trace` (A/D/T columns) for logical sector `0244`.
Registers are 16-bit; `AD` is the 32-bit double accumulator (`A` high, `D` low):

```
010012 AND (17777    A := L & 017777
010013 COPY DD SA    D := A = L
010016 SAD ZIN SHR 20  AD >>= 16 : with A=D=L, AD=(L<<16)|L -> A=0, D=L  (dividend = L)
010017 SAT 14        T := 12 (decimal); the divisor
010020 RDIV ST       AD/T:  Q := L/12 , R := L mod 12       (A=Q, D=R)
010030 SHT 5         T := (Q << 5) & 0xFFFF = 32*Q
010032 RSUB DD SA    D := (L - 12*Q) & 0xFFFF = R
010033 RADD DD SD    D := (D + D) & 0xFFFF = 2*R
010034 RADD DT SD    T := (T + D) & 0xFFFF = 32*Q + 2*R      <-- RESULT (returned in A)
```

Closed form, all arithmetic mod 2^16:

```
DKADR(L) = 32*floor(L/12) + 2*(L mod 12)
```

This is a base-12 → `track*32 + 2*sector` repack (12 = sectors per track).

**[VERIFIED live]** `DKADR(0244) = 32*13 + 2*8 = 432 = 0o660`, seen loaded into
the CDC block-address register as `IOX 503 A=000660`. The unit-1 displacement
`0o52600` (`TSS1.SYMB:3632`) is never added (overlay disc is unit 0), and the
returned value is always `T` (`TSS1.SYMB:3642 COPY DA ST; EXIT AD1`). The
`DKLIM=626` clamp extracts the cylinder via the now-correct `SHA SHR 6`
(`A := T>>6 = 6`) and passes (6 < 626).

**Dense, small:** over the overlay logical range `0160..0257` the physical
sector runs `0o450..0o712` (max `458` dec). The writer still **scans all**
overlay sectors to size the image (correct for any mapping).

### The writer/reader alignment fix

* **Writer** — `mac-c/mac.c` `cdc_dkadr(L)` implements the closed form above
  (public, declared in `mac.h`; every ported step cites its `TSS1.SYMB` line and
  the live-trace PC). `mac_write_cdc_disc` places each overlay sector at
  `cdc_dkadr(logical)` and sizes the image by scanning all overlay sectors for
  the true max (`458+1 = 459` sectors → 235 008 bytes). The stderr table prints
  logical + physical per overlay plus a `max phys sector` summary line.
* **Device** — `nd100x/src/devices/cdc/deviceCDC.c` stays a dumb
  linear-by-**physical**-sector store (`cdc_lba_to_sector` is the identity);
  `DKADR` is applied once, at write time. `CDC_DEFAULT_SECTORS` lowered to `512`
  (256 KiB) to cover the corrected overlay range with headroom; the surface also
  grows to the backing-file size if a larger image is attached.

**[VERIFIED live]** With the aligned writer, overlay 032 (logical `0244`) is
placed at physical `0o660`, exactly where the running reader's `IOX 503` reads
it. Writer == reader.

### Appendix: reusable script

`E:\Dev\Ronny\TSS\mac-c\overlay_layout.py` — reads a `)LIST` symbol dump
(`reference/ASYMB.SYMB`) and prints the overlay→disc-sector table using the
verified `OVDK + 2*n` contract. Read-only; does not touch mac.c/main.c or any
TSS source.
