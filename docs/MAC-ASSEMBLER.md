# The ND MAC Assembler Family — Analysis and mac-c Defect Record

This is the single reference for (a) the historical Norsk Data MAC/FMAC/MACM
assembler family and its reverse engineering, and (b) the complete record of
defects found and fixed in **mac-c** (the C99 reimplementation) while
resurrecting NORD TSS 3.0 to a working login.

Everything marked **[VERIFIED]** was read out of a binary, a manual, the
golden dumps, or a live machine run. **[ASSUMPTION]** marks interpretation.
All numeric values are **octal** unless marked hex (`0x…`) or "dec".

For mac-c usage, build commands and the implementation-status tables, see
[`../mac-c/README.md`](../mac-c/README.md) — that document stays authoritative
for the tool itself; this one is the analysis and defect-history companion.

---

## 1. Scope and primary sources

| source | what it is |
|---|---|
| `D:\ND\BPUN\MAC.BPUN` (28389 bytes) | the real 1978 MAC binary (48-bit-float build), loaded in Ghidra as an ND-100 BPUN; origin of every permanent-symbol value in `mac-c/src/mac_permsym.c` |
| `fmac-1920c.prog` | FMAC, 32-bit-float build — **the assembler that built TSS** (§2) |
| `f48mac-1408d.prog` | the same FMAC built for 48-bit floats |
| `MACM-1718L.BPUN` | MACM, the mass-storage assembler (§4) |
| `ND-60.096.01` MAC User's Guide | the language specification (in `E:\Dev\Ronny\NDInsight\Reference-Manuals\`) |
| `ND-60.009.02` | MACM manual (DGET, `)9BYTT`, `)ULIST`, `)SYSDF`) |
| `reference/ASYMB.SYMB`, `reference/BSYMB.SYMB` | the golden oracle: symbol-table dumps of the original 1978 TSS builds |
| `reference/LIST1`–`LIST5` | the original assembly listings — the only archive artifact containing the original **instruction words** (still unused as an oracle; §8) |

---

## 2. The assembler family, and which one built TSS

### 2.1 [VERIFIED] TSS 3.0 was assembled with FMAC-1920C (32-bit floats)

Four independent evidence lines converge:

| evidence | detail |
|---|---|
| `ASSYSA` line 1 is literally `FMAC` | the operator started FMAC, not MAC |
| `)9MOVE` is used in TSS3 | present **only** in the FMAC builds; absent from MAC and MACM |
| `)9TSS` closes both build scripts | present in MAC and FMAC, absent from MACM |
| `[` float constants occupy **2 words** | derived from symbol arithmetic on the archived dump *before* the binary was found, then confirmed: `fmac-1920c.prog` carries `2OR3=2`, `LDR=024000`, `STR=020000` — Appendix E's 32-bit markers |

`f48mac-1408d.prog` is the *same* assembler (identical size, identical
handler addresses) built for 48-bit floats (`2OR3=3`, `LDR=034000`). On the
system where both are installed, the plain name **`FMAC` is the 1920C 32-bit
build** and the 48-bit one is explicitly `F48MAC` — which is why `ASSYSA`
saying `FMAC` yields 2-word floats.

### 2.2 Command inventory across the family [VERIFIED, decoded from each binary]

| command | MAC | FMAC | MACM | meaning |
|---|:--:|:--:|:--:|---|
| `)9ASSM )FILL )KILL )LINE )WRITE )WRTM )NWRT )WRUS )WLOC )WMNE )BPUN )CLEAR )PRINT )PUNCH )9SET )9LIB )9MSG )9RT )9LC )9ASF )9ADS )9EOF )9ENT )9EXT )9END )9BEG )9PARI )9LITR )9ASCI )9FABS )9EXIT` | ● | ● | ● | common core |
| `)9TSS` | ● | ● | – | alias of `)9EXIT` (same handler; §3.4) |
| `)9MOVE` | – | ● | – | **block move**, used by TSS3's `OVERX` |
| `)ULIST )SYSDF` | – | ● | ● | undefined-ref punch; system-definition mode |
| `)GJEM )HENT )SAM )CMOVE )BPUND )CLOAD )FIX )XPUN )9READ )9BYTT )9CTOM )9SAVE )9GET )RBP` | – | – | ● | MACM's mass-storage command set |
| `)ZERO )PCL )CORE )LIST )SETSM )RESSM )MCDEF )LSTM )9TABL` | ● | – | ● | option commands |

### 2.3 `)SOVER` and `)8DUMP` are not commands — they are `)SYMBOL` forms

**[VERIFIED]** `)SOVER` and `)8DUMP` are commands in **none** of the three
assemblers. They are MAC's `)SYMBOL` form — *"causes a jump to the address
given by the value of the symbol"* (ND-60.096.01 §3.2.3.9) — invoking
routines TSS assembles itself (`SOVER,` at TSS3:37, `8DUMP,` at TSS3:195 and
TDUMP:221), which return to the assembler via `JMP 103,B`. Both live only in
the `"NMACF` / `"TSBIN` conditional paths, which no golden or DRUM build ever
assembles, so mac-c correctly treats them as documented no-ops
(see `../mac-c/README.md` "Not implemented").

Related correction: **`)SCRATCH` and `)FRIEND` are not commands at all.**
**[VERIFIED]** they appear in the corpus only *inside quoted text strings*
(`src\TSS4.SYMB:544` `'()SCRATCH\ '`, `src\TSS4.SYMB:1714`
`...)FRIEND NAME: ]`); an earlier inventory misclassified them as ignored
commands.

---

## 3. MAC.BPUN reverse engineering

Binary: `D:\ND\BPUN\MAC.BPUN`, analyzed 2026-07-19 with the Ghidra MCP tools.

### 3.1 Identity

- **MAC — 14. MARS 1978** — version string in embedded listing pages
  (ram:cde7, ram:de64) **[VERIFIED]**.
- Loads at octal **145000–177777** (ram:ca00–ffff, 13.5K words at the top of
  the 64K address space) — the classic MAC placement that leaves 0–144777
  free for the program being assembled **[VERIFIED load segment]**.
