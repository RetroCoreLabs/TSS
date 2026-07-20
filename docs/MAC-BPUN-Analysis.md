# MAC.BPUN — Reverse Engineering Notes

Binary: `D:\ND\BPUN\MAC.BPUN` (28389 bytes), loaded in Ghidra as ND-100 BPUN.
Analyzed 2026-07-19 with the Ghidra MCP tools. **[VERIFIED]** = read from the
binary; **[ASSUMPTION]** = interpretation.

## Identity

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

## The big surprise: stale listing pages with the TSS symbol set

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

## Layout (as mapped so far)

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
| ed00 | ")9ASSM …Y DIAGNOSTICS" message |
| f84d, fa91 | Debugger strings: breakpoints, disassembler |
| fe9f | "CHECKSUM ERROR" (`)9READ` loader) |
| ff18–ff7f | MAC-monitor "$..." error strings (set 2) |

Functions renamed in Ghidra: `bpun_tape_block_loader` (e8ce),
`ptr_read_word_2frames` (e8e0), `ptr_read_frame` (e8e8). Plate comments set
at e8ce, df18, e030, cde7. Most of the image is still undisassembled —
hand-written MAC assembly with data interleaved; disassemble region-by-region
as needed.

## Consequences for the TSS build plan

1. This is the right MAC for the SINTRAN route (`Build\BUILD-TSS.md` Route 1):
   it is the SINTRAN-subsystem MAC, started from SINTRAN, doing stream I/O
   via SINTRAN logical device numbers.
2. It has BRF + debugger + `)9ASSM` diagnostics — a full-featured 1978 MAC.
3. It may already contain the TSS symbol set (see above) — if so, the ASSYSA
   preamble lines are redundant but harmless.
4. Duplicated string/loader regions suggest the image carries its own
   `)BPUN`/`)9READ` punch-and-load templates — consistent with the manual's
   dump/retrieval chapter.

## Tape structure and native boot **[VERIFIED by parsing the file]**

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
it was punched, so a native boot ends with a jump to location 0.

**This MAC cannot run without SINTRAN.** Verified: every I/O path is
MON 1/MON 2 (+ MON 65 QERMS), `)9EXIT`/`)9TSS` is literally `MON 0` (LEAVE),
and there is no console IOX and no MON trap handler in the image. Options:
1. Run it under SINTRAN (intended usage — matches the build plan).
2. Emulator route without SINTRAN: nd100x/RetroCore can service the small
   MON set it uses (0 LEAVE, 1 INBT, 2 OUTBT, 43 CLOSE, 65 QERMS, + the
   file-open path once identified) with a host-side shim. Feasible — the
   MON surface is tiny.

## The symbol table and command dispatcher **[VERIFIED]**

**Permanent symbol table** at 0xE9B3–0xEBE6 (octal 164663+), pointed to by
the pointer block at 0xE842 (base, twice) / 0xE844 (end):

- Entry = 3 words: `[name_hi, name_lo, value]`.
- Name encoding: up to **5 characters, 6 bits each (ASCII & 077)**,
  right-justified in the 30-bit field spanning the two name words.
  Verified: IOF=[0000,93C6,D101], TRA=[0001,4481,D000], MON=[0000,D3CE,D600],
  EXIT=[0015,8254,CC62] (=146142, matches the manual's EXIT value).
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
  standard MAC has a 9TSS command. Together with the TSS symbol set in the
  stale listing pages, this proves the binary is a custom TSS-restoration MAC.
- The permanent table does **NOT** contain GRP1/GRP2/INTDS/… — so the ASSYSA
  preamble definitions ARE required; they go into the **local symbol table**
  at octal 104000–120402 (0x8800–0xA182, per the pointer block), which lies
  **below** the dumped range — that's why no user symbols are in this file.
- 17 command handlers are registered as `cmd_*` functions in the Ghidra
  project; plate comments on the table (e9b3) and pointer block (e842).

## Paper-tape I/O capabilities **[VERIFIED]**

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
- The BPUN action/start word of this tape is 0 (confirmed against the
  emulator's BPUN loader "Action: 0000000") — MAC was punched without
  autostart; the operator was expected to start it manually.

## Is there a native-capable start address? **[VERIFIED: no]**

Searched for one specifically; the answer is no, for three independent
reasons:
1. The only OS-free I/O in the image is tape-reader INPUT (IOX 400) — the
   block loader and the `)9READ` copy at e1a0. Console I/O is MON 1/2
   exclusively (no console IOX at any alignment), punching is MON 2.
2. MAC's code is **globally B-relative** (`JPL 0x11,B`, `LDA 0xa,B` —
   see cmd_LINE at ECE4): any entry requires the base register B set up by
   the cold-start path first, and that path talks to the terminal via MON
   almost immediately.
3. There is no restart-after-LEAVE entry: only one real `MON 0` call site
   exists (9EXIT at F709); the other two D600 words are the symbol-table
   value for MON (eb3b) and an opcode-constant table at e57f.
The only thing that runs natively is the block loader at 164316 itself.
Candidate cold-start/init code: the F7C0–F810 region (sets up stream
control blocks at CC2x, contains the `COPY SA,DB` base-register loads at
F7D6/F7E8/F808) — to be confirmed dynamically.

Practical no-SINTRAN options, in order of preference:
1. **Emulator MON shim** (no binary modification): trap MON 0/1/2/43/65
   (+ the file-open MON once identified) in nd100x/RetroCore and service
   them host-side. Then any entry works and the true cold-start can be
   found empirically with the DAP debugger.
2. **Binary patch**: all ~30 MON call sites are located; replace them with
   JPLs to IOX console/reader/punch routines placed in free RAM below
   145000 (needs a second BPUN block to load the shim).

## Complete MON call inventory **[VERIFIED — word-aligned scan + code context]**

All real MON call sites (data-word false positives excluded: e57f opcode
table, eb3b symbol-table value, plus odd-alignment noise). Names from
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

## Remaining open questions

- The main parse-loop / dispatcher code (the routine that packs a parsed
  name into 6-bit format and walks the table at [E842]) is not yet pinned —
  trace backward from the `cmd_*` handlers or the E030 line editor.
- How `)9ASSM` opens named files (no MON 50 in the image; check other MON
  numbers on the SINTRAN of that era).
- ~~Exact cold-start entry address~~ **SOLVED, VERIFIED DYNAMICALLY**
  (nd100x --boot=bpun, DAP session 2026-07-19):
  **MAC cold-start entry = octal 173724 (0xF7D4)**. Sequence:
  `LDX (stack=F84E); LDA (E87B); COPY SA,DB; JPL 0x28,B` — i.e. **B is
  loaded with E87B: the global pointer pool IS the base register**, and
  all `nn,B` accesses across MAC index that pool. First action after init:
  prints CR (0215) to terminal 1 via `SAT 1; MON 2` — the characteristic
  startup CRLF from the manual. Ghidra: function `MAC_COLD_START`.
  Native behavior confirmed: with PIE=0, MON falls through as a NOP and
  MAC loops forever retrying its first CR in the output loop ded8–df44 —
  so the MON shim is required exactly as predicted, and 173724 is the
  address to start once MON 0/1/2/43/65 are serviced.
