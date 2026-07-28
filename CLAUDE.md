# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

Recovered 1973 MAC-assembler source for **NORD TSS 3.0** (Norsk Data
NORD-1 / NORD-10 timesharing OS), plus **`mac-c/`** — a C99 reimplementation
of the ND MAC assembler written to rebuild it. Read `README.md` and
`docs/PROJECT-DESCRIPTION.md` first; they hold the historical findings.

The active engineering work is in `mac-c/`. Everything else is data.

Layout: `archive/` originals (never edit) → `src/` clean source →
`reference/` golden oracle → `Build/` output only (disposable).
`derived/` holds project-made inputs. Every folder has a README.

**`archive/` holds the source twice and they are different programs.**
`TSSn.ORG` is Lewendal's 1973 original; `TSSn.SYMB` is CVS's patched copy
with 153 lines of symbol renames (`STR`→`XTR`, `LSS`→`XSS`) made because
those names collide with MAC's permanent table. `src/` carries the patched
version, because that is what the golden dumps were built from. The rename
is incomplete — `STR`, `STR0`–`STR2`, `STR1X`, `STR2X` are referenced but
never defined, which explains six of the twenty undefined symbols.

## Build and test

Requires a POSIX toolchain. On this machine everything runs through WSL:

```bash
wsl -d Ubuntu -- bash -lc "cd mac-c && make test"
```

| command (from `mac-c/`) | purpose |
|---|---|
| `make` | build `build/mac` + `build/test_mac` (must stay warning-free under `-Wall -Wextra`) |
| `make test` | run the whole suite (718 assertions, must be 0 failures) — builds `build/test_mac` but does NOT relink `build/mac`; run plain `make` before any `build_tss_*.sh` |
| `./scripts/verify/check_coverage.sh` | assert every public function and every implemented `)` command is referenced by a test |
| `./scripts/build/run_tss.sh` | assemble the five TSS parts directly |
| `./scripts/build/build_tss_assysa.sh` | **the real end-to-end test** — runs the original `ASSYSA`/`ASSYSB` command streams; writes to `../Build/` |
| `./scripts/verify/compare_asymb.sh <produced> <golden>` | score a symbol dump against the archived one |
| `./scripts/verify/first_divergence.sh <produced> <golden>` | first symbol whose value differs, in definition order |
| `./scripts/verify/verify_repo.sh` | everything at once: layout, no duplicates, build, tests, coverage, both TSS builds, oracle scores, markdown links |