- **SINTRAN III subsystem variant**: all console/stream I/O goes through
  monitor calls — MON 1 INBT / MON 2 OUTBT with T = logical device number,
  MON 65 QERMS after each, MON 43 CLOSE, MON 0 LEAVE. No direct console IOX
  exists in the image **[VERIFIED scan]**. The only direct IOX is the
  paper-tape reader (400/402/403) inside the BPUN loader template.
- Options compiled in, from error-message strings **[VERIFIED]**:
  assembler core ("ILL. MNEMONIC", ")FILL MISSING", "ILL. EXPRESSION"),
  **BRF output** ("ILL. BRF UNIT INITIATION"), **debugger with breakpoints
  and disassembler** ("ILL. BREAKPOINT", "DISASSEMBLER ERROR"),
  `)9ASSM` diagnostics counter ("…Y DIAGNOSTICS"), MAC-monitor error set
  ("$PRIV INST", "$ILL MCALL", "$ILL IOT OR DEVNO", "$FATAL ERR").

### 3.2 Stale listing pages with the TSS symbol set

The image is a **memory dump**, and its print buffers still hold pages of an
old MAC listing — of what appears to be **MAC's own source** (command-name →
handler table: `WRTM→WRMO`, `WRITE→WRRR`, `KILL→KRRR`, `BPUN→…`), including
permanent-symbol definitions at source lines 601–609:

```
GRP2=2  GRP1=1  INTDS=150440  INTEN=150540  MPR=3
SKA=1000  ACT=400  PIN=2000  SNI=0
```

**[VERIFIED text; interpretation ASSUMPTION]** This is *exactly* the
"Added by CVS" symbol set from `ASSYSA.SYMB`. Most plausible reading: this
MAC binary was rebuilt (or at least re-listed) with the TSS/NORD-1 symbols
patched into its permanent symbol table — i.e. **this BPUN comes from the
same TSS restoration effort** and may already know these symbols without the
ASSYSA preamble. The preamble redefinitions would then be harmless
(or produce ALREADY DEFINED warnings). Needs a runtime test to confirm.
Together with the `)9TSS` finding (§3.4) this proves the binary is a custom
TSS-restoration MAC.

### 3.3 Layout (as mapped)

| Address (hex / octal) | Content |
|---|---|
| ca00+ / 145000+ | Data cells + stale print-buffer fragments |
| cde7, de64 | Stale listing pages ("MAC - 14. MARS 1978", PAGE 11) |
| df0a–e02f | Input stream layer: MON 1 INBT char read, bit-7 strip, TAB/LF/CR handling, buffer pointers df47–df4b |
| e030–e0b6 | Terminal line editor, two inline instances (buffers e086+/e0b7+): BEL on empty, `^` char-delete echo, `_`+CRLF line kill, SBYT into line buffer |
| e115 | "CPTADXLBSOR" — register-name list for the debugger register dump |
| e1b2–e1f0 | Assembler error strings (set 1) |
| e78b–e7fc | MAC-monitor "$..." error strings (set 1) |
| e8ce–e8ee | `bpun_tape_block_loader` — BPUN block loader (addr, count, data, checksum; WAIT 77 on mismatch), reads PTR via `ptr_read_word_2frames` / `ptr_read_frame` |
| e8f2–e9b3 | Assembler error strings (set 2) — belongs to the `)BPUN` punch template region **[ASSUMPTION]** |
| e9b3–ebe6 | **Permanent symbol table + command table** (§3.4) |
| ed00 | ")9ASSM …Y DIAGNOSTICS" message |
| f84d, fa91 | Debugger strings: breakpoints, disassembler |
| fe9f | "CHECKSUM ERROR" (`)9READ` loader) |
| ff18–ff7f | MAC-monitor "$..." error strings (set 2) |

Functions renamed in the Ghidra project: `bpun_tape_block_loader` (e8ce),
`ptr_read_word_2frames` (e8e0), `ptr_read_frame` (e8e8), `MAC_COLD_START`
(f7d4), plus 17 `cmd_*` command handlers. Most of the image is still
undisassembled — hand-written MAC assembly with data interleaved.

### 3.4 The permanent symbol table and command dispatcher [VERIFIED]

**Permanent symbol table** at 0xE9B3–0xEBE6 (octal 164663+), pointed to by
the pointer block at 0xE842 (base, twice) / 0xE844 (end):

