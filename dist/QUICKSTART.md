# NORD TSS 3.0 — disc set and bring-up kit

A timesharing operating system written by Bo Lewendal in 1973 for the Norsk
Data NORD-1 / NORD-10, rebuilt from its original MAC assembler source and
running on the nd100x emulator.

This archive contains the **built system and blank media**, plus a
ready-made disc for the impatient. Either **jump start** with the ready
disc (§2a), or bring the disc up yourself (§2b) — the same three steps an
operator performed in 1973: install the bootstrap from the distribution
tape, format the disc, cold-start the system. It takes about two minutes,
and you only do it once: the disc image keeps everything you (and TSS)
write to it.

## What is in this archive

| file | what it is |
|---|---|
| `tss.bpun` | the assembled system, as a BPUN paper-tape image with an autostart address |
| `minit.bpun` | MINIT, the standalone disc-format program, as a paper tape |
| `cdbin-boot.bpun` | the CDBIN distribution tape: booting it makes TSS's own bootstrap loader write the disc boot sectors |
| `cdc.img` | the CDC cartridge disc — carries the system overlays, **not yet formatted** |
| `cdc-jumpstart.img` | the same disc **after** the bring-up: bootstrap installed, formatted, cold-started, user `SYSTEM` created |
| `drum.img` | the NORD-10 swapping drum (IOX 540), blank; filled in by the running system |
| `tss.cfg` | the validated nd100x configuration for normal boots |
| `run-tss.sh` | starts nd100x with the settings TSS needs |

## 1. Get the emulator

TSS needs the **nd100x** ND-100 emulator, which is a separate project:

```bash
git clone https://github.com/HackerCorpLabs/nd100x.git
cd nd100x
sudo apt install build-essential cmake libcjson-dev   # Debian/Ubuntu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary lands in `build/` — `find build -name nd100x -type f`.

## 2a. Jump start — skip the bring-up

The archive includes `cdc-jumpstart.img`: a disc that has already been
through the bring-up below (bootstrap installed, MINIT-formatted, cold
started — user `SYSTEM` exists and the disc is bootable on its own). To use
it:

```bash
cp cdc-jumpstart.img cdc.img
./run-tss.sh /path/to/nd100x
```

Then log in as in section 4. Do the bring-up yourself instead if you want
the 1973 operator experience — it is three commands.

## 2b. One-time bring-up: bootstrap, format, cold start

Run these from the unpacked archive. `ND` is your emulator binary:

```bash
ND=/path/to/nd100x
```

**Step 1 — install the disc bootstrap.** Boot the CDBIN distribution tape.
TSS's own tape loader (`HLOAD` → `TBOOT`) reads it and writes the boot
sectors to the disc itself, exactly as the 1973 install did. It ends in a
halt; the instruction bound makes the command return on its own:

```bash
$ND --boot=tape --image=cdbin-boot.bpun --cdc=cdc.img --max-instr=2000000
```

**Step 2 — format the disc.** Boot MINIT and answer its prompts. `4470` is
the first user-storage disc address, `4670` the last (giving 16 free
tracks), `I` means INITIALIZE. Press **Ctrl-C** when it prints `FINISHED`:

```bash
$ND --boot=bpun --image=minit.bpun --cdc=cdc.img
```
```
MASS STORAGE INIT
FIRST DISK ADDRESS (NCR): 4470
LAST DISK ADDRESS (NCR): 4670
INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): I
...
FINISHED            <- Ctrl-C here
```

**Step 3 — cold start.** The operator-panel setting `131313` makes TSS run
`SINIT`, which initialises the file system and creates the passwordless user
**SYSTEM**. Wait for `@ENTER` on the console, then press **Ctrl-C**:

```bash
$ND --boot=bpun --image=tss.bpun --cdc=cdc.img --drum=drum.img --opr=131313 --start=7
```

Ctrl-C is the correct stop — the emulator writes every disc sector through
to `cdc.img` immediately, so nothing is lost.

The disc is now live. Keep a copy of `cdc.img` if you want a fresh-system
restore point.

## 3. Run TSS

Every normal boot from here on:

```bash
./run-tss.sh /path/to/nd100x
```

or by hand:

```bash
nd100x --config=tss.cfg --mms=1
```

**`--mms=1` is required.** nd100x accepts `mms` only on the command line, not
in the config file. TSS is 1973 code that programs the MMU the
Paging-System-I way; with the default MMS2 the machine starts and then hangs
in the overlay loader.

After any completed cold start or login, the disc alone is bootable — this
is what the front-panel LOAD button did on the real machine, and what TSS's
`LOAD-SYSTEM` command relies on:

```bash
$ND --boot=cdc --image=cdc.img --cdc=cdc.img --drum=drum.img
```

## 4. Log in

```
NORD TSS VERSION 3.0A IS UP

@ENTER SYSTEM
PROJECT NUMBER P-1
TYPE IN DATE (DD,MM,YYYY,HH,MM,SS): 26,07,2026,08,30,00
@
```

`SYSTEM` has no password. The date prompt appears on the first login only.
At the `@` prompt, `HELP` lists all 60 commands.
Try `DISK-SPACE`, `LIST-USERS`, `WHO-IS-ON`, `DATE`, `MEMORY 0`.

To create a file you must first give `SYSTEM` disc quota, which a fresh
system has none of:

```
@TRANSFER
TO USER: SYSTEM
FROM USER: SYSTEM
NUMBER OF TRACKS: 20
```

Then `OPEN-FILE "MYFILE",WX` works. The quotes are a delimiter **pair** —
`OPEN-FILE "MYFILE,WX` gives `BAD FILENAME`.

## Known limitations

These are faithfully reproduced from the original — not emulator problems:

- **The time of day does not advance.** `DATE` returns whatever was last set.
  TSS is a 48-bit floating-point program whose clock constants were assembled
  in the 32-bit format, so every division underflows to zero. This is a defect
  in the **1978 original**: the archived build has the same constants.
- **`SET-REGISTER` sets the wrong register.** Under investigation.
- Elapsed-time accounting (`TIME-USED`, `LOGOUT`) *does* work.

Everything else — the user lifecycle, file system, memory assignment,
accounting, `PAUSE`/`CONTINUE`, device reservation — has been exercised
command by command against a running machine.

## More

Source, documentation and the full command-validation record:
https://github.com/HackerCorpLabs/TSS
