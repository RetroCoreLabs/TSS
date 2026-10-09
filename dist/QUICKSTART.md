# NORD TSS 3.0 — ready-to-boot disc set

A timesharing operating system written by Bo Lewendal in 1973 for the Norsk
Data NORD-1 / NORD-10, rebuilt from its original MAC assembler source and
running on the nd100x emulator.

This archive contains a **ready-to-boot disc**: the CDC cartridge disc has
the bootstrap installed, is formatted and cold-started, and the user
`SYSTEM` exists. The disc image keeps everything you (and TSS) write to it.

## What is in this archive

| file | what it is |
|---|---|
| `cdc.img` | the CDC cartridge disc: bootstrap installed, formatted, cold-started, user `SYSTEM` created (10-teletype `TEL10` system, see section 4) |
| `drum.img` | the NORD-10 swapping drum (IOX 540), blank; filled in by the running system |
| `tss.cfg` | the validated nd100x configuration: boots from the CDC disc |
| `run-tss.sh` | starts nd100x with the settings TSS needs |

Keep a copy of `cdc.img` if you want a fresh-system restore point - the
running system writes to it.

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

Every boot:

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

## 4. More than one user (terminals)

The shipped build is assembled for **10 teletypes** (`TEL10`). On nd100x the
usable ones are:

| TSS teletype | nd100x terminal | IOX | ident |
|---|---|---|---|
| TTY1 | console (the nd100x window / pty) | 0300 | 1 |
| TTY5 .. TTY8 | TERMINAL 5 .. 8 | 0340 .. 0370 | 044 .. 047 |
| TTY9, TTY10 | TERMINAL 9, 10 | 01300, 01310 | 050, 051 |

**TTY2-TTY4 are unusable on nd100x**: TSS expects them at IOX 0310-0330 with
idents 5, 6, 7, but nd100x's terminals at those addresses answer idents
0121-0123, which TSS's level-12 dispatch sends to `LNONE`.

An extra teletype is **dead** until **ESC** (033 octal) is typed on it; the
level-6 scanner then starts its process and it prints `@ENTER`. Any other
character typed on a dead teletype is read and dropped.

To reach TERMINAL 5-10 from outside the emulator window use nd100x's telnet
server: `nd100x --config=tss.cfg --mms=1 --telnet=9077`, then
`telnet localhost 9077`, pick a terminal from the menu (nd100x names them by
logical device number: `Terminal 39` is TERMINAL 8, `Terminal 48` is
TERMINAL 9, `Terminal 49` is TERMINAL 10 - the start-up log line
`Terminal 39 created (TERMINAL 8/ TET9, ident 47, address 370)` gives the
mapping), press ESC, log in as in section 3, and `WHO-IS-ON` lists both lines.

## Known limitations

Two defects of the 1973 source were found and fixed in the repository on
2026-10-08. A kit built from source before that date still has them. Both
fixes were verified on nd100x the same day: `DATE` advances in step with
`TIME-USED`, and `SET-REGISTER A 1234` / `X 777` / `B 4321` / `T 55` all read
back correctly in `STATUS` with `STS` untouched.

- **The time of day did not advance.** `DATE` returned whatever was last set.
  TSS is a 48-bit floating-point program, but the four clock constants in
  `TBANG` (`src/TSS2.SYMB`) were assembled in the 2-word 32-bit format, so
  every division came out zero. The archived 1978 build has the same
  constants, so this is a defect in the original. Fixed under the `CLKFX`
  mark, which the NORD-10 build input sets; the golden NORD-1 builds keep the
  original words. See `docs/TSS-FLOAT-FORMAT.md` section 8.
- **`SET-REGISTER` set the wrong register.** The NORD-10 path of `SETX`
  (`src/TSS3.SYMB`) indexed its register table with an addressing mode whose
  8-bit displacement cannot reach the table, so the write landed in the
  status register. Fixed in place; the 1978 builds never assembled this path.
  See `docs/TSS-COMMAND-VALIDATION.md` PART III section 4.
- Elapsed-time accounting (`TIME-USED`, `LOGOUT`) always worked.

Everything else — the user lifecycle, file system, memory assignment,
accounting, `PAUSE`/`CONTINUE`, device reservation — has been exercised
command by command against a running machine.

## More

Source, documentation and the full command-validation record:
https://github.com/HackerCorpLabs/TSS
