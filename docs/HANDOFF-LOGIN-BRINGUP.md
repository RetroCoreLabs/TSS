# Handoff — TSS login bring-up (mac-c `9377` fix + disk-init understanding)

**Date:** 2026-07-22. **Author of this pass:** Claude Code session.
**One-line status:** the assembler bug that made login impossible is **fixed and
validated**; the create/login mechanism is **fully understood and documented**;
the only thing left for a live login is the **MINIT disc-format run procedure**.

Read alongside: [`DISK-INIT-USERS-AND-BOOT.md`](DISK-INIT-USERS-AND-BOOT.md)
(the how-to) and the memory notes `tss-login-char-path`, `tss-login-disc-init`.

---

## 1. What was fixed (done, validated)

**mac-c tokenizer bug — `mac.c:289-306`.** All-digit tokens were classified as
numbers with `isdigit()` (0-9) but parsed as **octal**. The symbol `9377` (the
`377` byte-mask literal used by the teletype ring routines `WBUF`/`RBUF`) hit
`strtol("9377", …, 8)`, which stops at the non-octal `9` and returns **0**. So
`AND 9377` assembled as `AND 0` (operand = the instruction word itself, `070000`)
→ `char AND 070000 = 0`. **Every console character was zeroed** on the way into
the type-ahead buffer; `RBUF` returned 0, `TCI` spun forever, LOGON never saw
input. Golden dumps missed it because `9377` is *defined* fine — only *references*
mis-encoded (addresses match, encoding wrong).

**The fix:** in the octal branch require octal digits `0-7`:
```c
bool valid = decimal ? isdigit(c) : (c >= '0' && c <= '7');
```

**Validation performed:**
- `make test` → **677 passed, 0 failed** (added a regression test in
  `test_mac.c` §[7]; proven to fail `got 070000` when the bug is reintroduced).
- Golden dumps unchanged: **ASYMB 679, BSYMB 675, 0 assembly errors** (only the
  pre-existing `QOV1C` off-by-1 remains; word counts don't change).
- Rebuilt drum image: all four `AND 9377` sites now carry correct displacements
  resolving to `9377` (=`377`).
- **Live DAP proof:** on the fixed image, typing `SYSTEM` now **echoes**
  `@ENTER SYSTEM` (was total silence) and LOGON reads the whole name and leaves
  its input loop. The char-loss bug is gone.

**Files changed (TSS tree is NOT a git repo — nothing committed):**
- `mac-c/mac.c` — the fix.
- `mac-c/test_mac.c` — regression test in section `[7]`.
- Docs: this file, `docs/DISK-INIT-USERS-AND-BOOT.md`, `README.md`.
- `nd100x` (separate git repo, branch `main`): terminal diagnostics were added
  during debugging and then **reverted** (`git checkout` of
  `src/devices/terminal/deviceTerminal.c`); the binary was rebuilt clean. No
  intended nd100x change from this pass. (Pre-existing uncommitted nd100x changes
  from earlier sessions — `cpu_instr.c` IOT fix etc. — were left untouched.)

---

## 2. What we now understand about login (verified)

Two on-disc user tables, same user-number index:

| table | disc addr | holds | written by |
|---|---|---|---|
| `USTBL` | `DKBIT+idx` (`DKBIT=50₈` → ≈ disc 51₈) | **names** + track ownership | `CRUSE`/`FCRUS` — so `SINIT` and `CRUSR` |
| `USRDK` | `1460₈` | **password** word (entry offset 7) | `CRUSR`, `PASWD`, `CPASW` |

- `LOGON` matches the typed name via `ABLKP`, whose accessor `USARR` reads names
  from **`USTBL`** (`TSS3.SYMB:660`), **not** `USRDK`.
- The password comes from `USRDK[user][7]`; **0 = passwordless**.
- So **`SINIT` alone makes SYSTEM loginnable** (name → `USTBL`, empty `USRDK` →
  passwordless). There is **no "seed USRDK" step** — an earlier theory that was
  disproven.
- **But** `SINIT`'s `CRUSE→GTRK` needs free tracks, found by scanning the MIB
  bitmap — which only exists after **MINIT** formats the disc. On the raw
  overlay-only build image the bitmap is zero, so creation silently fails. That
  is why cold-booting `--opr=131313` on the raw image does not log in.

**Cold-start operator switch** (`TSS1.SYMB:3180`): `OPR=131313₈` → `SINIT`
(create SYSTEM); anything else → normal boot to `@ENTER`. On nd100x: `--opr=131313`.

---

## 3. Remaining work — the MINIT bring-up (the finish line)

The mandatory sequence is **MINIT (format) → cold-boot `--opr=131313` (SINIT) →
normal boot (login)**. Only the first step is unbuilt. Concrete TODO:

1. **Assemble MINIT** (`src/MINIT.SYMB`) to a BPUN with the **fixed** `mac-as`.
   Decide the `CDC` vs `N10` build marks (it has both, like TSS). No build script
   exists yet — add one in `mac-c/` (mirror `build_tss_drum.sh`).
2. **Find MINIT's entry/start address** and how it is booted on nd100x (it's a
   standalone program with its own `TCI`/`TCO`; likely boot its BPUN like the TSS
   drum image and start at its entry).
