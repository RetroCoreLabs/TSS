# MAC-C — a host-side MAC assembler for the NORD TSS sources

C99 reimplementation of the ND MAC assembler, written to rebuild the
**NORD TSS 3.0** sources in `E:\Dev\Ronny\TSS\Sources\`.

## Build and run

```
cd /mnt/e/Dev/Ronny/TSS/mac-c      # WSL / Linux
make            # builds build/mac and build/test_mac (-Wall -Wextra, zero warnings)
make test       # runs the unit-test suite
./scripts/build/run_tss.sh         # assembles all five TSS parts as ASSYSA does
./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
```

Everything generated lands in `build/`; `make clean` removes that directory.
The scripts may be run from anywhere — each restores `mac-c/` as its working
directory before doing anything.

## Layout

| path | what it holds |
|---|---|
| `src/` | the assembler, split by section — see `src/mac_internal.h` |
| `tests/` | `test_mac.c`, the unit-test binary's source |
| `build/` | **all** generated output: objects, `mac-c`, `test_mac`. Disposable. |
| `scripts/build/` | the artifact builds — `build_tss_assysa.sh`, `build_tss_drum.sh`, `build_minit.sh`, `run_tss.sh` |
| `scripts/verify/` | oracle scoring and repo checks — `verify_repo.sh`, `check_coverage.sh`, `compare_asymb.sh`, `first_divergence.sh`, `cmp_syms.sh` |
| `scripts/extract/` | regenerate `src/mac_permsym.c` from a real MAC/MACM binary |
| `scripts/attic/` | the probes that localised each historical divergence, kept as the reproduction path for claims the docs state as fact. Not part of any build. |

Note the two `src/` directories are different things: `mac-c/src/` is the
assembler's own C source, `../src/` is the TSS MAC corpus it assembles.

## Current state — measured, not claimed

**Unit tests: 695 passed, 0 failed.** `./scripts/verify/check_coverage.sh` reports
*no uncovered public functions or implemented commands*.

Instruction coverage is exhaustive rather than sampled:

| test | what it asserts |
|---|---|
| `[1]` | all **155** permanent symbols assemble to the value carried by the real `MAC.BPUN` table |
| `[1b]` | **every** memory-reference opcode against **all 8** addressing modes |
| `[1c]` | **every** conditional jump (forward and backward) and **every** argument instruction (positive and negative) |
| `[1d]` | the register/IO forms built by summing sub-fields (COPY, RADD, SKP, SHA, BSET, BSKP, IOX, MON, TRA/TRR/MST/MCL, IRW/IRR, RMPY/RDIV, EXR, …) |

**The original build script now runs end to end.** `./scripts/build/build_tss_assysa.sh`
feeds the real `ASSYSA` command stream to `mac-c` — `)9ASSM TSS1,LIST1,0`
… `)9ASSM TSS5,LIST5,ASYMB:SYMB` followed by `)LIST` — so the assembler
drives its own streams, nested source includes and ND file naming, and
writes `ASYMB:SYMB` itself. Both variants build with **zero errors**:

| build | golden symbols | exact matches |
|---|---|---|
| A (`ASSYSA`) | 693 | **679** |
| B (`ASSYSB`, DEBUG) | 689 | **675** |

The A/B difference is real and correct: `CORLD=000060` vs `000260`, exactly
as the two archived dumps differ.

**Golden-oracle reconciliation** against `ASYMB.SYMB.TXT`, the symbol dump
produced by the original 1978 MAC build of ASSYSA:

| metric | value |
|---|---|
| symbols in the golden dump | 693 |
| symbols produced | 681 |
| **exact name+value matches** | **679** |
| missing from produced | 14 |
| extra | 2 |
| **hard errors during assembly** | **0** |
| symbols left undefined | 20 (all accounted for, see below) |

The 20 undefined symbols are all expected: the library marks themselves
(`CDC MACF DIAB K14 TEL4 NN10 NK4 CLD ?`), symbols defined only inside
`"N10` blocks but referenced from unconditional code (`REA RKE EXRGP`,
`STR0/STR1/STR2/STRX/STR1X/STR2X`) — undefined in a NORD-1 build in the
original too — and the two constructs that are deliberately expressions
rather than symbols (`&L`, `PROGM&M`).

