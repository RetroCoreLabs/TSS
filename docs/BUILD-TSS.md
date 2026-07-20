# Building NORD TSS with MAC/FMAC

How to rebuild TSS from the recovered sources in `E:\Dev\Ronny\TSS\Sources\`
using the extracted build inputs in `E:\Dev\Ronny\TSS\Build\`.
Facts marked **[VERIFIED]** come from the archive files or the MAC manual
(ND-60.096.01); **[ASSUMPTION]** items are interpretation.

## Files

| File | Purpose |
|---|---|
| `Build\ASSYSA-MAC-INPUT.SYMB` | Version A (SYSA) build — pure MAC command stream |
| `Build\ASSYSB-MAC-INPUT.SYMB` | Version B (SYSB, DEBUG) build — pure MAC command stream |
| `Build\ASSYS-DRUM-N10-MAC-INPUT.SYMB` | **Experimental** NORD-10 + swapping-drum build (enables `XDRUM`) |
| `Sources\TSS1.SYMB` … `TSS5.SYMB` | The five TSS source parts (copy to target as `TSS1:SYMB` …) |
| `Sources\ASSYSA.SYMB` / `ASSYSB.SYMB` | The original scripts, untouched (for reference) |
| `Sources\DRUM-DRIVER.SYMB` | Standalone extraction of XDRUM/TRSFR (study/porting) |
| `Sources\MINIT.SYMB` / `TDUMP.SYMB` | Standalone utilities: mass-storage init, TSS dumper |

## What was changed vs. the original ASSYSA/ASSYSB **[VERIFIED diffs]**

Removed from the front (not MAC input — operating-system-level startup):
```
FMAC        <- [ASSUMPTION] command that starts the FMAC assembler
100         <- [ASSUMPTION] reply to an FMAC startup prompt (table size?)
SYSA / SYSB <- [ASSUMPTION] reply to a name prompt
```
Removed from the end: `*:`, `?`, `)9TSS` (A) / `)9TSS`, `@cc` (B) — `)9TSS` is not
in ND-60.096.01; **[ASSUMPTION]** a site-specific command that started TSS.
Added at the end: `)9EXIT` (documented MAC exit command, for unattended runs).
Added in A: double quotes around output file names — per the manual, `"FILE"`
makes MAC create the file; the unquoted originals required pre-existing files.
Everything between is verbatim, including the `% Added by CVS` symbol patches.

## Route 1 — MAC as SINTRAN III subsystem (recommended first attempt)

1. Transfer to the SINTRAN user area (7-bit ASCII, keep TABs):
   - `TSS1:SYMB` … `TSS5:SYMB` (from `Sources\`)
   - the chosen `*-MAC-INPUT.SYMB` content
2. Run (interactively or as a mode job):
   ```
   @MAC
   <contents of ASSYSA-MAC-INPUT.SYMB>
   ```
   As a mode file: first line `@MAC`, then the MAC input verbatim, ending with
   `)9EXIT`; execute with `@MODE <file>,<output-file>`.
3. Each `)9ASSM` prints a diagnostics count per part (`**000000 DIAGNOSTICS`
   = clean **[VERIFIED manual]**). Listings go to `LIST1:SYMB`…`LIST5:SYMB`,
   the final `)LIST` dumps the symbol table to `ASYMB:SYMB` (A) / `BSYMB:SYMB`
   (B) / `DSYMB:SYMB` (drum variant).

### Verification oracle
Compare the produced symbol dump against the archived golden dumps
`E:\Dev\Ronny\TSS\NoParity\ASYMB.SYMB.TXT` (version A) and `BSYMB.SYMB.TXT`
(version B). Matching values = byte-identical build. **[VERIFIED]** these are
`)LIST` outputs of the original successful builds.

## Route 2 — stand-alone MAC from BPUN paper tape

**[VERIFIED manual §1.x/App C]** MAC also exists stand-alone ("the stand-alone
NORD-10 with a console typewriter"), distributed as a bootable absolute binary
(BPUN) paper tape; MACM is the mass-storage variant used to generate SINTRAN.

Workflow **[ASSUMPTION — device details depend on the MAC tape you find]**:
1. Boot the MAC BPUN tape (1560& style bootstrap from the tape reader).
2. Mount each source as a paper tape image and switch streams with the `$`
   command (device-number form) instead of `)9ASSM` file names — `)9ASSM` with
   *names* needs a file system. Feed the symbol-definition preamble from the
   console first, then `$` per source tape.
3. This matches how the sources are structured: each part ends in `)LINE`,
   which returns control to the console between tapes **[VERIFIED]**.

Caveat: the TSS3–TSS5 overlay machinery executes `)9MOVE ROVER VOR VORS` at
assembly time and `SOVER` "WRITE OVERLAY ONTO DISK ... RETURNS TO MAC VIA
JMP 103,B" **[VERIFIED source]** — i.e. the original build ran on a machine
where MAC was resident with mass storage attached. A pure tape-only stand-alone
MAC may not support the overlay writes; the SINTRAN or MACM (mass-storage)
variants are the more promising hosts.

## Drum build notes (ASSYS-DRUM-N10-MAC-INPUT.SYMB)

- Marks `DRUM N10` added: compiles `TRSFR`+`XDRUM` (TSS1 lines 3711–3874),
  swapping to a drum at device `IOX 540`, spilling to CDC disk above
  4×DRMSZ pages. `DRMSZ` defaults to 2000 octal 256-word pages.
- `INTDS`/`INTEN` pre-definitions removed — under `N10` the source defines
  them itself as `IOF`/`ION` (TSS1 lines 103–106) **[VERIFIED]**.
- Known wart **[VERIFIED]**: TSS1 line 2658 (`LEV6` scanner Diablo probe)
  assembles NORD-1 `IOT` words unguarded, even in the N10 build. Runtime
  impact on an ND-100/emulator not analysed — review before booting.
- The driver's own header calls it a "1. APPROXIMATION" (NJL, 17/4/73) —
  expect to debug it.

## Open items

- No MAC/FMAC binary is present in this archive — you must supply one
  (SINTRAN `@MAC`, or a BPUN paper-tape image of stand-alone MAC/FMAC/MACM).
- Meaning of `FMAC`/`100`/`SYSA` startup lines and `)9TSS`/`*:`/`?` trailer
  is unconfirmed — if your MAC behaves differently at startup, adjust.
- Memory image: TSS assembles into memory at fixed addresses (page zero
  vectors at 0–7, `GOOM=*`, overlays via `ROVER`) and saves itself with
  `SYSSV`/`CORLD` (location 7: `LDT *+3; JMP I *+1; SYSSV; CORLD`)
  **[VERIFIED]** — the "binary" is the running memory image saved to disk,
  not a linker output file.
