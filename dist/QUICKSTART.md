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

**`--mms=1` is required.** `run-tss.sh` passes it on the command line.
nd100x 1.0.15 and later also accept `mms = 1` under `[machine]` in the
config file; older builds reject that key, so `tss.cfg` leaves it out.
TSS is 1973 code that programs the MMU the
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

| TSS teletype | nd100x terminal | nd100x screen name | IOX | ident | reachable at start |
|---|---|---|---|---|---|
| TTY1 | console (the nd100x window / pty) | `Console` | 0300 | 1 | locally |
| TTY5, TTY6, TTY7 | TERMINAL 5, 6, 7 | `Terminal 36`, `37`, `38` | 0340, 0350, 0360 | 044, 045, 046 | locally, F12 virtual screen |
| TTY8 | TERMINAL 8 | `Terminal 39` | 0370 | 047 | telnet |
| TTY9, TTY10 | TERMINAL 9, 10 | `Terminal 48`, `49` | 01300, 01310 | 050, 051 | telnet |

nd100x names each terminal by its logical device number, as in the third
column. Run nd100x with `--verbose` to see the mapping in the start-up log,
for example `Terminal 39 created (TERMINAL 8/ TET9, ident 47, address 370)`.

**TTY2-TTY4 are unusable on nd100x**: TSS expects them at IOX 0310-0330 with
idents 5, 6, 7, but nd100x's terminals at those addresses answer idents
0121-0123, which TSS's level-12 dispatch sends to `LNONE`.

An extra teletype is **dead** until **ESC** (033 octal) is typed on it; the
level-6 scanner then starts its process and it prints the banner and
`@ENTER`. Any other character typed on a dead teletype is read and dropped.
Log in as in section 3.

### F12: the virtual screens

Press **F12** in the nd100x window for the nd100x menu, then **2** for the
Virtual Screen Selector:

```
=== Virtual Screens (telnet port 9077) ===
  [1] Console                  *
  [2] Terminal 36              [Virtual]
  [3] Terminal 37              [Virtual]
  [4] Terminal 38              [Virtual]
  [5] Terminal 39              [Inactive]
  [6] Terminal 48              [Inactive]
  [7] Terminal 49              [Inactive]
  [8] Terminal 50              [Inactive]
  [9] Line Printer             (output only)
  [a] Paper Tape Punch         (output only)
  [b] Log                      (output only)
  [R] Release terminal (virtual->inactive, or disconnect telnet)
  [P] Pending connections (live view)
```

`*` marks the screen shown in the window. `[Virtual]` screens belong to the
nd100x window; `[Inactive]` screens are free for a telnet client.

- **Use TTY5-TTY7 in the nd100x window:** press the screen's number (`2` for
  `Terminal 36` = TTY5), then ESC to wake it. Go back with F12, 2, 1.
- **Hand TTY5-TTY7 to telnet:** press **R**, then the screen's number. The
  screen turns `[Inactive]` and appears in the telnet menu. The console and
  the screen currently shown cannot be released. Switching back to a
  released screen takes it back from telnet.
- **R** on a screen that has a telnet client disconnects that client.

### Telnet

Start nd100x with its telnet server:

```bash
nd100x --config=tss.cfg --mms=1 --telnet=9077
```

then `telnet localhost 9077`. The menu lists the `[Inactive]` terminals; at
start that is `Terminal 39`, `48`, `49` and `50` (TTY8, TTY9, TTY10, and
TERMINAL 11, which is not in the table above). Type the menu number, press ESC, log in
as in section 3. `WHO-IS-ON` then lists both lines, for example `1 SYSTEM`
and `8 SYSTEM`.

## Known limitations

No open defects are known. TTY2-TTY4 cannot be used on nd100x (section 4).

Two defects in the 1973 source were fixed before this release: the time of
day did not advance, and `SET-REGISTER` wrote the wrong register. The
details are in the source repository, `docs/TSS-FLOAT-FORMAT.md` section 8
and `docs/TSS-COMMAND-VALIDATION.md` PART III section 4.

The user lifecycle, file system, memory assignment, accounting,
`PAUSE`/`CONTINUE` and device reservation have been exercised command by
command against a running machine.

## More

Source, documentation and the full command-validation record:
https://github.com/HackerCorpLabs/TSS
