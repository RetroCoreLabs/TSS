# MAC-C Stub / Unimplemented / Accepted-but-Ignored Inventory

Scope: `E:\Dev\Ronny\TSS\mac-c\` — `mac.c` (~2740 lines), `mac.h`, `main.c`,
`test_mac.c`. `mac_permsym.c` is generated data and is **not** treated as
containing stubs (one data-correctness note is included at the end, clearly
marked as a data issue, because it is an established defect that changes
emitted words).

Every claim below is marked **[VERIFIED]** (with `file:line` and quoted code)
or **[INFERRED]**. "Fully implemented" findings are stated where a suspected
gap turned out not to exist, so the list is honest and not padded.

Line numbers are from the versions read on 2026-07-21.

---

## Summary — category counts

| Category | Count | What it means |
|---|---|---|
| **STUB / NO-OP CODE** | 4 | code present but does nothing useful (flag never read, operands never set, or unreachable) |
| **IGNORED-COMMAND** | 3 (+ generic fallthrough) | a `)`-command the TSS corpus uses that is silently accepted with no image effect |
| **UNIMPLEMENTED** | 5 groups | directives / features / instruction-family variants not handled at all |
| **PARTIAL** | 5 | works for the common case but with a documented limitation or wrong stream/format |
| **Fully-implemented (suspicions cleared)** | 5 | things that looked like gaps but are complete |

Blocking summary: the only items that block a known downstream goal are
`)SOVER`, `)8DUMP` and `)9MOVE` (TSS overlay generation / bootable TSS). All
others are cosmetic, unused-by-corpus, or already confirmed harmless by the
679/693 golden-dump reconciliation.

---

## 1. STUB / NO-OP CODE

| Item | file:line | Should do | Effort / notes | Blocks? |
|---|---|---|---|---|
| Dead second `)9ASSM` handler | `mac.c:2251-2258` | (leftover) log a nested include TODO | **[VERIFIED]** unreachable dead code — the real `)9ASSM` handler at `mac.c:2163` matches `strncmp(s,")9ASSM",6)` first and `return`s, so the identical `strncmp` at 2251 can never run. `[INFERRED]` effort: delete, trivial. | No |
| `)9PARI` — parity toggle never consumed | `mac.c:2136-2140` sets `st->parity_check`; **no reader exists** | Toggle input parity checking (ND-60.096.01 `)9PARI`) | **[VERIFIED]** grep of `parity_check` across `mac.c` shows writes at 2138 + init 2307 only, **no read**. So the command flips a flag that nothing inspects → pure no-op. `[INFERRED]` host input is plain text, so real parity checking is arguably moot; low value. | No |
| `)SETSM` / `)RESSM` — symbolic-output mode never consumed | `mac.c:2146-2155` sets `st->symbolic_out`; **no reader exists** | Select symbolic vs numeric printout mode (ND-60.096.01 `)SETSM`) | **[VERIFIED]** grep of `symbolic_out`: writes at 2148/2153 only, **no read**. No-op toggle. `[INFERRED]` small if a symbolic listing format is ever wanted. | No |
| `)CHANGE` — operands never set, so it is inert | handler `cmd_change` `mac.c:1124-1134`; dispatch `mac.c:2094-2098` | Within the `<` interval, replace masked bits `chg_old→chg_new` under `chg_mask` (ND-60.096.01 §4.2.3.5) | **[VERIFIED]** `cmd_change` **reads** `st->chg_old/chg_new/chg_mask` (1128-1131) but grep shows those three fields are **never assigned anywhere** in `mac.c`. They stay 0 from `mac_init`, so the test is `(mem & 0)==(0 & 0)` → always "matches", and the rewrite is `(mem & ~0)|(0 & 0)` = `mem` → no change. The command has no operand parser, so it can never do anything. `[INFERRED]` needs an operand syntax parser + the three cells populated; small pure logic. Not used by the TSS corpus. | No |

---

## 2. ACCEPTED-BUT-IGNORED COMMANDS (used by the corpus)

All three fall through to the catch-all at `mac.c:2259-2261`:
`/* Unknown / unhandled command: ignore quietly ... */  return;`
i.e. they are **recognised as a `)`-line, then silently discarded with no
error and no effect on the image or symbol table**. Confirmed against the
corpus command set (unique `)`-tokens across `src\*.SYMB`).

| Command | Where it appears | Real behaviour | Effort / notes | Blocks? |
|---|---|---|---|---|
| `)9MOVE` | `src\TSS3.SYMB:83` `)9MOVE ROVER VOR VORS` | **[VERIFIED via docs/ASSEMBLER-FAMILY-AND-PLAN.md §1,§4]** a genuine **FMAC** command (not in MAC/MACM): block-move of assembled words within the image; used by TSS3's `OVERX` to relocate the `ROVER` overlay to `VOR`. | `[INFERRED]` pure logic, small–medium (a `memmove` across `st->mem` for `count` words). Must first disassemble the FMAC handler (octal 170267) to fix argument order and whether the destination is the image or the coreload. | **Yes** — TSS3 overlay relocation is wrong/missing without it. |
| `)SOVER` | `src\TSS3.SYMB:78` (routine `SOVER,` defined at TSS3:37) | **[VERIFIED via plan §1]** NOT a real MAC command — it is the `)SYMBOL` form (ND-60.096.01 §3.2.3.9: "jump to the address given by the value of the symbol"). It invokes TSS's own assembled `SOVER` routine, which returns via `JMP 103,B`. `SOVER` writes the current overlay out to disk. | `[INFERRED]` **needs ND-100 execution** of assembled code (an interpreter, or driving nd100x, or special-casing the routine). Large. | **Yes** — overlay/segment write-out; part of the established blocker for a bootable TSS / the login path. |
| `)8DUMP` | `src\TSS3.SYMB:234,290,296`, `src\TSS5.SYMB:1987,1992`, `src\TDUMP.SYMB:234` | **[VERIFIED via plan §1]** same as `)SOVER`: a `)SYMBOL` invocation of the assembled `8DUMP` routine (defined at TSS3:195, TDUMP:221), which dumps a core block to the disk/tape device via `8FCN/8DKA/8NWD`. | `[INFERRED]` **needs ND-100 execution** (same machinery as `)SOVER`). Large. | **Yes** — disk/overlay image dump; blocks generating a runnable TSS image. |
| *(generic)* any other `)`-command | catch-all `mac.c:2259-2261` | — | Silently ignored. Harmless for the corpus (none besides the above are used), but note there is **no diagnostic** for an unrecognised command. | No |

Note — **false positives** cleared: `)SCRATCH` and `)FRIEND` show up in a raw
`)[A-Z]+` scan of the corpus but are **[VERIFIED]** *inside quoted text
strings*, not commands (`src\TSS4.SYMB:544` `'()SCRATCH\ '`,
`src\TSS4.SYMB:1714` `...)FRIEND NAME: ]`). The `README.md` "Not implemented"
list names `)SCRATCH` and `)FRIEND` as ignored commands — that is inaccurate;
they are not commands at all.

---

## 3. UNIMPLEMENTED (directives / features / variants absent entirely)

| Item | file:line | Should do | Effort / notes | Blocks? |
|---|---|---|---|---|
| `)SYMBOL` generic construct | not present in dispatch (`mac.c:1988-2262`) | "Jump to the address given by the value of the symbol" (ND-60.096.01 §3.2.3.9); runs an assembled routine, returns to the assembler. `)SOVER`/`)8DUMP` are the two instances the corpus uses. | `[INFERRED]` **needs ND-100 execution** of assembled words. Large; this is the root capability behind the two blockers in §2. | Yes (via `)SOVER`/`)8DUMP`) |
| BRF relocatable object output | not present | `)9BEG )9END )9ENT )9EXT )9LIB )9FABS )9EOF )9ASF )9ADS )9LC )9RT` — emit a relocatable binary (BRF) object stream | `[INFERRED]` medium. **Not used by TSS** (absolute assembly); documented as intentionally omitted in `README.md`. | No |
| `)ULIST`, `)SYSDF` | not present | FMAC/MACM commands: punch undefined refs; system-definition mode (ND-60.009.02 §3.8–3.9) | `[INFERRED]` small–medium. Not used by the TSS corpus. | No |
| Interactive debugger commands `.` `!` `\` | not present | Breakpoint / disassemble / examine (interactive MAC debugger, not assembly) | `[INFERRED]` out of scope for a batch assembler; README notes this. | No |
| Nested float literal `([7`-style form | `[` handler `mac.c:1681-1749` handles a bare leading `[` only | A `[` float used *inside* another construct (e.g. as a literal `([`) | **[VERIFIED]** the `[` path only triggers when `stmt[0]=='['` (1681); a `(`-literal whose body is `[...` is not special-cased. `[INFERRED]` corpus uses `[` directly, so unneeded; small if required. | No |

---

## 4. PARTIAL (works for the common case, documented limitation)

| Item | file:line | Limitation | Effort / notes | Blocks? |
|---|---|---|---|---|
| `)PUNCH` | dispatch `mac.c:2109-2113` → `cmd_print` (`mac.c:1211-1217`) | **[VERIFIED]** aliased to `cmd_print`, which writes an **octal text** dump `"%06o/ %06o"` to the **list** stream. Real `)PUNCH` produces a **binary** punch to the **object** stream. Comment at 2111 admits "same dump, object stream in real MAC". | `[INFERRED]` medium if a real binary punch is needed; `mac_write_bpun_range` already exists as a model. Not exercised meaningfully by corpus. | No |
| `)9ASCI` stream | dispatch `mac.c:2114-2118` passes `lst` (list stream) | **[VERIFIED]** header comment (`mac.h:206-208`) says `)9ASCI` belongs to the **object** stream, but dispatch sends it to `lst`. Functional output, possibly wrong stream. | `[INFERRED]` trivial. Not used by corpus for image production. | No |
| Floating-point constants | `mac.c:1681-1749` | Only bare `[NUMBER`; simple round-to-nearest mantissa; 32-bit (default) and 48-bit formats. `float48` is selectable only in code/tests — no command or CLI flag sets it. | **[VERIFIED]** `float48` read at 1702; grep shows no runtime setter outside tests. `[INFERRED]` small to add a CLI/variant switch. | No |
| `)WRUS` / undefined report format | `cmd_wrus` `mac.c:1163-1166` → `mac_report_undefined` (`mac.c:2444-2459`) | **[VERIFIED]** prints `UNDEFINED: NAME`, a debug format, not MAC's `)WRUS` listing layout. Functionally lists the right symbols. | `[INFERRED]` cosmetic, trivial. | No |
| `)9LITR` literal duplication | `literal_intern` `mac.c:447` | **[VERIFIED]** implemented: when `dup_literals` is set the dedup search is skipped so each use gets its own cell. Left here as a *documented* partial only because it is the one place a mode flag actually branches; it is otherwise complete. | none | No |

---

## 5. Fully-implemented (suspicions cleared — NOT stubs)

Stated explicitly so the list is not padded:

- **`)MCDEF` macros** — `macro_begin`/`macro_capture_line`/`macro_expand`
  (`mac.c:645-833`): full definition capture to `]`, `$P` parameter
  substitution with the trailing-space consumption rule, comma-separated
  call arguments, recursion-depth guard. **[VERIFIED]** complete. (The
  file-header "HONESTY NOTE" at `mac.c:22-23` calling macros "stubbed" is
  **stale** — the body is a full implementation.)
- **`)9ASSM` nested source include** — `mac.c:2163-2245`: three stream args,
  ND file naming, save/restore of stream position, depth guard. **[VERIFIED]**
  complete (the stale TODO is the dead block in §1, not this handler).
- **`)FILL` literal pool + fixups** — `cmd_fill` `mac.c:841-901`: places
  pending literals and patches both P-relative operand refs and absolute
  data-word refs. **[VERIFIED]** complete.
- **`)BPUN` / `)9READ`** — `mac_write_bpun_range` `mac.c:2568`,
  `mac_read_bpun` `mac.c:2662`: verbatim MAC.BPUN bootstrap, checksum,
  round-trip verified. **[VERIFIED]** complete.
- **All four instruction classes** (MRI / JUMP8 / ARG8 / PLAIN) with all 8
  addressing modes, `)9SET` B/X counters, literals, strings, `#ab`
  constants, `*N`, `@` shift, library marks — `mac.c` statement layer.
  **[VERIFIED]** complete and exhaustively tested per `README.md`.