3. **Run MINIT** against the CDC disc image the emulator will boot TSS from.
   It prompts (verified, `MINIT.SYMB:228-234`):
   ```
   FIRST DISK ADDRESS (NCR): <first track, octal>
   LAST  DISK ADDRESS (NCR): <last track, octal>
   INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): I
   ```
   Derive the correct FIRST/LAST **NCR** values from the driver's NCR→physical
   mapping for the emulated CDC geometry (nd100x CDC device = 512 sectors/surface).
4. **Cold-boot TSS** with `--opr=131313` on that formatted image → `SINIT`
   creates SYSTEM.
5. **Normal boot** (omit `--opr`) → `@ENTER` → type `SYSTEM` → passwordless login.

**Verification tools available:** drive the console via the DAP debugger
(`--debugger --port=<p>`, then `console_write`/`console_read` on terminal 192).
Use DAP port **1777** for TSS to avoid clashing with other tooling (memory
`tss-dap-port`). Confirm success by seeing the login echo and LOGON advancing
past its read loop; break at `GTRK`/`CRUSE` to confirm track allocation succeeds
after MINIT.

---

## 4. Quick reference — key symbols and source lines

| symbol | addr | source | role |
|---|---|---|---|
| cold-start OPR test | — | `TSS1.SYMB:3180` | `OPR=131313 → SINIT` |
| `SINIT` | `022057` | `TSS2.SYMB:858` | cold create SYSTEM (calls `CRUSE`) |
| `CRUSE`/`FCRUS` | `031272` / OV10 | `TSS5.SYMB:74` | create user: `RUTBL`/`GTRK`/`WUTBL`/`IUSER` |
| `CRUSR` | OV7 | `TSS4.SYMB:927` | interactive create user (writes `USRDK` too) |
| `MINIT` | standalone | `src/MINIT.SYMB` | disc formatter (writes MIB bitmap) |
| `LOGON` | `032736` | `TSS4.SYMB:546` | login (name via `USTBL`, pw via `USRDK`) |
| `USARR` | `032430` / OV1A | `TSS3.SYMB:651` | name accessor — reads `USTBL` |
| `ABLKP` | `032444` / OV1A | `TSS3.SYMB:730` | abbreviation/name matcher |
| `WBUF`/`RBUF` | `000610`/`000702` | `TSS1.SYMB:351`/`391` | teletype ring — where the `9377` bug bit |
| `9377` literal | `0o747` | `TSS1.SYMB:407` | `377` byte-mask (the mis-assembled symbol) |

Build/test entry points (from `mac-c/`): `make test`, `./build_tss_drum.sh`,
`./build_tss_assysa.sh`, `./compare_asymb.sh`.