Layout inside `mac-c/`: `src/` the assembler (split by section, see
`src/mac_internal.h`), `tests/` the test source, `build/` **all** generated
output, `scripts/{build,verify,extract,attic}/`. Every script restores
`mac-c/` as its working directory, so they run from anywhere. Note `mac-c/src/`
(the assembler's C source) and `../src/` (the TSS MAC corpus) are different
directories.

There is no single-test runner; `tests/test_mac.c` is one binary with numbered
sections (`[1]`, `[1b]`, … `[15]`). To run one section, comment out the
others in `main()` or add a temporary early `return`.

`MACTRACE=1` makes the assembler print which line ended each source stream
and the conditional state on exit. This is the tool for "the build silently
stopped early" bugs.

## The validation methodology — this is the core workflow

`reference/ASYMB.SYMB` and `reference/BSYMB.SYMB` are the **symbol-table
dumps of the original 1978 builds**. They are the oracle. Any change to the
assembler is judged by re-running the build and re-scoring against them.

Current state: **679/693 exact matches (version A), 675/689 (version B),
zero assembly errors.** 13 of the 14 unmatched entries in each are macro
names, which real MAC lists with the address of the macro body *inside MAC's
own memory image* — not reproducible by a host assembler, and not to be
faked.

**Never guess MAC semantics.** When behaviour is unclear, resolve it from a
primary source and record the evidence in a comment plus a test:

1. `$NDINSIGHT/Reference-Manuals/ND-60.096.01 MAC Interactive Assembly and Debugging System User's Guide.md` — the language spec.
2. `$ND_BPUN_DIR/MAC.BPUN` — the real 1978 MAC binary. Its permanent symbol
   table at `0xE9B3` is where every opcode value in `src/mac_permsym.c` came
   from (3-word entries, names packed 5 chars × 6 bits). Ghidra MCP tools
   work on it; see `docs/MAC-ASSEMBLER.md`.
3. The golden dumps — arithmetic on symbol addresses localises a missing or
   extra word precisely.
4. `archive/*.SYMB` — the original 8-bit bytes, when a parity-stripped file
   looks wrong (this settled that `#SY` really has one `#`).

Beware: matching symbol *addresses* only proves the word **count** is right.
A bug that emitted `000000` for every `COPY SA DT` survived a long time
because the count was correct. That is why `tests/test_mac.c` asserts every
instruction's **encoding**, not just the corpus totals.

## mac-c architecture

**One pass with fixup chains**, deliberately — not two passes. MAC's
`)KILL`/`)PCL` give symbols *time-ordered* scope (TSS reuses names like
`TCL` per routine), which a second pass would destroy. Forward references
are recorded in `g_pending` (per-symbol chains) and patched when the symbol
is defined; literal references go in `g_litrefs` and are patched at `)FILL`.
**Fixups must carry the expression's constant addend** (`mac_fixup.addend`):
`JMP RFN+2` with RFN forward is sym+2, not sym. Dropping it silently
retargeted TSS2 ROBJ's error exits and made LOGON's ()SCRATCH open loop
forever — the login "hang". Pinned by test [15]; the golden dumps cannot
see this class (addresses unchanged, only content). Also: `make test` does
NOT relink `mac-c` — run plain `make` before any `build_tss_*.sh`.

**Statement dispatch order in `assemble_stmt()` is load-bearing.** It is
interval `<` → text string `'…'` → location-set `EXPR/` → label `NAME,` →
assignment `NAME=` → `first_token` → macro call → float `[` → data literal
`(` → instruction/data. Moving the location-set check after the label check
makes `DEVTB+NTTY+1/OEV,…` parse as a label that truncates to `DEVTB` and
silently redefines it.

**Every scanner must skip `#ab` character constants** — the comment strip,
the `;` splitter, and the `/`, `,` and `<` scans. `#` plus *two* characters
is one word `(a<<8)|b`, and the second character is frequently a blank,
`;`, `'` or `<`. Use `skip_char_const()`; do not hand-roll the check.

**Streams** follow MAC: source / list / object. `)LIST` writes to the
**object** stream (that is how `)9ASSM TSS5,LIST5,ASYMB:SYMB` + `)LIST`
produces the archived dump); reports go to the list stream; `0` selects the
dummy device (NULL). `main()` must call `mac_close_streams()` or buffered
object output is lost.

**`src/mac_permsym.c` is generated** from the MAC binary. Do not hand-edit it;
regenerate if the extraction changes.

## MAC semantics that are counter-intuitive

Each was established from a primary source and is pinned by a test. Changing
any of them will break the corpus reconciliation.

- **P-relative displacement is `target - here`**, where `here` is the
  instruction's own address (not `here+1`).
- **Word = opcode + mode + (disp & 0377).** Mode bits and displacement must
  be kept separate; masking after adding lets a negative displacement eat
  the `,B` bit.
- **Only mode 4 `,X` is X-relative.** The displacement's base is decided by
  what the displacement *addresses*, not by which mode bits are set:
  `,B` forms (1,3,5,7) are B-relative, mode 4 `(X)+D` is X-relative, and
  everything else — including **mode 6 `,I ,X` = `((P)+D)+(X)`** — is
  P-relative, because `D` locates the indirect pointer word relative to `P`
  and `X` is added after the fetch. Testing the X bit alone caught mode 6
  too and silently miscompiled nine backward references in TSS1's swapper,
  hanging `MEMORY`. Forward references hide this class (they go through
  `MAC_FIX_PREL8`, which is correct), so **test backward references**.
  See `docs/MAC-ASSEMBLER.md` §6.7; pinned by test [17].
- **Symbol significance is the LAST 5 characters** (MAC shifts into a 30-bit
  field). `RBLOAD` → `BLOAD`, `TIMOUT` → `IMOUT`.
- **A label redefinition keeps the FIRST value** ("ALREADY DEFINED");
  `)KILL` is required before an intentional redefinition.
- **`BSS` is not a builtin** — it is absent from MAC's table; TSS defines it
  as a macro that only advances the location counter.
- **`[` floats are 2 words here** (the optional 32-bit format). `MAC.BPUN`
  is the 48-bit variant (`2OR3=3`, `LDR=034000`), but the golden dumps were
  produced by a 32-bit-float MAC. Default is 32-bit; `mac_state.float48`
  selects the other. These are different MAC builds.
- **`*N` means `*+N`** (`JMP *3`).
- **Blanks separate expression terms** — MAC statements are sums.
- **`-` negates a mark term even unspaced**: `"CDC-DRUM` is `CDC AND NOT DRUM`.
- **A library mark is true when the symbol is referenced but undefined.**
  A bare symbol line both makes the mark and emits a word.
- **`)LINE` is an ordinary command** and must be skipped inside a false
  conditional region, like any other. TSS5 contains a `"TSBIN`-guarded
  `)LINE`; honouring it unconditionally ends the file early *and* leaves the
  conditional stuck off, which silently swallows the final `)LIST`.
- **An unrecognised expression line still emits a word** (`&L`), and a macro
  name with trailing non-symbol characters (`PROGM&M`) is an expression,
  not a macro call.

## Corpus notes

- 20 symbols remain undefined after a full build and that is correct: the
  library marks themselves, `"N10`-only definitions (`REA`, `RKE`, `EXRGP`)
  referenced from unconditional code, the six casualties of CVS's
  incomplete rename (`STR`, `STR0`–`STR2`, `STR1X`, `STR2X`), and
  `&L` / `PROGM&M`, which are expressions by design.
- The drum driver (`XDRUM`, NORD-10 only, device `IOX 540`) is compiled
  **out** of both archived builds. `derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB`
  enables it via the `DRUM`+`N10` marks.
- `Build/` is committed so the output is browsable, but everything in it
  except `README.md` is regenerated; the build scripts delete and rewrite
  it, preserving only that README.
- `mac-c/scripts/attic/` holds the divergence-localising probes
  (`probe_*.sh`, `bisect_hash.sh`, `find_hash.sh`); `diff_cvs_vs_original.sh`
  and `check_rename_gaps.sh` there reproduce the `.ORG`-vs-`.SYMB` analysis. Prefer
  writing a script file over long inline shell, since PowerShell→WSL quoting
  mangles complex one-liners.