- Entry = 3 words: `[name_hi, name_lo, value]`.
- Name encoding: up to **5 characters, 6 bits each (ASCII & 077)**,
  right-justified in the 30-bit field spanning the two name words.
  Verified examples: IOF=[0000,93C6,D101], TRA=[0001,4481,D000],
  MON=[0000,D3CE,D600], EXIT=[0015,8254,CC62] (=146142, matches the
  manual's EXIT value).
- E9B3–EB7E: the full **NORD-10/ND-100 instruction set** as symbols
  (LDA=044000 … IOX=164000 — IOX, not NORD-1 IOT — MON=153000, IRW, IRR,
  SRB/LRB, IDENT + PL10–13, POF/PON, RMPY/RDIV/MIX3, SBYT/LBYT, EXR,
  internal registers PIE/PID/PCR/PVL/IIC/IIE/ALD/…).
- EB81–EBE4: the **command table** — same entry format, value = handler
  address (octal, hex in parentheses):

  | Command | Handler | Command | Handler |
  |---|---|---|---|
  | FILL | 171504 (F344) | 9SET | 167375 (EEFD) |
  | WRUS | 171157 (F26F) | 9FABS | 167532 (EF5A) |
  | LINE | 166344 (ECE4) | 9LIB | 167116 (EE4E) |
  | PUNCH | 172563 (F573) | 9MSG/9MSGE | 172772 (F5FA) |
  | PRINT | 172557 (F56F) | 9RT | 167516 (EF4E) |
  | NWRT | 170034 (F01C) | 9LC | 167503 (EF43) |
  | WRTM | 170032 (F01A) | 9ASF | 167507 (EF47) |
  | WRITE | 170134 (F05C) | 9ADS | 167524 (EF54) |
  | KILL | 170026 (F016) | 9EOF | 166511 (ED49) |
  | BPUN | 172645 (F5A5) | 9ENT | 167537 (EF5F) |
  | WLOC | 171207 (F287) | 9EXT | 167535 (EF5D) |
  | WMNE | 171205 (F285) | 9END | 166551 (ED69) |
  | CLEAR | 170002 (F002) | 9BEG | 167475 (EF3D) |
  | 9PARI | 166151 (EC69) | 9ASSM | 173321 (F6D1) |
  | 9LITR | 171666 (F3B6) | **9EXIT** | **173411 (F709)** |
  | 9ASCI | 172552 (F56A) | **9TSS** | **173411 (F709)** |

- **`)9TSS` mystery solved**: `9TSS` and `9EXIT` share handler F709, which
  is simply `MON 0` (LEAVE). `)9TSS` is a **custom alias added for the TSS
  restoration** so the historic ASSYSA script terminates cleanly — no
  standard MAC has a 9TSS command.
- The permanent table does **NOT** contain GRP1/GRP2/INTDS/… — so the ASSYSA
  preamble definitions ARE required; they go into the **local symbol table**
  at octal 104000–120402 (0x8800–0xA182, per the pointer block), which lies
  **below** the dumped range — that is why no user symbols are in this file.

### 3.5 BPUN tape structure and native boot [VERIFIED by parsing the file]

```
offset 0-203     null leader
offset 204-507   OCTAL-ASCII section (chars carry parity bit):
                   164316/          <- set deposit address (0xE8CE)
                   146101 ... 000000  <- 36 octal words = the block loader
                   164316!          <- start execution at 164316
offset 508-511   binary block header: load addr 145000, count 33000 octal
                                     (13824 words = the whole image)
offset 512-28159 image data ca00..ffff (big-endian words)
offset 28160     checksum 057774 - verified, matches
then             null trailer
```

Boot flow on bare metal: feed the octal section through the console loader
(OPCOM octal load from the tape reader); it deposits and starts the 36-word
block loader at 164316; the loader reads the single binary block into
145000–177777 (overwriting itself with identical bytes), verifies the
checksum (WAIT 77 halt on error), then does `JMP I 164361` (e8f1) — and the
start word at e8f1 **is 0 on this tape**: no auto-start address was set when
it was punched (confirmed against the emulator's BPUN loader
"Action: 0000000"), so a native boot ends with a jump to location 0 and the
operator was expected to start MAC manually.

**This MAC cannot run without SINTRAN.** Verified: every I/O path is
MON 1/MON 2 (+ MON 65 QERMS), `)9EXIT`/`)9TSS` is literally `MON 0` (LEAVE),
and there is no console IOX and no MON trap handler in the image.

### 3.6 Paper-tape I/O capabilities [VERIFIED]

- **Reading: YES, natively.** Two direct-IOX reader routines (device 400,
  IOX 400/402/403) exist: the BPUN block loader at e8ce, and an internal
  copy at e1a0/e1a8 inside MAC proper — the `)9READ` binary-load path.
  Tape reading needs no OS.
- **Punching: MON 2 only.** No punch IOX (411/412/413 → words E909/E90A/E90B)
  exists anywhere in the image, at any byte alignment. `)PUNCH`/`)BPUN`
  write through SINTRAN (MON 2 to the punch's logical device number).
  cmd_PUNCH/cmd_BPUN reference the loader template (E8CE), the start-word
  cell (E8F1) and the string block (E8F2) via the global pointer pool at
  E860–E8CD — i.e. `)BPUN` copies that template onto every tape it makes.
- E8AC–E8C0 is the **error-message pointer table** (error code → string
  address in e8f2–e9a5). The e1a0/e1b2 region is a second, partly-zeroed
  copy of loader + error strings (template vs live copy).

### 3.7 No native-capable start address exists [VERIFIED: no]

Searched for one specifically; the answer is no, for three independent
reasons:
1. The only OS-free I/O in the image is tape-reader INPUT (IOX 400) — the
   block loader and the `)9READ` copy at e1a0. Console I/O is MON 1/2
   exclusively (no console IOX at any alignment), punching is MON 2.
2. MAC's code is **globally B-relative** (`JPL 0x11,B`, `LDA 0xa,B` — see
   cmd_LINE at ECE4): any entry requires the base register B set up by the
   cold-start path first, and that path talks to the terminal via MON almost
   immediately.
3. There is no restart-after-LEAVE entry: only one real `MON 0` call site
   exists (9EXIT at F709); the other two D600 words are the symbol-table
   value for MON (eb3b) and an opcode-constant table at e57f.
The only thing that runs natively is the block loader at 164316 itself.

Practical no-SINTRAN options, in order of preference:
1. **Emulator MON shim** (no binary modification): trap MON 0/1/2/43/65
   (+ the 15/25/42 trio, §3.8) in nd100x/RetroCore and service them
   host-side. The MON surface is tiny.
2. **Binary patch**: all ~30 MON call sites are located; replace them with
   JPLs to IOX console/reader/punch routines placed in free RAM below
   145000 (needs a second BPUN block to load the shim).

### 3.8 Complete MON call inventory [VERIFIED — word-aligned scan + code context]

All real MON call sites (data-word false positives excluded: the e57f opcode
table, the eb3b symbol-table value, plus odd-alignment noise). Names from
ND-860228-2 SINTRAN III Monitor Calls:

| MON (octal) | Name | Sites | Where / purpose |
|---|---|---|---|
| 0 | LEAVE | f709 | `)9EXIT` / `)9TSS` |
| 1 | INBT | df18, e036, e067 | stream input; terminal line editors |
| 2 | OUTBT | df43, e03f, e044, e047, e04a, e057, e070, e075, e078, e07b, e088 | all output (terminal, list, punch) |
| 3 | ECHOM | de9f, ded0 | set echo mode (with SAA 2 = echo strategy) |
| 4 | BRKM | dea0, ded1 | set break mode |
| 7 | RPAGE | df0e | read 256-word block (scratch-file / image page) |
| 13 | CIBUF | f706 | clear input buffer (exit path) |
| 43 | CLOSE | deb0, dee1 | close file (two stream-teardown paths) |
| 64 | ERMSG | ed30, f6c1 | warning message |
| 65 | QERMS | 14 sites (paired after MON 1/2 etc.) | error message + abort check |
| 143 | RSIO | f705 | get execution mode + command I/O devices (exit/init path) |
| **15** | **not in any manual** | f6af | `)9ASSM` file machinery — takes 6-bit-packed name in A/D (`SAD ZIN SHR 20`) |
| **25** | **not in any manual** | f6b3 | same block, same packed-name convention |
| **42** | **not in any manual** | f6b8 | same block, has error skip-return (`JMP 2`) — the actual open? |

MON 15/25/42 are absent from ND-860228-2 (the table skips 15B, 25B, 42B),
ND-60.050.06, and the VSX documentation. Two hypotheses **[ASSUMPTION]**:
(a) genuine 1978-era SINTRAN calls dropped from later manuals, or
(b) **custom monitor calls implemented by the restoration's SINTRAN/shim**,
taking MAC's native 6-bit packed file names directly (the same packing as
the symbol table). Given the proven 9TSS customization, (b) is plausible.
A MON shim for no-SINTRAN operation must implement: 0, 1, 2, 3, 4, 7, 13,
43, 64, 65, 143 + the 15/25/42 trio (open/create/connect semantics to be
reverse-engineered from their parameter blocks at f6aa–f6b9).

