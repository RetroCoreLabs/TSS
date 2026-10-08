# TSS 3.0 and SINTRAN III - what they share

**This file:** `docs/TSS-AND-SINTRAN.md`

NORD TSS 3.0 is dated 1973 by its own source: `src/TSS1.SYMB` line 1 reads
`NORD TIMESHARING SYSTEM  BY BO LEWENDAL`, and the NORD-10 drivers are
signed `NJL 17/4/73` (`src/TSS1.SYMB:3763`) and `NJL 28/5/73`
(`src/MINIT.SYMB:596`). SINTRAN III is Norsk Data's later operating system.
This document compares the two at three levels, monitor calls, commands, and
files and users, using only what was read on both sides.

**Sources on the SINTRAN side** (under `$NDINSIGHT/Reference-Manuals/`):

| short name | document |
|---|---|
| MC | ND-860228-2-EN *SINTRAN III Monitor Calls* |
| UG | ND-60.050.06 *SINTRAN III Users Guide*, the version of June 1976 (its page 1) |
| RM | ND-60.128.5 *SINTRAN III Reference Manual* |
| TBG | ND-60.132.03 *SINTRAN III Timesharing Batch Guide* |

Line numbers below refer to the Markdown transcriptions of those manuals.

## 1. Monitor calls

**Calling convention: the same.**

| | TSS 3.0 | SINTRAN III |
|---|---|---|
| instruction | `MON n` = `161000` + n; the trap handler `PVI` recognises it by `AND (177400; SUB (161000` (`src/TSS1.SYMB` `LEV14`) | `MON n` = `161000` + n |
| call number | low 8 bits of the instruction, `AND (377` (`src/TSS1.SYMB` `LEV3`, NORD-10 variant) | low 8 bits |
| dispatch | table `MCTBL` indexed by the number; a number past `MCSIZ` or a zero entry goes to the error exit `LF` (`src/TSS1.SYMB:4302-4331`) | dispatch table; unused numbers go to an error routine |
| arguments | the user's A, T, X and D, reloaded from the user's register block before the routine runs (`src/TSS1.SYMB` `LEV3`) | A, T, X, D |
| error return | `INBT`: error code in A, no skip; success skips (`src/TSS2.SYMB` `INBT`, `ISS, MIN LREG,B`) | MC examples: `JMP ERROR` right after the `MON`, then the normal path, error number in A |

The TSS error/skip rule was read in `INBT` only; that it holds for every TSS
call is inferred.

**Call table.** `MCTBL` has 81 entries, octal 0-120. 18 have the same number
and the same name in the numeric list of MC:

| octal | TSS | SINTRAN III | argument registers checked on both sides |
|---|---|---|---|
| 0 | `LEAVE` | `LEAVE` | - |
| 1 | `INBT` | `INBT` | T = file or device; byte in A - same |
| 2 | `OUTBT` | `OUTBT` | T, A - same |
| 3 | `ECHOM` | `ECHOM` | not checked |
| 4 | `BRKM` | `BRKM` | A = mode, X = table in both; SINTRAN adds T and D |
| 5 | `RDISK` | `RDISK` | T = page/block, X = buffer - same |
| 6 | `WDISK` | `WDISK` | not checked |
| 7 | `RPAG` | `RPAGE` | T = file, A = block, X = buffer - same |
| 10 | `WPAG` | `WPAGE` | not checked |
| 13 | `CIBUF` | `CIBUF` | not checked |
| 14 | `COBUF` | `COBUF` | not checked |
| 32 | `MSG` | `MSG` | X = string - same |
| 35 | `IOUT` | `IOUT` | A = number, T = radix/format - same |
| 64 | `ERMSG` | `ERMSG` | not checked |
| 65 | `QERMS` | `QERMS` | not checked |
| 66 | `ISIZE` | `ISIZE` | not checked |
| 67 | `OSIZE` | `OSIZE` | not checked |
| 76 | `SETBS` | `SETBS` | T = file, A = block size - same |

Three more keep their function but moved: `OPEN` 42 became 50 (X = name,
A = type, T = access code 0-3 with the same four meanings), `REABT` 114
became 75, `SETBT` 115 became 74. Call 11 has the same number but a different
function (TSS `RCTIM`, SINTRAN `TIME`). The remaining TSS calls, mostly
string, mail and user utilities from 15 octal upward, have no SINTRAN
counterpart at their number.

## 2. Commands

TSS has 60 commands (`src/TSS5.SYMB:1791-1801`).

**24 exist under the same name in SINTRAN III:** `RECOVER`, `DUMP`,
`GOTO-USER`, `LOAD-BINARY`, `PLACE-BINARY`, `MEMORY`, `CONTINUE`, `LOGOUT`,
`HELP`, `MODE`, `STATUS`, `OPEN-FILE`, `CLOSE-FILE`, `DELETE-FILE`,
`CREATE-USER`, `DELETE-USER`, `CLEAR-PASSWORD`, `WHO-IS-ON`, `LIST-USERS`,
`TIME-USED`, `INIT-ACCOUNTING`, `CREATE-FRIEND`, `DELETE-FRIEND`,
`LIST-FRIENDS`. Only the names were matched; whether each behaves the same
was checked for a few (`DUMP`/`RECOVER`, `MODE <input> <output>`) and is
otherwise unknown.

**About 13 are renamed equivalents:**