Of the 14 missing, **13 are macro names** (`BSS, DATA, DECL, ENTER, GLOBL,
GOVER, OVERL, OVERX, PATCH, PROGM, RET, SRET, STSET`). Real MAC lists a
macro name with the address of its body **inside MAC's own macro buffer**
(values 146152…146627, i.e. inside MAC's 145000+ image). Those addresses are
artifacts of MAC's internal memory layout and cannot be reproduced by a host
assembler; inventing them would be dishonest, so they are simply absent.

Excluding those, **679 of 680 reproducible symbols match exactly (99.85%)**.

### Known remaining discrepancies

1. `QOV1C` — golden `000642`, produced `000641`: one word short somewhere in
   the TSS3 EDIT overlay. Not yet localised.
2. `GRP2=000002` is produced but absent from the golden dump (the original
   build expunged it; the mechanism has not been identified).
3. 2 hard errors remain in the log (`forward reference in '='`), and 314
   symbols are referenced but never defined. Many of the latter are genuine
   (SINTRAN monitor entry names listed in `MCTBL`), but the count has not
   been audited symbol by symbol.

## Bugs the exhaustive instruction tests exposed

Address-only comparison against the golden dump had been hiding wrong
*code generation*, because the word **count** stayed correct:

1. **`eval_expr` skipped blanks instead of treating them as term
   separators**, so `COPY SA DT` fused into one bogus symbol `COPYSADT`
   and emitted `000000`. Every multi-term register instruction in the
   corpus was wrong. (ND-60.096.01 §2.5: elements are "separated by a
   single space, a plus sign or a minus sign".) Fixing it dropped the
   undefined-symbol count from **315 to 30**.
2. Once blanks separated terms, whitespace fell into the `@`
   shift-operator branch and swallowed the following term — guarded.
3. `#ab` is a character **pair**, not `##`+char. `SINQ, #SY; #ST; #EM`
   (TSS2:843, confirmed against the original 8-bit bytes) needs the
   general rule; `##c` is the same rule with `a='#'`.
4. A `#ab` whose second character is a blank, `;` or `<` broke the
   comment scanner, the statement splitter and the interval command
   (`SAA ##<` in TSS3:1247 was being read as `SAA ## < ...`).
5. Trailing-whitespace trimming destroyed the blank in `SAT ## `
   (TSS2:2507), where the blank *is* the constant.
6. `*N` (e.g. `JMP *3`) was unimplemented — it is shorthand for `*+N`.
7. **`)LINE` was being honoured inside a false conditional region.**
   TSS5:1980 opens `"TSBIN` and TSS5:1981 is a `)LINE` belonging to the
   binary-tape build only. Executing it unconditionally ended TSS5 early
   *and* left the mark conditional switched off, which then silently
   swallowed the `)LIST` at the end of ASSYSA — the object stream came out
   empty with no error reported. `)LINE` is an ordinary command and must be
   skipped like any other when the region is false. Found only by running
   the real build script; feeding files in order had hidden it.

`MACTRACE=1` in the environment prints which line ended each stream and the
conditional state on exit — that is what localised bug 7.

## What is implemented

Statements: labels `NAME,`; assignment `NAME=EXPR`; location set `EXPR/`
**including the "set then continue on the same line" form**; multi-statement
lines (`;`); `%` comments; text strings `'…'`; floating constants `[…`;
literals `(…` both as instruction operands and as data words; `##c`
character constants; digit-leading symbols (`8LP`, `9TTI`, `9ASSM`).

Instruction classes with correct encoding: memory-reference (all 8 addressing
modes), conditional jumps, argument instructions, and plain register/IO
statements.

**Commands — all 28 implemented and individually tested:**

`)FILL` `)KILL` `)PCL` `)LIST` `)LINE` `)MCDEF` (full macro definition, `$`
parameters, `]` terminator, call-site substitution) `)WRTM` `)NWRT`
`)WRITE` `)WRUS` `)WLOC` `)WMNE` `)9SET` (B/X location counters) `)ZERO`
`)CHANGE` `)CORE` `)PRINT` `)PUNCH` `)9ASCI` `)9MSG` `)CLEAR` `)9PARI`
`)9LITR` `)SETSM` `)RESSM` `)9EXIT` `)9TSS` `)9ASSM` (three streams +
nested source include) `)BPUN` `)9READ`, plus the `<` interval command.

Library marks: `"EXPR` regions, bare `"` reset, `-` negation both spaced and
unspaced.

Streams: source / list / object, as MAC has them. `)LIST` goes to the
**object** stream (which is why `)9ASSM TSS5,LIST5,ASYMB:SYMB` + `)LIST`
produces the archived symbol dump), the reports go to the list stream, and
`0` selects the dummy device.

I/O: symbol listing in the archived `ASYMB:SYMB` format; image save/load
(`MACIMG`, round-trip verified); BPUN paper-tape emission reusing the
**verbatim octal bootstrap recovered from the real `MAC.BPUN`**, and
`)9READ`/`mac_read_bpun` reloading it with checksum verification.

## The overlay-to-disc pipeline (implemented)

TSS ships its 31 code overlays on the CDC system disc and pages them in at
run time. mac-c now reproduces that pipeline (see `docs/TSS-ARCHITECTURE.md (overlay chapter)`):

- **`)9MOVE src dst count`** (ND-60.096.01 §C.1.1.1) — a verbatim block copy
  of `count` assembled words within the image, **no relocation**. The `"MACF`
  `OVERX` macro uses it (`)9MOVE ROVER VOR VORS`, TSS3:83) to stage each
  overlay from its assembly window at `ROVER` into a distinct `VOR` slot
  (040000, 041000, … 076000) so all 31 survive in memory. It copies memory
  only, so the golden symbol dump is unchanged.
- **CDC-disc image (`mac -c FILE`)** — after assembly, writes a raw
  big-endian disc image placing overlay *n* on sectors `OVDK+2n` / `OVDK+2n+1`
  (two 256-word sectors, byte offset `sector*512`), exactly where the run-time
  overlay reader (routine `S5`) looks. Constants (`OVDK`, `VORS`, `RQR`, `VOR`)
  are read from the build's own symbol table, not hard-coded; `VOR_base =
  VOR − RQR*VORS`. Wired into `build_tss_drum.sh` → `Build/drum/tss-cdc.img`
  (174 sectors / 89 088 bytes for the DRUM build). The writer prints an
  overlay→sector table to stderr so the boot test can verify the mapping.
  The window-index → overlay-number mapping is the straightforward parallel
  choice (window *n* → overlay *n*); it is not fully pinned in the TSS source
  (`TSS-ARCHITECTURE.md (overlay chapter)` §8) which is why the diagnostic table is emitted.

## Not implemented (and why)

- `)SOVER`, `)8DUMP` — `)SYMBOL`-style invocations of assembled ND-100
  routines that would need **ND-100 execution** to run. They live only in the
  `"NMACF` / `"TSBIN` paths, which the MACF/DRUM builds never assemble
  (`TSS-ARCHITECTURE.md (overlay chapter)` §1.3, §6), so on the builds mac-c targets they are a
  **documented intentional no-op** — and unnecessary, because the run-time
  disc contract they implement is reproduced directly by `)9MOVE` + the CDC
  image writer above. mac-c does **not** fake ND-100 execution.
  (Note: `)SCRATCH` / `)FRIEND` are **not** MAC commands at all — they appear
  only inside quoted strings in the corpus; an earlier version of this list
  named them wrongly.)
- BRF relocatable object output (`)9BEG`, `)9END`, `)9ENT`, `)9EXT`,
  `)9LIB`, `)9FABS`, `)9EOF`, `)9ASF`, `)9ADS`, `)9LC`, `)9RT`). TSS is an
  absolute assembly and never uses them.
- The breakpoint/disassembler debugging commands (`.`, `!`, `\`) — these
  are interactive debugger features, not assembly.
- Nested float literals of the `([7` form (the corpus uses `[` directly).

## Facts established from primary sources (not assumed)

Each of these was verified and then locked into a unit test:

| rule | evidence |
|---|---|
| word = opcode + mode + (disp & 0377); mode bits kept separate from a masked displacement | ND-60.096.01 §2.6 worked example (043776 + 400 = 044376) |
| P-relative: EA = (P) + disp, P = the instruction's **own** address | ND-60.096.01 §2.3.1, and `JMP *0xdef6` at `0xdf11` in MAC.BPUN encoding disp 0345 = −27 |
| mode bits `,X`=02000, `I`=01000, `,B`=0400 | MAC.BPUN permanent table + `nd100-markdown/docs/addressing_modes.md` |
| all 154 permanent symbol values | decoded from MAC.BPUN's table at 0xE9B3 (5 chars × 6 bits per name) |
| strings: chars **plus the terminating quote**, 2/word, high byte first | ND-60.096.01 §3.2.3.7 (`'ABCDEF'` at 400 occupies 400–403, counter → 404) |
| macros: `)MCDEF NAME $P,…` … `]`, parameter followed by a space that is consumed | ND-60.096.01 §4.2.9.2 |
| symbol significance = **last** 5 characters | golden dump lists `RBLOAD` as `BLOAD` and `TIMOUT` as `IMOUT` |
| a label redefinition keeps the FIRST value ("ALREADY DEFINED") | golden `LBYTE=010375`; the string exists in MAC.BPUN |
| `BSS` is **not** a MAC builtin — TSS defines it as a macro that only advances the counter | absent from MAC.BPUN's table; `)MCDEF BSS $A1` / `*+$A1 /` in TSS1:153 |
| `[` floats are 2 words here (optional 32-bit format, Appendix E) | the TBANG…RDATE span is 55 words in the golden dump, which only fits 4 × 2-word constants |
| `##c` = ASCII of `c`, and must not disturb `/`, `,`, `'` or `;` scanning | TSS1 `SAT ##/`, TSS2 `AAA -##'`; manual shows `SAA ##?` |
| `-` negates a mark term even unspaced (`"CDC-DRUM`) | golden has `TRSFR=007672`, only reachable when `CDC AND NOT DRUM` is true |
| an unrecognised expression line (e.g. `&L`) still emits one word | golden `LEV7=004176` vs `ECH02=004165` with an 8-word line between |
| a macro name with trailing non-symbol characters (`PROGM&M`) is an expression, not a call | golden `BCOPY=027051` |

### A deliberate divergence worth knowing

`D:\ND\BPUN\MAC.BPUN` is the **48-bit** floating-point MAC (its table has
`2OR3=3`, `LDR=034000`, `STR=030000` — the 48-bit values per Appendix E),
but the archived `ASYMB:SYMB` was produced by a **32-bit** float MAC. They
are different builds. MAC-C defaults to the 32-bit format so it reproduces
the corpus; set `mac_state.float48 = true` for the other variant. Both are
covered by tests.

## Files

| file | purpose |
|---|---|
| `src/mac.h` | the public interface |
| `src/mac_internal.h` | shared across the split translation units; not public |
| `src/mac_symtab.c` | symbol table, fixup chains, diagnostics, memory emission |
| `src/mac_expr.c` | expression evaluation, MRI operand evaluator |
| `src/mac_macro.c` | `)MCDEF` capture and expansion |
| `src/mac_cmd.c` | the `)` command implementations and stream plumbing |
| `src/mac_stmt.c` | statement dispatch, library marks, line processing |
| `src/mac_api.c` | the public `mac_*` entry points |
| `src/mac_bpun.c` | image / CDC disc / BPUN tape readers and writers |
| `src/mac_permsym.c` | generated permanent symbol table (do not hand-edit) |
| `src/main.c` | `mac-c` driver |
| `tests/test_mac.c` | unit tests |
| `scripts/build/run_tss.sh` | assemble the TSS corpus as ASSYSA does |
| `scripts/verify/check_coverage.sh` | audits that every public function and command is tested |
| `scripts/verify/compare_asymb.sh`, `first_divergence.sh`, `cmp_syms.sh` | golden-dump reconciliation |
| `scripts/attic/probe_*.sh`, `check_symlen.sh`, `find_hash.sh`, `bisect_hash.sh` | diagnostics used to localise divergences |

Additional rules established while completing the command set:

| rule | evidence |
|---|---|
| `#ab` is a character PAIR `(a<<8)\|b`; `##c` is that rule with `a='#'` | `SINQ, #SY; #ST; #EM` (TSS2:843), verified against the original 8-bit bytes |
| `*N` is shorthand for `*+N` | the disassembler prints `LDA *003` (§4.2.7); TSS writes `JMP *3` |
| blanks separate terms in an expression | ND-60.096.01 §2.5 |
| `)LIST` writes to the OBJECT stream | §4.2.3.3, and it is how ASSYSA produces ASYMB:SYMB |
| `)9SET ,B SYM` makes an address `,B` operand relative to the B counter | §3.2.4 worked example (`LDA A1,B` after `)9SET ,B G`) |
| `)ZERO` fills the `<` interval, optionally with a symbol's value | §4.2.3.1 |
| `)CHANGE` rewrites masked bits across the `<` interval | §4.2.3.5 |
| MAC starts in non-write mode; parity checking starts on; literals are shared until `)9LITR` | §)WRTM, §)9PARI, §)9LITR |
