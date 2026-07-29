# NORD TSS 3.0 — ready-to-run disc set

A timesharing operating system written by Bo Lewendal in 1973 for the Norsk
Data NORD-1 / NORD-10, rebuilt from its original MAC assembler source and
brought up on the nd100x emulator.

## What is in this archive

| file | what it is |
|---|---|
| `tss.bpun` | the assembled system, as a BPUN paper-tape image with an autostart address |
| `cdc.img` | the CDC cartridge disc: MINIT-formatted, cold-started, user `SYSTEM` created |
| `drum.img` | the NORD-10 swapping drum (IOX 540), filled in by the running system |
| `tss.cfg` | the validated nd100x configuration |
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

## 2. Run TSS

From this unpacked archive:

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

## 3. Log in

```
NORD TSS VERSION 3.0A IS UP

@ENTER SYSTEM
PROJECT NUMBER P-1
TYPE IN DATE (DD,MM,YYYY,HH,MM,SS): 26,07,2026,08,30,00
@
```

`SYSTEM` has no password. At the `@` prompt, `HELP` lists all 60 commands.
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

These are faithfully reproduced from the original, or gaps in the rebuild —
not emulator problems:

- **The time of day does not advance.** `DATE` returns whatever was last set.
  TSS is a 48-bit floating-point program whose clock constants were assembled
  in the 32-bit format, so every division underflows to zero. This is a defect
  in the **1978 original**: the archived build has the same constants.
- **`LOAD-SYSTEM` never returns.** It reads disc page 0 and jumps to it, but
  this disc has no boot sector written. The command is correct; the disc is
  unprepared.
- **`SET-REGISTER` sets the wrong register.** Under investigation.
- Elapsed-time accounting (`TIME-USED`, `LOGOUT`) *does* work.

Everything else — the user lifecycle, file system, memory assignment,
accounting, `PAUSE`/`CONTINUE`, device reservation — has been exercised
command by command against a running machine.

## More

Source, documentation and the full command-validation record:
https://github.com/HackerCorpLabs/TSS