### 3.9 Cold-start entry — SOLVED, VERIFIED DYNAMICALLY

(nd100x `--boot=bpun`, DAP session 2026-07-19):
**MAC cold-start entry = octal 173724 (0xF7D4)**. Sequence:
`LDX (stack=F84E); LDA (E87B); COPY SA,DB; JPL 0x28,B` — i.e. **B is loaded
with E87B: the global pointer pool IS the base register**, and all `nn,B`
accesses across MAC index that pool. First action after init: prints CR
(0215) to terminal 1 via `SAT 1; MON 2` — the characteristic startup CRLF
from the manual. Ghidra: function `MAC_COLD_START`.

Native behavior confirmed: with PIE=0, MON falls through as a NOP and MAC
loops forever retrying its first CR in the output loop ded8–df44 — so the
MON shim is required exactly as predicted, and 173724 is the address to
start once MON 0/1/2/43/65 are serviced.

### 3.10 Remaining open RE questions

- The main parse-loop / dispatcher code (the routine that packs a parsed
  name into 6-bit format and walks the table at [E842]) is not yet pinned —
  trace backward from the `cmd_*` handlers or the E030 line editor.
- How `)9ASSM` opens named files (no MON 50 in the image) — the MON 15/25/42
  trio's semantics (§3.8).
- Whether this BPUN's permanent table really contains the TSS symbol set the
  stale listing pages suggest (§3.2) — needs a runtime test.

---

## 4. MACM — the mass-storage assembler

**MACM assembles into a core image on mass storage, not into core.** From
ND-60.009.02: *"the main difference is its ability to assemble programs out
on a mass storage device (drum) in a core image format"*, and decisively:
*"accessing assembled code must be done using a subroutine (**DGET**) while
in MAC this can be done directly."* That one sentence is the entire
architectural difference.

### 4.1 Storage model

```
  MACAD -> save area for MACM itself
  DASA  -> "GJEM/HENT" area
  DRES  -> core-resident core image  (16K)
  BLST  -> coreload #1, #2, #3 ...
```

A *core image* = core-resident part + the current *coreload*. Coreload 0
means the coreload area of the resident image, giving one contiguous image.
Ten parameters configure this, set by `)9BYTT` so one binary serves drum or
disc: `MSTYP DEVNO CORAD LONG CLM BLST DRES CRMAX MACAD DASA`.

Drum addressing has three distinct concepts — *drum address*, *hardware
drum address* (`32·S + ((B + S) mod 32)`, rotated one block per track to
avoid a full rotation of latency) and SINTRAN's *software address*
(= ¼ · drum address).

### 4.2 Ghidra work completed on `MACM-1718L.BPUN`

Fifteen command handlers located via the decoded command table and named:

| function | address | command |
|---|---|---|
| `cmd_GJEM_save_core_image` | ram:990b | `)GJEM` / `)9STOR` |
| `cmd_HENT_restore_core_image` | ram:9913 | `)HENT` / `)9REST` |
| `cmd_SAM_compare_image_vs_save` | ram:99c1 | `)SAM` / `)9COMP` |
| `cmd_CMOVE_copy_coreload` | ram:9a1e | `)CMOVE N1 N2` |
| `cmd_BPUND_punch_rebootable_tape` | ram:9c5b | `)BPUND` |
| `cmd_CLOAD_set_coreload_number` | ram:9ce6 | `)CLOAD N1` |
| `cmd_XPUN_punch_MACM_itself` | ram:9fd5 | `)XPUN` |
| `cmd_9BYTT_set_basic_parameters` | ram:a07c | `)9BYTT` ×10 |
| `cmd_9SAVE_coreimage_to_disc` | ram:a019 | `)9SAVE A,B,C` |
| `cmd_9GET_disc_to_coreimage` | ram:a01c | `)9GET A,B,C` |
| `cmd_ULIST_punch_undefined_refs` | ram:92b2 | `)ULIST` |
| `cmd_SYSDF_system_definition_mode` | ram:8d52 | `)SYSDF` |
| `cmd_9READ_load_binary_tape` | ram:b474 | `)9READ` |
| `cmd_FIX_make_symbols_permanent` | ram:b4b6 | `)FIX` |
| `cmd_RBP_reset_breakpoints` | ram:98ce | `)RBP` |
| `cmd_LINE_and_9CTOM_flush_buffer` | ram:88b4 | `)LINE`, `)9CTOM` share a handler |

The core-image engine was located at **ram:9d27** (maps a core-image
address to block + offset: `MPY` by block size, then `SAD ZIN` / `SHD ZIN
SHR` to split) and **ram:9d4b** (the block-buffer manager: compares the
wanted block against the one held at `ram:9d97`, writes back and reloads on
a miss). `)9BYTT` stores its ten parameters at **ram:834c**, all zero in
this shipped tape because it is unconfigured until `)9BYTT` runs.

**[ASSUMPTION]** the individual field order at ram:834c follows the manual's
listed order; the offsets are not yet individually confirmed against reads.

---

## 5. mac-c design essentials

Full status and usage in [`../mac-c/README.md`](../mac-c/README.md); this is
the one-page architecture summary the defect record (§6) builds on.

