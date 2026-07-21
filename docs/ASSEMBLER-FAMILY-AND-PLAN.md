# The ND assembler family, and a plan for MACM-C and FMAC-C

Everything marked **[VERIFIED]** was read out of a binary, a manual, or the
golden dumps. **[ASSUMPTION]** marks interpretation.

---

## 1. Which assembler built TSS — resolved

**[VERIFIED] TSS 3.0 was assembled with FMAC-1920C**, the 32-bit
floating-point FMAC. Three independent facts converge:

| evidence | detail |
|---|---|
| `ASSYSA` line 1 is literally `FMAC` | the operator started FMAC, not MAC |
| `)9MOVE` is used in TSS3 | present **only** in the FMAC builds; absent from MAC and MACM |
| `)9TSS` closes both build scripts | present in MAC and FMAC, absent from MACM |
| `[` float constants occupy **2 words** | derived from symbol arithmetic on the archived dump *before* the binary was found, then confirmed: `fmac-1920c.prog` carries `2OR3=2`, `LDR=024000`, `STR=020000` — Appendix E's 32-bit markers |

`f48mac-1408d.prog` is the *same* assembler (identical size, identical
handler addresses) built for 48-bit floats (`2OR3=3`, `LDR=034000`). On the
system where both are installed, the plain name **`FMAC` is the 1920C
32-bit build** and the 48-bit one is explicitly `F48MAC` — which is why
`ASSYSA` saying `FMAC` yields 2-word floats.

### Command inventory across the family **[VERIFIED, decoded from each binary]**

| command | MAC | FMAC | MACM | meaning |
|---|:--:|:--:|:--:|---|
| `)9ASSM )FILL )KILL )LINE )WRITE )WRTM )NWRT )WRUS )WLOC )WMNE )BPUN )CLEAR )PRINT )PUNCH )9SET )9LIB )9MSG )9RT )9LC )9ASF )9ADS )9EOF )9ENT )9EXT )9END )9BEG )9PARI )9LITR )9ASCI )9FABS )9EXIT` | ● | ● | ● | common core |
| `)9TSS` | ● | ● | – | alias of `)9EXIT` (same handler) |
| `)9MOVE` | – | ● | – | **block move**, used by TSS3's `OVERX` |
| `)ULIST )SYSDF` | – | ● | ● | undefined-ref punch; system-definition mode |
| `)GJEM )HENT )SAM )CMOVE )BPUND )CLOAD )FIX )XPUN )9READ )9BYTT )9CTOM )9SAVE )9GET )RBP` | – | – | ● | MACM's mass-storage command set |
| `)ZERO )PCL )CORE )LIST )SETSM )RESSM )MCDEF )LSTM )9TABL` | ● | – | ● | option commands |

`)SOVER` and `)8DUMP` are **not commands in any of the three**. They are
MAC's `)SYMBOL` form — *"causes a jump to the address given by the value of
the symbol"* (ND-60.096.01 §3.2.3.9) — invoking routines TSS assembles
itself (`SOVER,` at TSS3:37, `8DUMP,` at TSS3:195 and TDUMP:221), which
return to the assembler via `JMP 103,B`.

### A defect this exposes in `mac-c` **[VERIFIED]**

`mac_permsym.c` was extracted from `MAC.BPUN` — the **48-bit** build. Three
entries differ from FMAC-1920C:

| symbol | mac-c has | FMAC-1920C (correct for TSS) | uses in corpus |
|---|---|---|---|
| `LDR` | 034000 | 024000 | 0 — harmless |
| `2OR3` | 3 | 2 | 0 — harmless |
| `STR` | 030000 (`STF`) | 020000 (`STD`) | **7 — wrong code emitted** |

Seven instruction words are wrong, and no symbol address moves, so the
golden-dump score cannot see it. **Action: regenerate `mac_permsym.c` from
`fmac-1920c.prog`.** (The float default is already correct.)

---

## 2. MACM — what it is, and what the RE established