---

## 6. Data-correctness note (NOT a code stub — `mac_permsym.c` is data)

Recorded here because it is an established defect that changes emitted words,
even though the task excludes flagging `mac_permsym.c` entries as stubs.

**[VERIFIED via docs/ASSEMBLER-FAMILY-AND-PLAN.md §1]** `mac_permsym.c` was
extracted from `MAC.BPUN`, the **48-bit** MAC build, but TSS 3.0 was
assembled with **FMAC-1920C** (32-bit). Three permanent values differ:

| symbol | mac-c has | correct for TSS (FMAC-1920C) | corpus uses |
|---|---|---|---|
| `STR` | 030000 (`STF`) | 020000 (`STD`) | **7 — 7 wrong words emitted** |
| `LDR` | 034000 | 024000 | 0 (harmless) |
| `2OR3` | 3 | 2 | 0 (harmless) |

The golden-dump score cannot detect this (no symbol *address* moves). Fix:
regenerate `mac_permsym.c` from `fmac-1920c.prog`. `[INFERRED]` small — a
data regeneration, not code. Blocks: byte-correct code output (7 `STR`
instruction words), not symbol reconciliation.

Also documented in `README.md` "Known remaining discrepancies": `QOV1C`
one word short in the TSS3 EDIT overlay, and an extra produced `GRP2=000002`
— corpus reconciliation gaps, not stubs; listed for completeness.