- **One pass with fixup chains, deliberately — not two passes.** MAC's
  `)KILL`/`)PCL` give symbols *time-ordered* scope (TSS reuses names like
  `TCL` per routine), which a second pass would destroy. Forward references
  are recorded in `g_pending` (per-symbol chains) and patched when the
  symbol is defined; literal references go in `g_litrefs` and are patched at
  `)FILL`. **Each fixup carries the expression's constant part in
  `mac_fixup.addend`** (`mac-c/src/mac.h:126`) — PREL8 patches
  `((sym+addend)-pc) & 0377`, ARG8 `(sym+addend) & 0377`, FULL pre-stores
  the constant in the word and the patch adds. This rule exists because of
  defect §6.6.
- **Statement dispatch order in `assemble_stmt()` is load-bearing.** It is
  interval `<` → text string `'…'` → location-set `EXPR/` → label `NAME,` →
  assignment `NAME=` → `first_token` → macro call → float `[` → data
  literal `(` → instruction/data. Moving the location-set check after the
  label check makes `DEVTB+NTTY+1/OEV,…` parse as a label that truncates to
  `DEVTB` and silently redefines it.
- **Every scanner must skip `#ab` character constants** — the comment strip,
  the `;` splitter, and the `/`, `,` and `<` scans. `#` plus *two*
  characters is one word `(a<<8)|b`, and the second character is frequently
  a blank, `;`, `'` or `<`. Use `skip_char_const()`; do not hand-roll it.
- **Streams** follow MAC: source / list / object. `)LIST` writes to the
  **object** stream (that is how `)9ASSM TSS5,LIST5,ASYMB:SYMB` + `)LIST`
  produces the archived dump); reports go to the list stream; `0` selects
  the dummy device (NULL). `main()` must call `mac_close_streams()` or
  buffered object output is lost.
- **`mac_permsym.c` is generated data — do not hand-edit.** Provenance:
  extracted from `MAC.BPUN`'s table at 0xE9B3 — the **48-bit-float** build.
  FMAC-1920C (which built TSS) differs on three entries: `STR`
  (030000 vs 020000), `LDR` (034000 vs 024000), `2OR3` (3 vs 2).
  **[VERIFIED 2026-07-23]** `mac-c/src/mac_permsym.c:22-23` still carries
  `LDR=034000`, `STR=030000` — the FMAC re-audit remains **open** (§8).
  Current corpus impact is nil: after the `STR→XTR` rename completion
  (§6.5), `STR` appears in `src/*.SYMB` only inside `%` comments
  **[VERIFIED by grep 2026-07-23]**, so no corpus word is currently
  mis-encoded by it. The float default (32-bit, 2-word `[` constants) is
  already correct for TSS; `mac_state.float48` selects the 48-bit variant.

### 5.1 The overlay pipeline (assembler side)

TSS stages its 31 code overlays at assembly time with the `"MACF` variant of
the `OVERX` macro: **`)9MOVE ROVER VOR VORS`** (TSS3:83) — a verbatim block
copy, no relocation — moves each finished 1000-word overlay from its
assembly window at `ROVER` into a distinct `VOR` slot (040000, 041000, …
076000) so all 31 survive in the image. **Overlays both assemble AND execute
at `ROVER`** — the 40000 `VOR` window is build-time staging only, which is
why a verbatim copy is correct (internal references stay `ROVER`-relative).
**[VERIFIED]** an instrumented build showed zero unresolved forward-reference
or literal fixups inside any overlay window at the `)9MOVE` snapshot point;
pinned by `test_mac.c [12]`.

`mac_write_cdc_disc` (`mac-c/src/mac_bpun.c`, `mac-as -c FILE`) then writes each
overlay *n* to CDC-disc sectors `OVDK+2n` / `OVDK+2n+1`, applying the
linear→physical map **at write time**:

```
DKADR(L) = 32*floor(L/12) + 2*(L mod 12)      (12 = sectors per track)
```

