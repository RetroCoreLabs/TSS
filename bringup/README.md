# TSS bring-up toolkit — run and validate it yourself

Scripts to take a bare disc all the way to a NORD TSS 3.0 login, plus the tools
to validate each step. **Run everything from WSL** (the nd100x emulator and the
POSIX tools live there):

```bash
cd /mnt/e/Dev/Ronny/TSS/bringup
```

For *how it is supposed to work* (control flow, with Mermaid diagrams), read
[`../docs/TSS-ARCHITECTURE.md`](../docs/TSS-ARCHITECTURE.md) (cold-start and
login chapters, incl. the
scheduler/interrupt mechanism). Those are the reference used while debugging.

---

## The two facts that make or break the bring-up

1. **Cold-start at ADDRESS 7, not the BPUN autostart.** The tape's built-in start
   (`ISTRT`) is the *normal-running* entry and skips the operator-switch (`OPR`)
   test that runs `SINIT`. You must enter at **`--start=7`**.
2. **Pre-size the CDC disc.** The emulated CDC device does not grow on writes; the
   user area (NCR `4470₈`) maps to a high physical sector, so the backing file
   must be padded (the scripts pad to 8192 sectors / 4 MB).

---

## Prerequisites (build the images once)

```bash
cd /mnt/e/Dev/Ronny/TSS/mac-c
make                 # mac-as + tests  (expect: 695 passed, 0 failed)
                     # NOTE: "make test" alone does NOT relink mac-as —
                     # always run plain "make" after assembler changes
./build_tss_drum.sh  # -> Build/drum/tss-drum.bpun + tss-cdc.img   (0 errors)
./build_minit.sh     # -> Build/minit/minit.bpun                   (0 errors)
```

---

## The four steps (local-console, type it yourself)

| step | script | what you do | verify |
|---|---|---|---|
| 1 | `./1-prepare-disc.sh` | (nothing — makes a fresh padded disc in `Build/bringup/`) | — |
| 2 | `./2-format-minit.sh` | type `4470` ⏎ `4670` ⏎ `I` ⏎ ; at `FINISHED` press **Ctrl-C** | `python3 verify-disc.py` → *16 free tracks* |
| 3 | `./3-create-system.sh` | wait for `@ENTER`; press **Ctrl-C** | `python3 verify-disc.py` → *15 free + SYSTEM* |
| 4 | `./4-login.sh` | at `@ENTER` type `SYSTEM` ⏎ then `1` ⏎ | you reach the `@` command prompt |

**Always stop the emulator with Ctrl-C** — that flushes the CDC surface back to
`cdc.img`. Killing it another way loses disc writes.

### What each prompt means
- MINIT `FIRST/LAST DISK ADDRESS (NCR)` — octal disc addresses; `4470` is the start
  of user storage (FSYS), `4670` gives 16 free tracks. `I` = INITIALIZE.
- TSS `@ENTER` — the login prompt (one per terminal).
- `PROJECT NUMBER P-` — needs a **positive** number; `0`/blank re-prompts.
- SYSTEM is **passwordless**, so there is no `PASSWORD` prompt.
- `AMBIGUOUS FILENAME` after the project number is **non-fatal** on a fresh disc
  (the ()SCRATCH file does not exist yet); login continues.
- `TYPE IN DATE (DD,MM,YYYY,HH,MM,SS)` — first login only; answer e.g.
  `23,07,2026,15,30,00`. Then the `@` command prompt appears — try `HELP`.

---

## Validate at any time

```bash
python3 verify-disc.py            # reads Build/bringup/cdc.img
python3 verify-disc.py /path/to/other.img
```

It reports the MIB free-track count and whether `SYSTEM` is in the user table,
and tells you which step to run next.

---

## Driving it over DAP (scripted / step-through debugging)

For breakpoints, register/memory inspection, or automated console I/O, run a
stage under the debugger instead of the local console:

```bash
./debug-with-dap.sh minit     # MINIT, port 1777
./debug-with-dap.sh cold      # cold-start (--start=7 --opr=131313)
./debug-with-dap.sh login     # normal boot (--start=7)
```

Then connect a DAP client to `127.0.0.1:1777`:
1. enable console capture on **terminal 192**,
2. attach, set any breakpoints, `continue`,
3. send console input as **raw bytes** — a carriage return must be the byte
   `0x0D` (a literal `\r` is echoed as backslash-r):
   - `SYSTEM`+CR = `hex 53 59 53 54 45 4D 0D`
   - `4470`+CR = `hex 34 34 37 30 0D`, `I`+CR = `hex 49 0D`

**Key addresses (octal):** `MINIT` 000206 · `SINIT` 022057 · `CRUSE` 031272 ·
`LOGON` 032736 · `ABLKP` 032444 · `S10` 07271 · OPR test 07307 · `XDISK` 021137 ·
`DWAIT` 010313 · `INIT` 000301 · `SYSSV` 010603. Disc: `MIB/DKBIT` 50 ·
`USRDK` 1460 · `FSYS` 4470.

---

## Troubleshooting

| symptom | cause | fix |
|---|---|---|
| MINIT prints `DISK ERROR` on every track | CDC image too small | re-run `1-prepare-disc.sh` (pads to 8192 sectors) |
| cold-start never shows `@ENTER` | started at ISTRT, not 7 | ensure `--start=7` (the scripts set it) |
| no console echo at all | wrong console charset/terminal | it is terminal 192 / the local console; check `--start=7` |
| `verify-disc.py` shows 16 free after step 3 | SINIT never ran | you booted without `--opr=131313` or not at addr 7 |
| login loops silently after the project number | stale `mac-as`: `make test` does NOT relink it | run plain `make` in `mac-c/`, rebuild images, re-run steps 1-3; check with `python3 check-robj-encoding.py ../Build/bringup/tss.bpun` (must say FIXED) |
| (historical) login "hang" after SYSTEM | forward-ref addend bug, FIXED 2026-07-23 | see `../docs/MAC-ASSEMBLER.md` (defect record); regression-pinned by test [15] |

---

## Config

Override the emulator path or DAP port via environment variables:

```bash
ND=/path/to/nd100x  DAP_PORT=1780  ./3-create-system.sh
```

Working images are written to `Build/bringup/` (disposable; re-created by step 1).