---

## 7. Recommended implementation order

Ordered by blocking impact, then by cost:

1. **Regenerate `mac_permsym.c` from `fmac-1920c.prog`** — correctness fix,
   not a feature; removes 7 wrong `STR` words. Cheapest high-value item.
2. **`)9MOVE`** — pure logic block-move; unblocks TSS3 `OVERX` overlay
   relocation. Disassemble the FMAC handler first to fix arg order.
3. **`)SOVER` + `)8DUMP` via `)SYMBOL` / ND-100 execution** — the real
   blocker for a runnable/bootable TSS. Biggest effort; choose one of: a
   small bounded ND-100 interpreter (preferred per plan §5), drive nd100x, or
   special-case the two short routines.
4. **`)CHANGE` operand parsing** — make the inert command real (small, pure
   logic) *if* any future source needs it; not needed by TSS.
5. **`)PUNCH` real binary output** / `)9ASCI` object-stream fix — cosmetic
   correctness; reuse `mac_write_bpun_range`.
6. **`)9PARI`, `)SETSM`/`)RESSM`** — either wire the flags to real behaviour
   or leave as accepted no-ops; lowest priority (no corpus impact).
7. **Delete the dead `)9ASSM` block (`mac.c:2251-2258`)** and refresh the
   stale "macros stubbed" header note — housekeeping.

BRF output, `)ULIST`/`)SYSDF`, the interactive debugger commands and nested
`([` floats are intentionally out of scope (not used by TSS) and are listed
only for completeness.