**[VERIFIED live]** per-instruction from the running driver's trace
(`DKADR(0244) = 0o660`, seen loaded into `IOX 503`). The nd100x CDC device
stays a dumb linear-by-physical-sector store; DKADR is applied exactly once,
by the writer, so writer == reader. The overlay→disc layout tables and the
run-time reader (`S5`, `GOVX`, `OVDK`, `ROVER`/`ROV4`) are documented in
[`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md).

### 5.2 Intentionally not implemented

- `)SOVER` / `)8DUMP` — `)SYMBOL`-form invocations of TSS's own assembled
  routines (§2.3); would need ND-100 execution, and are compiled out of
  every build mac-c targets. The disc contract they implement is reproduced
  directly by `)9MOVE` + the CDC image writer (§5.1).
- BRF relocatable object output (`)9BEG )9END )9ENT )9EXT )9LIB )9FABS
  )9EOF )9ASF )9ADS )9LC )9RT`) — TSS is an absolute assembly.
- The interactive debugger commands (`.` `!` `\`) — debugger features, not
  assembly.
- `)ULIST` / `)SYSDF` (FMAC/MACM; ND-60.009.02 §3.8–3.9) — unused by the
  corpus.

---

## 6. THE DEFECT RECORD — mac-c bugs found via TSS

Each entry: symptom → mechanism → why the golden dumps could not see it →
fix → pinning test. Dates are the fix dates. The unifying theme is stated
at the end (§6.8); the golden symbol dumps are an **address** oracle, and an
entire class of code-generation bugs is invisible to it.

### 6.1 The `9377` octal-parse bug — the login char-killer (2026-07-22)

- **Symptom:** typing at the TSS login prompt produced total silence — no
  echo, `LOGON` never saw a character. `RBUF` returned 0 forever, `TCI`
  spun.
- **Mechanism:** in mac-c's tokenizer (`mac.c:289-306` at the time; now `src/mac_expr.c`),
  all-digit tokens were classified as numbers with `isdigit()` (0–9) but
  parsed as **octal** with `strtol(…, 8)`. The TSS *symbol* `9377` (the
  `377` byte-mask literal used by the teletype ring routines `WBUF`/`RBUF`,
  defined at `TSS1.SYMB:407`, value 0o747 in the image) hit
  `strtol("9377", …, 8)`, which stops at the non-octal `9` and returns
  **0**. So `AND 9377` assembled as `AND 0` — the operand became the
  instruction word itself (070000), and `char AND 070000 = 0`: **every
  console character was zeroed** on the way into the type-ahead buffer.
- **Why the golden dumps were blind:** `9377` is *defined* fine — its
  address matches the dump exactly. Only the *references* mis-encoded;
  addresses and word counts were all correct.
- **Fix:** the octal branch requires octal digits only:
  `bool valid = decimal ? isdigit(c) : (c >= '0' && c <= '7');`
- **Pinning test:** `test_mac.c` §[7] regression (proven to fail
  `got 070000` when the bug is reintroduced). Live-verified: on the fixed
  image, typing `SYSTEM` echoes `@ENTER SYSTEM` and LOGON leaves its read
  loop.

### 6.2 The `SHR` shift-modifier negation bug (2026-07-21)

- **Symptom:** the disc driver's `DKADR` (linear→physical CDC sector
  conversion, `TSS1.SYMB:3563-3623`) computed garbage — one investigation
  derived the nonsense closed form `077300` for every input — so overlays
  were written to the wrong physical sectors.
- **Mechanism:** per ND-60.096.01 §2.3.8, *"`SHR` — Shift right, gives
  negative shift counter. Note that `SHR` must precede the specified shift
  counter."* MAC's permanent value for `SHR` is `0200` (verified in
  MAC.BPUN's table), bit-identical to `SHD`'s register-select bit. mac-c
  treated `SHR` as a plain additive `0200`, turning every `… SHR n` right
  shift into a **left** shift of a different register
  (`SHA SHR 6` → `0154606` = `SAD 6`). Three shifts inside `DKADR` were
  miscompiled:

  | source (`TSS1.SYMB`) | PC | wrong (additive) | correct (`SHR` negates) |
  |---|---|---|---|
  | `SAD ZIN SHR 20` (3617) | 010016 | `0157020` (left, no-op) | `0156760` (AD right 16) |
  | `SHT ZIN SHR 5` (3624)  | 010026 | `0156205` (left) | `0156173` (T right 5) |
  | `SHA SHR 6` (3637)      | 010046 | `0154606` = `SAD 6` | `0154572` (A right 6) |

- **Why the golden dumps were blind:** encoding-only; every symbol address
  and word count unchanged.
- **Fix:** `eval_expr` (`mac-c/src/mac_expr.c`) tracks a pending-`SHR` state
  (`shr_pending`): `SHR` still contributes its `0200`, then the
  **following** shift-count term is SUBTRACTED, forming the 7-bit
  two's-complement negative counter. `ROT`/`ZIN`/`LIN` (shift-type bits
  9–10) stay additive.
- **Pinning tests:** `SHA SHR 6`=`0154572`, `SHA ZIN SHR 1`=`0156577`,
  `SAD ZIN SHR 20`=`0156760`, … in `test_mac.c`. With the fix, `DKADR` runs
  its true algorithm — verified per-instruction on the live machine
  (`DKADR(0244)=0o660`, §5.1). (A related **TSS-source** issue found in the
  same hunt: `RGDIV` was undefined so `DKADR`'s divide never ran; fixed
  source-side with `RGDIV=RDIV`, `TSS1.SYMB:3616`.)

### 6.3 Background: the `COPY SA DT` zero-emit class (earliest of the family)

The archetype of the whole class, found long before the login work, during
exhaustive instruction testing:

- **Symptom:** none visible — the golden reconciliation score was good.
- **Mechanism:** `eval_expr` skipped blanks instead of treating them as term
  separators (ND-60.096.01 §2.5: elements are "separated by a single space,
  a plus sign or a minus sign"). `COPY SA DT` fused into one bogus symbol
  `COPYSADT` and emitted **`000000`** — every multi-term register
  instruction in the corpus was wrong, while every symbol address stayed
  correct because one (wrong) word was still one word.
- **Why the golden dumps were blind:** the defining example — matching
  symbol *addresses* only proves the word **count** is right.
- **Fix + fallout:** blanks became term separators; fixing it dropped the
  undefined-symbol count from 315 to 30 and immediately exposed two
  follow-on bugs (whitespace falling into the `@` shift branch; `#ab`
  constants breaking the scanners — see `../mac-c/README.md` "Bugs the
  exhaustive instruction tests exposed").
- **Pinning tests:** `test_mac.c` [1b]–[1d] assert the **encoding** of every
  memory-reference opcode × all 8 modes, every conditional jump, every
  argument instruction, and the summed register/IO forms — precisely so this
  class cannot recur silently.

### 6.4 The `CLD` permanent-symbol gap (2026-07-23)

- **Symptom:** `SWAP CLD DT SA` (clear-destination register operate)
  assembled without the `CLD` bit — silently wrong register-op words.
- **Mechanism:** the "byte-verified" permsym extraction from MAC.BPUN had
  missed the `CLD` modifier entirely; an undefined `CLD` contributed 0.
- **Why the golden dumps were blind:** encoding-only again — and `CLD` even
  appears in the expected-undefined list of older reconciliations, masking
  it as normal.
- **Fix:** `CLD = 0100` added to `mac_permsym.c` (documented as a corrected
  extraction gap, not a hand-edit deviation). **Validated on the REAL MAC by
  hand**: `SWAP CLD DT SA` → `144156`, `SWAP DT SA` → `144056`
  (diff exactly 0100); mac-c now reproduces both.
- **Pinning test:** `test_mac.c` [14].

### 6.5 The `STR→XTR` rename completion + the undefined-operand guard (2026-07-23)

Two halves of one fix — a corpus repair, and the guard that makes the whole
silent-zero-operand class a hard error:

- **Symptom:** `SAT STR1` and 47 similar sites silently assembled as
  `SAT 0`.
- **Mechanism:** `archive/TSSn.SYMB` is CVS's patched copy of the 1973
  source, with `STR`→`XTR` (and `LSS`→`XSS`) renamed because those names
  collide with MAC's permanent table — but the rename was **incomplete**:
  `STR`, `STR0`–`STR2`, `STRX`, `STR1X`, `STR2X` were referenced but never
  defined. An undefined symbol used as a value operand contributes 0.
- **Why the golden dumps were blind:** the six dangling names are among the
  20 expected-undefined symbols of the original build too, and word counts
  never moved.
- **Fix (corpus):** all 48 dangling `STR*` references in `src/TSS2–TSS5`
  completed to their `XTR*` definitions (`SAT STR1` → `SAT XTR1`); verified
  in the built overlay.