**MACM assembles into a core image on mass storage, not into core.** From
ND-60.009.02: *"the main difference is its ability to assemble programs out
on a mass storage device (drum) in a core image format"*, and decisively:
*"accessing assembled code must be done using a subroutine (**DGET**) while
in MAC this can be done directly."*

That one sentence is the entire architectural difference and drives the
whole C design.

### Storage model

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

### Ghidra work completed on `MACM-1718L.BPUN`

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

## 3. Plan — `macm-c/`

MACM is *MAC plus a storage layer*. The whole point is that emitted words go
through an accessor instead of straight into an array.

**Step 1 — factor the storage seam into `mac-c` first.** Today `mac.c`
writes `st->mem[addr]` in a dozen places. Replace with `img_put(st, addr, w)`
/ `img_get(st, addr)`. In `mac-c` these stay a flat array; in `macm-c` they
become the block-buffered core image. This is the DGET distinction, made
explicit. Do it in `mac-c` because its 617 tests prove the refactor is safe.

**Step 2 — share the front end.** Lexer, expression evaluator, symbol table,
macros, library marks and every common command are identical. Build them
once as `libmacfront` and link both assemblers against it, rather than
forking 2,400 lines.

**Step 3 — the core-image layer.** `coreimage.c`: the ten `)9BYTT`
parameters, coreload selection, address→block mapping, a one-block
write-back buffer, and the flush that `)LINE` performs. Model the drum
address translation faithfully — it is documented, and getting it wrong
silently misplaces data.

**Step 4 — MACM's commands**, each with a test: `)CLOAD )CMOVE )GJEM )HENT`
`)SAM )FIX )SYSDF )ULIST )BPUND )XPUN )9READ )9BYTT )9CTOM )9SAVE )9GET`.

**Step 5 — oracle.** We have no MACM golden dump, so the oracle is
differential: assemble the same source through `mac-c` and `macm-c` with
coreload 0 and require the images to be identical. Plus round-trip
`)BPUND` → `)9READ`.

## 4. Plan — `fmac-c/`

Much smaller: FMAC is MAC plus floats, `)9MOVE`, `)ULIST`, `)SYSDF`.

Because `mac-c` already implements 32- and 48-bit floats and the whole
common core, **FMAC is a configuration of `mac-c`, not a separate program.**
Recommended: keep one binary with a `--variant=mac|fmac|f48mac` switch that
selects the permanent table and float width, rather than a third folder.

Required work, all small:
1. Regenerate the permanent table from `fmac-1920c.prog` (fixes the `STR`
   defect above) and from `f48mac-1408d.prog` for the 48-bit variant.
2. Implement `)9MOVE src dst count` — disassemble the handler at octal
   170267 to fix the argument order and whether it moves within the image
   or to the coreload.
3. `)ULIST` and `)SYSDF` — both specified in ND-60.009.02 §3.8–3.9.

## 5. The remaining blocker for a bootable TSS

`)SOVER` and `)8DUMP` require **executing assembled ND-100 code**, because
they are TSS's own routines invoked through `)SYMBOL`. Real MAC could: it
ran on the machine. Options, in preference order:

1. **A small ND-100 interpreter inside the assembler.** Bounded: both
   routines are short and straight-line. Their I/O (disk write, tape punch)
   is modelled as writes into the output image.
2. **Drive nd100x** — reuses a proven CPU, at the cost of marshalling state.
3. **Special-case the two routines** from their disassembly — fastest, but
   it is pattern-matching TSS rather than implementing `)SYMBOL`.

## 6. Immediate next actions

1. Regenerate `mac_permsym.c` from `fmac-1920c.prog`; re-score. **Do first —
   it is a correctness fix, not a feature.**
2. Disassemble `)9MOVE` at octal 170267 in the FMAC image; implement.
3. Disassemble `SOVER` and `8DUMP` from the TSS sources themselves — they
   are our own assembled output, so they can be read directly.
4. Verify emitted code against `reference/LIST1`–`LIST5`, which contain the
   original assembled words. This is the only oracle for instruction bits
   and is still unused.
