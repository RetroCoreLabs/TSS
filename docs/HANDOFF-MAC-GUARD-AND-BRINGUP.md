# Handoff — mac-c hardening, TSS bring-up, and the login open item

**Date:** 2026-07-23. **Branch:** `mac-undefined-guard-rgdiv`.
**One-line status:** the bare-disc → SYSTEM-account bring-up **works and is
verified**; mac-c gained real silent-miscompile fixes (`CLD`, `STR→XTR`) and an
undefined-operand guard; the **interactive login still hangs** in the overlay
subsystem (not the assembler) — the one remaining thread.

Read alongside: [`TSS.md`](TSS.md) (doc hub), [`TSS-BRINGUP-RUNBOOK.md`](TSS-BRINGUP-RUNBOOK.md)
(the verified runbook), the `bringup/` scripts, and memory notes
`mac-c-undefined-operand-guard`, `tss-opr-coldstart-entry`, `tss-login-stall-diagnosis`.

---

## 1. What now works (verified)

### Bring-up to a created SYSTEM account
- **MINIT** builds from the original source (`make minit` → `Build/minit/minit.bpun`)
  and runs on nd100x: formats a CDC disc (writes the MIB free-track bitmap).
  New build script `mac-c/build_minit.sh`.
- **Cold-start at ADDRESS 7** (not `ISTRT`) with `--opr=131313` runs the real
  cold-start (`SYSSV→INIT→scheduler→S10→OPR test→SINIT→CRUSE`) and **creates user
  SYSTEM** — disc-verified (a track allocated in the MIB, name `SYSTEM` written to
  USTBL sector 106), and it reaches the `@ENTER` login prompt.
- The **key gotcha**: the drum BPUN autostart (`-e ISTRT`) enters at the normal
  LEV2/LOGON path and skips the OPR test; you must enter at address 7.
  See `tss-opr-coldstart-entry` and `TSS-BRINGUP-RUNBOOK.md`.
- The **CDC image must be pre-sized** (padded to 8192 sectors) — the emulated CDC
  device does not grow on writes, and the user area (NCR 4470₈) maps to a high
  physical sector.

### mac-c assembler hardening (all `make test` 688/0, golden 679/693 preserved)
- **`CLD = 0100`** added to `mac_permsym.c` — a clear-destination register-op
  modifier the "byte-verified" extraction had missed. **Validated on the REAL
  MAC by hand**: `SWAP CLD DT SA`→144156, `SWAP DT SA`→144056 (diff 0100);
  mac-c now reproduces both. Faithful to real MAC, not a deviation.
- **`STR→XTR` rename completed** — 48 dangling `STR0/1/2/STRX/STR1X/STR2X`
  references (the incomplete CVS rename) fixed to their `XTR*` definitions, so
  `SAT STR1`(silently `SAT 0`) → `SAT XTR1`. Verified in the built overlay.
- **Undefined-operand guard** (`mac.c` + `mac.h` `used_as_operand`): a still-
  undefined symbol consumed as the VALUE OPERAND of a defined instruction
  (MRI/JUMP8/ARG8) is now a **hard build error** ("undefined operand:"). This is
  the class the golden ADDRESS dumps cannot see (word count unchanged). Library
  marks are not flagged (no false positives on golden or drum builds). Test `[14]`.

### Documentation + tooling
- Architecture docs: `TSS-ARCHITECTURE`, `TSS-DRIVERS`, `TSS-FILESYSTEM`,
  `TSS-USERS`, `TSS-COMMANDS-AND-PROGRAMS`, `TSS-BOOT-FLOW` (mermaid),
  `TSS-LOGIN-FLOW` (mermaid), `TSS-BRINGUP-AND-MINIT`, `TSS-BRINGUP-RUNBOOK`,
  and the hub `TSS.md`. All source-cited, `[VERIFIED]`/`[ASSUMPTION]` tagged.
- **`bringup/` scripts** — `1-prepare-disc.sh` … `4-login.sh`, `verify-disc.py`,
  `debug-with-dap.sh`, and `README.md`: run the whole bring-up yourself.
- `mac-c/Makefile` restructured: `make` / `test` / `tss` / `drum` / `minit` / `all`,
  with `mac-as` a prerequisite of every artifact.

---

## 2. The open item — interactive login hangs (NOT the assembler)

After `@ENTER`, typing `SYSTEM<CR>` echoes the name, then the process blocks at
the LEV0 WAIT-trap idle (virtual 006106 = `MIN 5; JMP *-1`) and never resumes.

**Ruled out** (verified): the `9377` char bug (image decodes correctly); the
char-read + USRDK disc-read (a breakpoint at `ABLKP` fires after the name); the
STR corruption (fixed in the running overlay, yet `ABLKP` step-out STILL yields
to LEV0); a missing emulator interrupt (nd100x delivers IRQs; TSS disc I/O is
busy-poll, not interrupt-driven — see `tss-login-stall-diagnosis`).

**Root location:** `ABLKP` (032444) is the GOVER **overlay trampoline**. LOGON
is overlay `OV6`; ABLKP is overlay `OV1A`. So `LOGON→ABLKP` forces a **nested
overlay load** (OV1A over OV6 in the same VOR window at 40000₈); the process
yields to LEV0 for the level-5 overlay reader and never becomes runnable again.

**Next step:** breakpoint the level-5 overlay reader (`S5`/`SDISK`, TSS1:3086/3151)
and `LEV5` (2720), type SYSTEM, and watch whether the OV1A load runs, completes
(busy-poll `DWAIT` returns ready), and re-schedules the LOGON process — dumping
the PRT process-table flags for the LOGON slot. That names why the nested overlay
never resumes. It is a TSS overlay-subsystem/scheduler condition, fixable on our
side (no nd100x change).

---

## 3. Validation / how to reproduce

```bash
cd mac-c && make test              # 688 passed, 0 failed
./build_tss_assysa.sh && ./compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
                                   # 13 macro names + QOV1C off-by-1 (679/693, unchanged)
./build_tss_drum.sh                # 0 errors
```
Bring-up (from `bringup/`, WSL): `./1-prepare-disc.sh` → `./2-format-minit.sh`
(type 4470 / 4670 / I) → `./3-create-system.sh` (wait for @ENTER) →
`python3 verify-disc.py` (expect 15 free + SYSTEM). See `bringup/README.md`.

---

## 4. Constraints carried forward
- **Do not change nd100x without approval** (`no-nd100x-changes-without-approval`).
  The login fix is TSS-side; a real-MAC-oracle would need a ~12-call MON shim in
  nd100x (placeholder exists in `ndfunc_mon`) — approval-gated. The user can
  assemble on real MAC by hand as the current oracle.
- `mac_permsym.c` is generated/byte-verified; the `CLD` addition is documented as
  a corrected extraction gap. TODO: re-audit the extraction for other missing
  register-op modifiers.

---

## 5. Key files changed this session
- `mac-c/mac.c`, `mac.h`, `mac_permsym.c`, `test_mac.c` — guard, CLD, tests.
- `mac-c/Makefile`, `mac-c/build_minit.sh` — build pipeline.
- `src/TSS2.SYMB`, `TSS3.SYMB`, `TSS4.SYMB`, `TSS5.SYMB` — STR→XTR rename.
- `docs/` — the analysis/bring-up docs above; `bringup/` — the driver scripts.
- `Build/` — regenerated outputs (disposable).