- **Fix (guard):** `used_as_operand` flag (`mac-c/src/mac.h:92` + `src/mac_stmt.c`) — a
  still-undefined symbol consumed as the VALUE OPERAND of a defined
  MRI/JUMP8/ARG8 instruction is now a **hard build error**
  ("undefined operand:"). Library marks are exempt (no false positives on
  the golden or drum builds). This closes the class prospectively: the next
  `SAT <undefined>` fails the build instead of emitting 0.
- **Pinning test:** `test_mac.c` [14] (guard + CLD together).

### 6.6 The forward-reference addend loss — the login-hang root cause (2026-07-23)

The canonical write-up of the bug that made interactive login hang after the
project number.

- **Symptom:** login reached `PROJECT NUMBER P-`, accepted the number, then
  hung — the CPU busy-looping at level 2 (not blocked). Initially
  mis-diagnosed as an overlay-load problem; a live DAP session proved the
  nested overlay load (`LOGON`(OV6) → `ABLKP`(OV1A) via GOVX/S5) works
  fine.
- **Mechanism:** mac-c's forward-reference fixups (`MAC_FIX_PREL8` /
  `MAC_FIX_ARG8`) replaced the displacement with `sym - pc`, **discarding
  the constant addend** of the operand expression: `JMP RFN+2` assembled as
  `JMP RFN`. In TSS2's `ROBJ`, all three error exits are forward jumps
  (`RF4/RF9: … JMP RFN+2`, `R2: … JMP RFN+3`) — all landed on `RFN`, so
  RF9's error 9 ("index out of range") fell through into `SAA -1` and
  became −1 ("empty object"). `GBARR` maps A<0 to a *successful*
  empty-string return, so `ABLKP`'s file-directory enumeration
  (LOGON → `OPEN "()SCRATCH"` → `FFILE` → ABLKP-with-GBARR) never received
  its terminating failure and enumerated forever (live index observed at
  124505).
- **Why the golden dumps were blind:** word counts and every symbol address
  unchanged — the same family as §6.3. `MAC_FIX_FULL` fixups were already
  correct (the constant is pre-stored in the word and the patch adds), which
  is why data-word forward references never showed the bug.
- **Fix:** `mac_fixup.addend` (`mac-c/src/mac.h:126`) carries the expression's
  constant part; PREL8 patches `(sym+addend)-pc`, ARG8 `(sym+addend)&0377`.
- **Pinning test:** `test_mac.c` [15], which reproduces the exact ROBJ
  shape. `bringup/check-robj-encoding.py` additionally byte-checks a built
  artifact for the fixed-vs-broken ROBJ pattern.
- **Verified result:** full bring-up + login on the fixed image:
  `@ENTER` → `SYSTEM` → project number → date prompt → **`@` command
  prompt**; `HELP` lists the ~60-command catalogue, `WHO-IS-ON` prints
  `1 SYSTEM`.

### 6.7 Addressing mode 6 encoded X-relative — the swapper hang (2026-07-27)

- **Symptom:** the `MEMORY` command never returned. The swapper's page scan
  ran all `NDPGS = 36` iterations, set neither index, and fell into `FTLER`,
  killing the process; the CPU then idled in `LEV0`. Measurement at the hang
  gave `IDXA = IDXB = -1`, `J = 36`, and 35 of 36 `PDTBL` frames free —
  values that contradict the algorithm in `TSS1.SYMB:2995-3022`, which sets
  `IDXA := J` on the first frame with `W0 = 0`.

- **Mechanism:** `assemble_stmt()` chose the displacement base from the mode
  bits, testing the X bit alone:

  ```c
  else if (oper.mode & 02000)          /* caught modes 4 AND 6 */
      disp = oper.disp - st->xcounter;
  ```

  Only **mode 4** `,X` is X-relative (`(X) + D`). **Mode 6** `,I ,X` is
  `((P) + D) + (X)`, which ND-60.096.01's addressing-mode table names
  *indirect P-relative indexed*: `D` locates the indirect **pointer word**
  relative to `P`, and `X` is added only after that word is fetched. With
  the X location counter at 0 the subtraction was a no-op, leaving the
  symbol's **absolute low byte** in the displacement field.

  Only **backward** references were affected. A forward reference emits a
  bare opcode and is patched by `MAC_FIX_PREL8`, which is P-relative and
  correct. That asymmetry is why the bug survived: it is invisible from a
  forward-reference test case.

  Nine sites were miscompiled, every one of them in TSS1's swapper, because
  its locals block sits between `SW4` (forward references, correct) and
  `SW5` onward (backward references, broken):

  | source (`TSS1.SYMB`) | PC | wrong | correct |
  |---|---|---|---|
  | `LDA I J2,X` (3109)    | 006732 | `047324` (→`006656`) | `047372` (→`006724`) |
  | `LDA I II2,X` (3113)   | 006737 | `047276` (→`006635`) | `047337` (→`006676`) |
  | `LDA I J2,X` (3118)    | 006746 | `047324` (→`006672`) | `047356` (→`006724`) |
  | `STA I IDXX,X` (3149)  | 007003 | `007317`             | `007314` |
  | `LDA I IDXX2,X` (3171) | 007032 | `047320` (→`006752`) | `047266` (→`006720`) |

  At `006732` the instruction addressed `006656` — a word of the `AAA -2`
  instruction — instead of `PDTBL[J]`. Every iteration read the same fixed
  word, so the scan never observed a free frame regardless of `J`. That
  explains all four measured values, `J = 36` included.

- **Why the golden dumps were blind:** encoding-only. Only the displacement
  byte changes; every symbol address and word count is bit-identical, and
  the reconciliation held at 679/693 (A) and 675/689 (B) across the fix.

- **How it was actually found:** not by debugging, but by an invariant sweep
  over the `mac-as -L` listing that assumes nothing about MAC semantics —
  **a P-relative instruction assembled at two different addresses must
  encode two different displacements.** `LDA I J2,X` emitted the identical
  word `047324` at both `006732` and `006746`, which is arithmetically
  impossible; at most one could be right, and in fact neither was. The same
  sweep independently confirmed the `,B` path is sound: `TREG` encodes `-4`
  at all 728 of its sites, which is only possible if that path is
  base-relative as intended.