| TSS | SINTRAN III |
|---|---|
| `RENAME` | `RENAME-FILE` |
| `LIST-FILE` | `LIST-FILES` |
| `WHERE-IS` | `WHERE-IS-FILE` |
| `RESERVE` / `RELEASE` | `RESERVE-DEVICE-UNIT` / `RELEASE-DEVICE-UNIT` |
| `PASSWORD` | `CHANGE-PASSWORD` |
| `DEFINE-FILE-ACCESS` | `SET-FILE-ACCESS` |
| `TRANSFER` | `GIVE-USER-SPACE` / `TAKE-USER-SPACE` |
| `SYSUP` / `SYSDOWN` | `SET-AVAILABLE` / `SET-UNAVAILABLE` |
| `ALLOCATE FILENAME,TRACK-ADDRESS,NUMBER-OF-TRACKS` | `ALLOCATE-FILE <file name> <page address> <no of pages>` (UG:2135): same arguments in the same order, pages instead of tracks |
| `DEFINE-DATE` | `DATCL` (meaning not checked) |
| `LIST-OBJECTS` | `DUMP-OBJECT-ENTRY` (weak) |

**About 23 were not found** in UG, RM or the SINTRAN command reference,
among them `SET-REGISTER`, `EXAMINE`, `CLOCK-ON`/`CLOCK-OFF`, `LINK-TO`,
`PAUSE`, `DISK-SPACE`, `LIST-TRACKS`, `DATE`, `SAVE-CORE`/`GET-CORE`,
`RBLOAD`, `BACKUP`, `DEFINE-VERSION` and `LOAD-SYSTEM`. Other SINTRAN
manuals were not searched, so "not found" is not proof of absence.

## 3. Files and users

| | TSS 3.0 | SINTRAN III | result |
|---|---|---|---|
| file name | `(USER)NAME:TYPE`, parsed by `OPEN` (`src/TSS3.SYMB:1361-1384`) | `(<file-directory-name>:<owner-name>) <filename>:<type>; version` (UG:1527) | same grammar; SINTRAN adds directory and version |
| new file | a name in quotation marks creates it (flag `NEWF`, `src/TSS3.SYMB`) | "with the file name surrounded by quotation marks" (UG:1422) | same rule |
| default types | `BIN PROG BRF SYMB DATA CORE` + `RB` (`src/TSS1.SYMB:4729-4735`) | `SYMB BRF PROG CORE BIN DATA` (UG:1436-1441) | 6 of 7 identical |
| login | ESC wakes the terminal, `ENTER`, password, `PROJECT NUMBER`, `@` (`docs/TSS-USER-MANUAL.md`) | ESC, `ENTER`, `PASSWORD:`, `PROJECT NUMBER:` (UG:1614-1640) | same sequence |
| friends | `CREATE-`/`DELETE-`/`LIST-FRIEND(S)`; a 6-word block per user holds the friend table and the access word (`src/TSS2.SYMB:180-182`), so the friend limit is not established | same three commands, "up to eight other users as friends" (UG:1386) | same model |
| access | one 9-bit word, owner / friend / public, 3 bits each (`src/TSS2.SYMB` `CKACC`) | owner / friends / public (UG:1388), access letters R W A C D (UG:1479-1483) | same groups, different bits |
| space quota | tracks, granted by SYSTEM (`TRANSFER`) | 1K-word pages (`GIVE-USER-SPACE`, RM:5127) | same idea, different unit |
| directories | one flat user area | several named directories | SINTRAN only |
| devices as files | `TELETYPE` = 1, `TAPE-READER` = 2, `LINE-PRINTER` = 5, matched before any file (`src/TSS3.SYMB` `O4`, table `PDEVS`) | `TERMINAL` is unit 1 (UG:2004), `LINE-PRINTER` and `TAPE-READER` are peripheral files | same idea, terminal renamed |

**Message texts mostly differ.** Of the TSS messages
(`src/TSS5.SYMB:1210-1241` and the second table), only `NO SUCH PAGE`
appears word for word (RM:1932). `AMBIGUOUS FILENAME` / `AMBIGUOUS FILE
NAME` and `NO SUCH FILE` / `NO SUCH FILE NAME` (TBG) differ by one word. The
friend and user messages were not found in SINTRAN.

## 4. What this proves, and what it does not

**Proven** (read on both sides): the same monitor-call instruction, number
field and table dispatch; 18 calls with the same number and name, with the
same argument registers wherever checked; 24 identical command names; the
same file-name grammar and quote rule; 6 of 7 default file types; the same
login sequence; friends; owner/friend/public access; devices usable as file
names.

**Inferred, not recorded anywhere found:** that SINTRAN III's monitor-call
interface, command language and file model were taken from TSS and extended.
The shared details are distinctive (hyphenated command names, the quote
rule, friends, identical low call numbers), not generic, but no document read
states the derivation.

**Unknown:** whether any SINTRAN code was copied from TSS code (that needs a
comparison of the code, not of the manuals); whether the 24 same-name
commands behave alike; who designed SINTRAN III.

**On the history.** Secondary sources in `$NDINSIGHT/History/sources/` (ndwiki
copies) say that SINTRAN II was real-time only, that NORD TSS was the
timesharing system of the NORD-1, and that SINTRAN III did both and replaced
them. They also say the first SINTRAN came from NTH and SINTEF in 1968 and
that the name joins SINTEF and FORTRAN; that statement comes from a 2009
copy of a Wikipedia article and has no reference, so it is not repeated here
as fact. Leads that could settle the derivation question, not yet read: the
TSS manual ND-60.039.01, Lewendal's *Timesharing: What, Why and Whiter* (spelled so in the ndwiki list, with "[sic]"), and
*Software Nord-10 Design Goals (TSS-02)*.
