# TSS bring-up — driven by the top-level Makefile

The bring-up is driven by the **repo-root `Makefile`** — run from WSL:

```bash
cd /mnt/e/Dev/Ronny/TSS
make help          # lists every target with a one-line description
```

The full sequence (interactive stages marked `*` — you type into the TSS
console on your terminal and stop the emulator with **Ctrl-C**, which flushes
the CDC disc image; any other kill loses disc writes):

```bash
make build         # mac-as + 695 tests + all TSS artifacts
make prepare       # STEP 1: fresh padded disc set in Build/bringup/
make format        # STEP 2*: MINIT — type 4470 ⏎  4670 ⏎  I ⏎ ; Ctrl-C at FINISHED
make verify        #   -> expect: 16 free tracks
make coldstart     # STEP 3*: creates user SYSTEM; Ctrl-C once @ENTER shows
make verify        #   -> expect: 15 free + SYSTEM
make login         # STEP 4*: at @ENTER type SYSTEM ⏎, project number 1 ⏎,
                   #   first login asks TYPE IN DATE — then you are at the @ prompt
```

`make dap-format` / `make dap-coldstart` / `make dap-login` run the same
stages under the DAP debugger (port 1777) for scripted control; stop those
with `make stop` (a flushing SIGINT). `make status` shows what state you are
in. Full reference: [`../docs/TSS-BRINGUP.md`](../docs/TSS-BRINGUP.md).

## What lives in this folder

| file | purpose |
|---|---|
| `verify-disc.py` | reads `Build/bringup/cdc.img`: MIB free-track count, SYSTEM present, and which step to run next |
| `check-robj-encoding.py` | byte-checks a built BPUN/image for the fixed-vs-broken ROBJ encoding — catches the stale-`mac-as` trap (`make test` does not relink `mac-as`) |

## What each prompt means

- MINIT `FIRST/LAST DISK ADDRESS (NCR)` — octal disc addresses; `4470` is the
  start of user storage (FSYS), `4670` gives 16 free tracks. `I` = INITIALIZE.
- TSS `@ENTER` — the login prompt (one per terminal).
- `PROJECT NUMBER P-` — needs a **positive** number; `0`/blank re-prompts.
- SYSTEM is **passwordless**, so there is no `PASSWORD` prompt.
- `AMBIGUOUS FILENAME` after the project number is **non-fatal** on a fresh
  disc (the ()SCRATCH file does not exist yet); login continues.
- `TYPE IN DATE (DD,MM,YYYY,HH,MM,SS)` — first login only; answer e.g.
  `23,07,2026,15,30,00`. Then the `@` command prompt appears — try `HELP`.

## Driving a DAP stage by hand

Connect a DAP client to `127.0.0.1:1777`, enable console capture on
**terminal 192**, attach, continue — and send console input as **raw bytes**
(a carriage return must be the byte `0x0D`):
`SYSTEM`+CR = `hex 53 59 53 54 45 4D 0D`, `4470`+CR = `hex 34 34 37 30 0D`.

## Troubleshooting

| symptom | cause | fix |
|---|---|---|
| MINIT prints `DISK ERROR` on every track | CDC image too small | re-run `make prepare` (pads to 8192 sectors) |
| cold-start never shows `@ENTER` | started at ISTRT, not 7 | the make targets pass `--start=7`; check overrides |
| `make verify` shows 16 free after coldstart | SINIT never ran | boot with `--opr=131313` at addr 7 (`make coldstart` does) |
| login loops silently after the project number | stale `mac-as` | run `make build`, `make prepare`, redo steps; `make check-encoding` must say FIXED |
| disc changes lost after a run | emulator not stopped with SIGINT | always Ctrl-C / `make stop` |

Override the emulator path or DAP port: `make login ND=/path/to/nd100x DAP_PORT=1780`.
Working images live in `Build/bringup/` (disposable; re-created by `make prepare`).