- **Fix:** require the I bit to be clear before selecting the X location
  counter, so mode 6 falls through to the P-relative branch:

  ```c
  else if ((oper.mode & 02000) && !(oper.mode & 01000))
  ```

- **Pinning test:** `[17]` in `mac-c/tests/test_mac.c` — asserts the
  backward mode-6 encoding at two different addresses (they must differ),
  that the forward case still works, and that modes 4, 1 and 7 are
  unchanged.

- **Verified by running the OS:** `MEMORY 40000 44000` now returns to the
  `@` prompt, with `IDXA = 2` and `J = 2` (the loop exits early via
  `GOTO SW6` the moment a frame is found) instead of `IDXA = IDXB = -1`,
  `J = 36`.

### 6.8 The moral

**Symbol-address oracles validate word COUNTS, not encodings.** Every defect
above except §6.5's dangling names left every symbol address in the golden
dumps bit-identical. No instruction-bit oracle exists in the archive's
symbol dumps — the only archive artifact holding original instruction words
is `reference/LIST1`–`LIST5` (still unused, §8), and the only oracles that
caught these bugs were (a) exhaustive per-instruction encoding tests,
(b) hand-verification on the real MAC, and (c) **running the OS** — the
ultimate oracle, which found §6.1 and §6.6 as a dead console and a hung
login respectively.

§6.7 adds a fourth, and it is the cheapest of the four: **invariant sweeps
over the `-L` listing.** Some properties of correct code generation can be
checked without knowing any MAC semantics at all — a P-relative instruction
assembled at two addresses must encode two displacements; a base-relative
one must encode the same displacement everywhere the symbol means the same
thing. Violations are proofs, not suspicions, and the sweep covers all
16,680 listing rows at once instead of the single point a debugger session
can reach. When a runtime measurement contradicts the source, check that
the emitted code matches the source **before** concluding the measurement
was wrong — in §6.7 the measurement was right and the binary was not.

---

## 7. Validation methodology

1. **Golden symbol-dump reconciliation** — `./scripts/build/build_tss_assysa.sh` runs the
   original `ASSYSA`/`ASSYSB` command streams; `./scripts/verify/compare_asymb.sh` scores
   against `reference/ASYMB.SYMB` / `BSYMB.SYMB`. Current state:
   **679/693 exact (A), 675/689 (B), zero assembly errors**; 13 of the 14
   unmatched entries per dump are macro names, which real MAC lists with
   the macro-body address inside MAC's own memory image — not reproducible
   and not to be faked. Remember §6.7: this oracle proves addresses/counts
   only.
2. **The unit-test suite** — `make test`: **695 passed, 0 failed**
   **[VERIFIED by running it, 2026-07-23]**. Sections [1]–[15]; [1]–[1d]
   are the exhaustive encoding tests, [12] the overlay-snapshot guard,
   [14] undefined-operand + CLD, [15] the addend rule.
   `./scripts/verify/check_coverage.sh` asserts every public function and implemented `)`
   command is referenced by a test.
3. **Live run as the ultimate oracle** — the full bring-up
   (MINIT format → cold-start at address 7 with `--opr=131313` → normal
   boot → login to the `@` prompt) exercises encodings no static oracle
   covers. See [`TSS-BRINGUP.md`](TSS-BRINGUP.md).
4. **Divergence localisation** — `./scripts/verify/first_divergence.sh` (first symbol whose
   value differs, in definition order), `probe_*.sh`, `bisect_hash.sh`,
   `find_hash.sh`; `MACTRACE=1` prints which line ended each source stream
   and the conditional state on exit (the tool for "the build silently
   stopped early"). `./scripts/verify/verify_repo.sh` runs everything at once.

**Build gotcha that cost a debugging cycle:** `make test` rebuilds
`test_mac` but does **NOT relink `mac-as`** — after editing anything in `src/`, run
plain `make` before any `build_tss_*.sh`, or the build silently uses the
stale assembler.

---

## 8. Possible future work

- **Regenerate `mac_permsym.c` from `fmac-1920c.prog`** — still open
  **[VERIFIED 2026-07-23: `mac_permsym.c:23` is `STR=030000`]**, the
  48-bit MAC.BPUN values. Currently harmless for the corpus (`STR` survives
  only in comments after the XTR rename, §5), but the FMAC table is the
  correct provenance for the assembler that actually built TSS, and the CLD
  gap (§6.4) shows the extraction can miss entries — a full re-audit against
  `fmac-1920c.prog` (and `f48mac-1408d.prog` for the 48-bit variant) is the
  right hygiene.
- **Use `reference/LIST1`–`LIST5` as an instruction-bit oracle** — the only
  archive artifact containing the original assembled words; a
  listing-vs-produced word diff would close the oracle gap of §6.7.
- **`fmac-c`** — FMAC is MAC plus floats, `)9MOVE`, `)ULIST`, `)SYSDF`;
  since mac-c already implements the floats and `)9MOVE`, FMAC is a
  *configuration* of mac-c (a `--variant=mac|fmac|f48mac` switch selecting
  permanent table + float width), not a separate program.
- **`macm-c`** — MACM is MAC plus a storage layer (the DGET distinction,
  §4). The seam: replace direct `st->mem[addr]` writes with
  `img_put`/`img_get` accessors (flat array in mac-c, block-buffered core
  image in macm-c), share the front end, then implement the `)9BYTT`
  parameters, coreload selection, the drum-address translation, and MACM's
  command set. With no MACM golden dump, the oracle is differential:
  identical images from mac-c and macm-c at coreload 0, plus a
  `)BPUND` → `)9READ` round-trip.
- **MAC.BPUN open RE items** — §3.10 (parse-loop dispatcher, MON 15/25/42
  semantics, the TSS-symbol-set-in-permanent-table question); a ~12-call
  emulator MON shim would let the real 1978 MAC run as a second oracle.

---

## Cross-references

- [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) — the OS itself: overlay
  disc layout tables, drivers, scheduler, file system.
- [`TSS-BRINGUP.md`](TSS-BRINGUP.md) — the verified bring-up procedure
  (MINIT, cold-start, login).
- [`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md) — the historical
  findings and project overview.
- [`../mac-c/README.md`](../mac-c/README.md) — mac-c usage, build, and
  implementation status (kept authoritative; not duplicated here).
