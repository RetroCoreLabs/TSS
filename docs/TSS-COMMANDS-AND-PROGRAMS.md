# TSS 3.0 — Command Processor and User Programs

**Status / marking convention** (per user rules):
- `[VERIFIED]` — read directly from the recovered source, with a `file:line` citation.
- `[ASSUMPTION]` — my interpretation of verified source; not stated in the source itself.
- `NOT DETERMINED FROM SOURCE` — I could not establish it from the files.

All citations are into `E:\Dev\Ronny\TSS\src\*.SYMB` (the CVS-patched build, the
same tree the golden dumps were made from — see `CLAUDE.md`). Line numbers are the
1-based lines of those files. This document does **not** duplicate
`docs/TSS-Analysis.md`, `docs/LOGON-PATH.md`, `docs/PROJECT-DESCRIPTION.md` or
`docs/MAC-BPUN-Analysis.md`; it cross-references them.

---

## 1. The command processor

### 1.1 Where the loop lives

The command interpreter is `XSTAR` / entry `S1`, in overlay `OV19`, in
`src/TSS5.SYMB:1806` (`PROGM` … `GOVER OV19,S1,XSTAR` at
`src/TSS5.SYMB:1808`). It is also the **level-2 entry point** ("START AND LEVEL 2
ENTRY POINT / START COMMAND PROCESSOR", `src/TSS5.SYMB:1803-1804`). [VERIFIED]

The main loop is labelled `CLOOP` at `src/TSS5.SYMB:1844`. Each iteration:

1. `SAA ##@; JPL I (TCO` — emits the prompt character `@` (`src/TSS5.SYMB:1844`). [VERIFIED]
   `[ASSUMPTION]` `@` is the TSS command prompt seen by the user.
2. `JPL I (EDIT` — runs the line editor on the command line (`src/TSS5.SYMB:1845`).
   `EDIT` is the line-editing monitor routine (see §2 and §4), not a text editor. [VERIFIED]
3. `JPL I (GCMD; JMP ERR` — `GCMD` ("GET COMMAND", defined `src/TSS2.SYMB:2052`)
   trims the line and extracts the first token up to a `,` or space
   (`src/TSS2.SYMB:2052-2064`). [VERIFIED]
4. `JPL I (RCMD` — loads the command-name string array (`src/TSS5.SYMB:1848`;
   `RCMD`→`FRCMD`, `src/TSS3.SYMB:3523` / `src/TSS5.SYMB:3523`, body at
   `src/TSS5.SYMB:1774`). [VERIFIED]
5. `LDX CPTR,B; LDT (STARR; SAA 1; COPY DD SA; LDA (SCMD; JPL I (ABLKP`
   — calls the **abbreviation lookup** `ABLKP` against the `SCMD` string array
   (`src/TSS5.SYMB:1848-1849`). [VERIFIED]
6. `JMP CERR; ADD (CMD; COPY DX SA; … LDX 0,X; JPL 0,X; JMP ERR`
   — on a match, index `A` is added to the `CMD` routine-pointer table and the
   command routine is called indirectly (`src/TSS5.SYMB:1850-1851`). [VERIFIED]

Error paths: `ERR` (`src/TSS5.SYMB:1856`) emits `?` and re-prompts;
`CERR` (`src/TSS5.SYMB:1857`) distinguishes `ABLKP` return `-1` (no match) from
`-2` (ambiguous), the latter giving error message 53
(`SAA 53; JPL I (ERMSG`, `src/TSS5.SYMB:1858`). [VERIFIED]

### 1.2 The abbreviation matcher `ABLKP`

`ABLKP` ("ABBREVIATION LOOKUP ROUTINE") is defined at `src/TSS3.SYMB:715`
(header comment `src/TSS3.SYMB:715-726`, body `A1` at `src/TSS3.SYMB:748`). Its
contract, quoted from the source header: input `X` = given string, `T` = string-array
accessor function, `A` = pointer to the string array, `D` = start index; it returns
`A` = index of the matching entry, or `A=-1` (no match) / `A=-2` (ambiguous, with
`T` = first ambiguous entry). [VERIFIED, `src/TSS3.SYMB:717-726`]

It also matches **hyphen-separated parts** via the helper `GPART` ("RETURN THE
ITH PART OF THE FIRST STRING IN THE SECOND STRING", `src/TSS3.SYMB:783-789`) and
`SBSTR`, so a user can abbreviate each hyphenated segment of a command name.
[VERIFIED, `src/TSS3.SYMB:762-772`]
`[ASSUMPTION]` This is why the command names in §1.4 are hyphenated
(`LOAD-BINARY`, `WHO-IS-ON`): each part is independently abbreviable.

`ABLKP` is also a **user-callable monitor call** (it appears in `MCTBL`, §2), and a
second matcher `ABLKU` is exposed too (`src/TSS1.SYMB:4306,4311`). [VERIFIED]

### 1.3 The two parallel tables

The dispatch uses **two arrays in the same order**:

- `SCMD` — the command-**name** string array. `SCMD=QBUF` (`src/TSS2.SYMB:217`,
  "STRING ARRAY FOR COMMANDS"); it is filled at runtime by `FRCMD` copying the
  read-only `XCMD` template (`src/TSS5.SYMB:1776-1778`). The literal names live in
  `SC1…SC61` at `src/TSS5.SYMB:1643-1702`, and `XCMD`/`XC1…XC61` at
  `src/TSS5.SYMB:1632-1765`. Count `NCMD=*-XCMD-1` (`src/TSS5.SYMB:1641`). [VERIFIED]
- `CMD` — the command-**routine** pointer table, first word `NCMD` then the routine
  labels, at `src/TSS5.SYMB:1791-1801`. [VERIFIED]

The two lists are positionally aligned. Note **`SC31` is intentionally absent**:
the name list jumps `SC30` → `SC32` (`src/TSS5.SYMB:1672-1673`) and `XCMD` likewise
skips `XC31` (`src/TSS5.SYMB:1636`), so there are **60 live commands**, not 61.
[VERIFIED] `NOT DETERMINED FROM SOURCE`: why slot 31 was retired.

### 1.4 Command table — every built-in command

Pairing `SC1…SC61` (`src/TSS5.SYMB:1643-1702`) with the `CMD` routine list in
order (`src/TSS5.SYMB:1791-1801`). "What it does" is the command name plus, where I
read the routine, a source-grounded note; otherwise it is `[ASSUMPTION]` from the
name. Routine source lines are given where I located a `GOVER …,<routine>` or
`<routine>,` definition.

| # | Command (typed) | Routine | Name def | Routine def | What it does |
|---|---|---|---|---|---|
| 1  | `RESET`             | `RESET`  | TSS5:1643 | `src/TSS2.SYMB:433` | [VERIFIED] Release the user's address space (marks all 8 pages, `src/TSS2.SYMB:427-446`) |
| 2  | `RECOVER`           | `RECV`   | TSS5:1644 | — | [ASSUMPTION] Recover/resume a session (also invoked at `CERR1`, `src/TSS5.SYMB:1860`) |
| 3  | `DUMP`              | `DUMP`   | TSS5:1645 | — | [ASSUMPTION] Dump memory |
| 4  | `GOTO-USER`         | `GOTO`   | TSS5:1646 | `src/TSS2.SYMB:387` | [VERIFIED] Start execution of the loaded user program (`src/TSS2.SYMB:380-409`) |
| 5  | `LOAD-BINARY`       | `LOAD`   | TSS5:1647 | `src/TSS2.SYMB:418` | [VERIFIED] `PLACE` then `GOTO` — load a binary and run it (`src/TSS2.SYMB:412-424`) |
| 6  | `PLACE-BINARY`      | `PLACE`  | TSS5:1648 | `src/TSS3.SYMB:1508` | [VERIFIED] Load a binary into core without starting it (the loader, §3) |
| 7  | `MEMORY`            | `MEM`    | TSS5:1649 | `src/TSS3.SYMB:1596` | [VERIFIED] Set the size of the address space (`src/TSS3.SYMB:1590-1596`) |
| 8  | `LOGOUT`            | `QUIT`   | TSS5:1650 | `src/TSS4.SYMB:1071` | [VERIFIED] Log the user out (`GOVER OV8,Q1,QUIT`) |
| 9  | `HELP`              | `HELP`   | TSS5:1651 | `src/TSS3.SYMB:1044` | [VERIFIED] Help command (`GOVER OV1B,H1,HELP`) |
| 10 | `OPEN-FILE`         | `OPFC`   | TSS5:1652 | — | [ASSUMPTION] Open a file (command wrapper over the `OPEN` monitor call) |
| 11 | `CLOSE-FILE`        | `CLFC`   | TSS5:1653 | — | [ASSUMPTION] Close a file (wrapper over `CFILE`) |
| 12 | `LIST-FILE`         | `LISTF`  | TSS5:1654 | — | [ASSUMPTION] List a file's contents |
| 13 | `DELETE-FILE`       | `DELFC`  | TSS5:1655 | — | [ASSUMPTION] Delete a file |
| 14 | `RENAME`            | `RENAM`  | TSS5:1656 | — | [ASSUMPTION] Rename a file |
| 15 | `STATUS`            | `REGS`   | TSS5:1657 | — | [ASSUMPTION] Show status / registers (routine name `REGS`) |
| 16 | `SET-REGISTER`      | `SETX`   | TSS5:1658 | — | [ASSUMPTION] Set a register of the user program |
| 17 | `EXAMINE`           | `EXAM`   | TSS5:1659 | — | [ASSUMPTION] Examine memory |
| 18 | `RESERVE`           | `RESRV`  | TSS5:1660 | — | [ASSUMPTION] Reserve a device |
| 19 | `RELEASE`           | `RELSE`  | TSS5:1661 | — | [ASSUMPTION] Release a device |
| 20 | `WHERE-IS`          | `WHERE`  | TSS5:1662 | — | [ASSUMPTION] Locate a user |
| 21 | `CLOCK-ON`          | `CLON`   | TSS5:1663 | — | [ASSUMPTION] Turn on CPU-time accounting/clock |
| 22 | `CLOCK-OFF`         | `CLOFF`  | TSS5:1664 | — | [ASSUMPTION] Turn off the clock |
| 23 | `LINK-TO`           | `LINKS`  | TSS5:1665 | — | [ASSUMPTION] Link terminals for chat (uses `BROAD`/`PMSG`, see §5) |
| 24 | `BREAK-LINKS`       | `BRKLK`  | TSS5:1666 | — | [ASSUMPTION] Break terminal links |
| 25 | `CREATE-USER`       | `CRUSR`  | TSS5:1667 | — | [ASSUMPTION] Create a user account (privileged) |
| 26 | `PASSWORD`          | `PASWD`  | TSS5:1668 | — | [ASSUMPTION] Set your password |
| 27 | `WHO-IS-ON`         | `WHOS`   | TSS5:1669 | — | [ASSUMPTION] List logged-in users (uses `WHUSR`) |
| 28 | `INIT-ACCOUNTING`   | `IACCT`  | TSS5:1670 | — | [ASSUMPTION] Initialise the accounting file |
| 29 | `LIST-ACCOUNTS`     | `PACCT`  | TSS5:1671 | — | [ASSUMPTION] Print accounting records |
| 30 | `LIST-USERS`        | `LUSR`   | TSS5:1672 | — | [ASSUMPTION] List the user table |
| 31 | `TIME-USED`         | `TUSED`  | TSS5:1673 | — | [ASSUMPTION] Show CPU/connect time used (name slot 32) |
| 32 | `PAUSE`             | `PAUSE`  | TSS5:1674 | — | [ASSUMPTION] Pause the user program |
| 33 | `CONTINUE`          | `CONT`   | TSS5:1675 | — | [ASSUMPTION] Continue a paused program |
| 34 | `DISK-SPACE`        | `DKSP`   | TSS5:1676 | — | [ASSUMPTION] Report free disk space |
| 35 | `ALLOCATE`          | `ALKAT`  | TSS5:1677 | — | [ASSUMPTION] Allocate disk space |
| 36 | `LIST-OBJECTS`      | `LOBJ`   | TSS5:1678 | — | [ASSUMPTION] List objects/files owned |
| 37 | `CREATE-FRIEND`     | `CRFRC`  | TSS5:1679 | — | [ASSUMPTION] Grant another user "friend" access |
| 38 | `DELETE-FRIEND`     | `DLFRC`  | TSS5:1680 | — | [ASSUMPTION] Remove a friend |
| 39 | `LIST-FRIENDS`      | `LSFRC`  | TSS5:1681 | — | [ASSUMPTION] List friends |
| 40 | `DEFINE-UIB-ACCESS` | `SUIBC`  | TSS5:1682 | — | [ASSUMPTION] Set user-information-block access |
| 41 | `DEFINE-FILE-ACCESS`| `SFILC`  | TSS5:1683 | — | [ASSUMPTION] Set file access permissions |
| 42 | `MODE`              | `CMOD`   | TSS5:1684 | — | [ASSUMPTION] Set a mode |
| 43 | `MAKE-REENTRANT`    | `MRENT`  | TSS5:1685 | — | [ASSUMPTION] Mark a program reentrant/shared |
| 44 | `DELETE-MEMORY`     | `DLMEM`  | TSS5:1686 | — | [ASSUMPTION] Delete a memory segment |
| 45 | `CLEAR-PASSWORD`    | `CPASW`  | TSS5:1687 | — | [ASSUMPTION] Clear a password (privileged) |
| 46 | `RESPONSE-TIME`     | `RSPT`   | TSS5:1688 | `src/TSS5.SYMB:1952` | [VERIFIED] Print average response time (`GOVER OV19,R1,RSPT`, `src/TSS5.SYMB:1946-1966`) |
| 47 | `DELETE-USER`       | `DELUS`  | TSS5:1689 | — | [ASSUMPTION] Delete a user account (privileged) |
| 48 | `SAVE-CORE`         | `SAVE`   | TSS5:1690 | — | [ASSUMPTION] Save core image to a file |
| 49 | `GET-CORE`          | `GET`    | TSS5:1691 | — | [ASSUMPTION] Load a saved core image |
| 50 | `PIN-DEVICE`        | `PINC`   | TSS5:1692 | — | [ASSUMPTION] Pin/attach a device |
| 51 | `DEFINE-DATE`       | `DDATE`  | TSS5:1693 | `src/TSS5.SYMB:1872` | [VERIFIED] Set the system date/time (`GOVER OV19,D1,DDATE`, `src/TSS5.SYMB:1866-1888`) |
| 52 | `DATE`              | `CDATE`  | TSS5:1694 | `src/TSS5.SYMB:1897` | [VERIFIED] Print date and time (`GOVER OV19,C1,CDATE`, `src/TSS5.SYMB:1891-1905`) |
| 53 | `RBLOAD`            | `RBLOAD` | TSS5:1695 | — | [ASSUMPTION] Reentrant/bootstrap load (`RBLOAD` is also a monitor call, `src/TSS1.SYMB:4314`) |
| 54 | `TRANSFER`          | `TRTRK`  | TSS5:1696 | — | [ASSUMPTION] Transfer tracks (routine `TRTRK` = "transfer track") |
| 55 | `LIST-TRACKS`       | `LTRKS`  | TSS5:1697 | `src/TSS5.SYMB:1622` | [VERIFIED] List tracks (`)PCL LTRKS`, `src/TSS5.SYMB:1622`) |
| 56 | `BACKUP`            | `BAKUP`  | TSS5:1698 | — | [ASSUMPTION] Disk backup |
| 57 | `DEFINE-VERSION`    | `DEVER`  | TSS5:1699 | — | [ASSUMPTION] Set a version/edition string |
| 58 | `LOAD-SYSTEM`       | `LOADV`  | TSS5:1700 | `src/TSS5.SYMB:609` | [VERIFIED] Load a system overlay/version (`GOVER OV12,L1,LOADV`) |
| 59 | `SYSUP`             | `SYSUP`  | TSS5:1701 | — | [ASSUMPTION] Bring the system up |
| 60 | `SYSDOWN`           | `SYSDW`  | TSS5:1702 | — | [ASSUMPTION] Bring the system down |

`[ASSUMPTION]` A block of these (CREATE-USER, DELETE-USER, CLEAR-PASSWORD,
INIT-ACCOUNTING, SYSUP, SYSDOWN, LOAD-SYSTEM) are operator/privileged commands, but
the source does **not** annotate a privilege bit at the table, so which commands a
non-privileged user may run is `NOT DETERMINED FROM SOURCE` from the table alone.

---

## 2. Monitor-call interface for user programs

### 2.1 The transfer vector `MCTBL`

User programs call the monitor through a numbered transfer vector `MCTBL`
("MONITOR CALL TRANSFER VECTOR", `src/TSS1.SYMB:4300`), listed at
`src/TSS1.SYMB:4302-4318`, with size `MCSIZ=*-MCTBL` (`src/TSS1.SYMB:4319`). Each
call number is the routine's **position** in this vector (0-based). [VERIFIED]

| # (oct) | Name | # | Name | # | Name |
|---|---|---|---|---|---|
| 0  | `LEAVE`  | 1  | `INBT`  | 2  | `OUTBT` |
| 3  | `ECHOM`  | 4  | `BRKM`  | 5  | `RDISK` |
| 6  | `WDISK`  | 7  | `RPAG`  | 10 | `WPAG`  |
| 11 | `RCTIM`  | 12 | `EDIT`  | 13 | `CIBUF` |
| 14 | `COBUF`  | 15 | `SETUP` | 16 | `GCI`   |
| 17 | `GCD`    | 20 | `WCI`   | 21 | `WCD`   |
| 22 | `GC`     | 23 | `LEN`   | 24 | `SETR`  |
| 25 | `SETW`   | 26 | `ABLKP` | 27 | `STEQL` |
| 30 | `STARR`  | 31 | `GCMD`  | 32 | `MSG`   |
| 33 | `DIV`    | 34 | `NORM`  | 35 | `IOUT`  |
| 36 | `CNS`    | 37 | `CSN`   | 40 | `TRIM`  |
| 41 | `BCOPY`  | 42 | `OPEN`  | 43 | `CFILE` |
| 44 | `GCLIN`  | 45 | `DBRK`  | 46 | `GBRK`  |
| 47 | `SBRK`   | 50 | `OPIII` | 51 | `BROAD` |
| 52 | `GUNUM`  | 53 | `GUNAM` | 54 | `PMSG`  |
| 55 | `IMAIL`  | 56 | `GMAIL` | 57 | `WHUSR` |
| 60 | `ABLKU`  | 61 | `GCMD`  | 62 | `STARR` |
| 63 | `SOUT`   | 64 | `ERMSG` | 65 | `QERMS` |
| 66 | `ISIZE`  | 67 | `OSIZE` | 70 | `EXEC`  |
| 71 | `UGNXT`  | 72 | `UDLTR` | 73 | `XDRD`  |
| 74 | `SSTAT`  | 75 | `RSTAT` | 76 | `SETBS` |
| 77 | `RWACM`  |100 | `TTIM`  |101 | `RKLOK` |
|102 | `MDRIV`  |103 | `RDATE` |104 | `PDATE` |
|105 | `IOCLL`  |106 | `ESCTR` |107 | `RESUM` |
|110 | `RMAXB`  |111 | `SMAXB` |112 | `LF`    |
|113 | `MO113`  |114 | `REABT` |115 | `SETBT` |
|116 | `SFLOP`  |117 | `ZFLOP` |120 | `TMSFL` |

(Numbering is my positional count over `src/TSS1.SYMB:4302-4318`. [VERIFIED] list,
[ASSUMPTION] the octal indices.) Some slots are duplicated on purpose: `GCMD`,
`STARR` appear twice (numbers 31/61 and 30/62). [VERIFIED, `src/TSS1.SYMB:4306,4311`]

### 2.2 The call mechanism `LEV3`

The dispatcher is `LEV3` ("MONITOR CALL PROCESSOR", `src/TSS1.SYMB:4322-4323`,
body `src/TSS1.SYMB:4326`). On NORD-10 (`"N10`) it reads the calling instruction,
`AND (377` to isolate the call number (`src/TSS1.SYMB:4328`), bounds-checks against
`MCSIZ` (`src/TSS1.SYMB:4330`), indexes `MCTBL`, and if the entry is non-zero
`JPL 0,B` calls it (`src/TSS1.SYMB:4331-4338`). User registers are shuttled through
the swapped register block `RBLOK`/`RBLK1`/`RBLK2` (`src/TSS1.SYMB:4333-4344,4362-4364`).
An unknown call number falls to `LF` which raises a trap
(`LDA (TRAP; STA I (UBLOK … MCL/MST PID`, `src/TSS1.SYMB:4357-4359`). [VERIFIED]

**Calling convention** `[VERIFIED]` from the routines: arguments and results are
passed in the CPU registers `A/D/T/X` (the ND accumulator, double, T and X
registers). Examples: `LEN` takes `X`=string pointer and returns length in `D`;
`OPEN` takes `X`=name string, `A`=file mode, `T`=type and returns `A`=file number
(`src/TSS3.SYMB:1537-1538`); `PMSG` takes `A`=user number, `X`=message address
(`src/TSS4.SYMB:263-267`). Routines return via `EXIT`/`RET` which restores the
user register block. `[ASSUMPTION]` there is no stack-based argument passing; it is
register-based throughout.

### 2.3 User-facing calls (grouping)

Grouped by purpose; names as in §2.1. [VERIFIED names; ASSUMPTION grouping]

- **Program control / exit:** `LEAVE` (#0) returns to the command processor
  (`LDA (START; … WAIT; JMP LEV3`, `src/TSS1.SYMB:4369-4381`); `EXEC` sets exec
  mode; `ESCTR`/`RESUM` set an escape trap and resume (`src/TSS1.SYMB:4384-4403`).
- **Terminal I/O:** `INBT`/`OUTBT` (byte in/out), `ECHOM`/`BRKM` (echo/break mode),
  `GCI`/`GCD`/`WCI`/`WCD`/`GC` (get/put chars), `MSG` (write a message), `GCLIN`
  (get command line), `IOUT`/`CNS`/`CSN` (number ⇄ string conversion),
  `SOUT`, `ERMSG`/`QERMS` (error messages).
- **File I/O:** `OPEN` (#42), `CFILE` (#43 close), `RDISK`/`WDISK` (raw disk),
  `RPAG`/`WPAG` (page I/O), `ISIZE`/`OSIZE`, `XDRD`.
- **String utilities exposed as calls:** `LEN`, `SETR`/`SETW` (set read/write
  pointer), `SETUP`, `BCOPY`, `TRIM`, `STEQL`, `STARR`, `ABLKP`/`ABLKU`, `GCMD`,
  `DIV`/`NORM` (arithmetic helpers).
- **Inter-user / mail:** `BROAD` (broadcast), `PMSG` (put message), `IMAIL`/`GMAIL`
  (mailbox init/get), `GUNUM`/`GUNAM` (user number/name), `WHUSR` (who am I).
- **Time / date / accounting:** `RCTIM`, `RKLOK`, `TTIM`, `RDATE`/`PDATE`,
  `RWACM` (read/write accounting), `RMAXB`/`SMAXB`.
- **Break / interrupt handling:** `DBRK`/`GBRK`/`SBRK`, `SETBS`/`SETBT`, `REABT`.
- **Device / floppy:** `SFLOP`/`ZFLOP` (set/reset floppy flag,
  `src/TSS1.SYMB:4407-4416`), `TMSFL`, `MDRIV`, `IOCLL`.

---

## 3. Building and running programs under TSS

### 3.1 There is a loader — `PLACE`

Yes. `PLACE` ("LOAD BINARY PROGRAM", `src/TSS3.SYMB:1501-1504`, body `P1` at
`src/TSS3.SYMB:1534`) is the binary loader. It:

1. Opens the named file with mode `MTP1` (`LDA (MTP1; … JPL I (OPEN`,
   `src/TSS3.SYMB:1537`). [VERIFIED]
2. Scans for a start marker `12` then `15` (octal), skipping/echoing the header,
   and reads an 8-word checksum via `CSN` (`src/TSS3.SYMB:1540-1547`). [VERIFIED]
3. On the marker word `!` (`SAT ##!`, `src/TSS3.SYMB:1559`), reads the load block:
   two bytes → **first core location** `CORL` (`SHA 10; …; ADD`,
   `src/TSS3.SYMB:1560-1561`), two bytes → **cell count** `CORS`
   (`src/TSS3.SYMB:1562-1563`), bounds-checks against `MSTRT` and via `CKMEM`
   (`src/TSS3.SYMB:1564-1567`). [VERIFIED]
4. Reads `CORS` data words (each two bytes, big-endian `SHA 10; ADD`), storing to
   `I CLOC,B` and accumulating a checksum `CSUM` (`src/TSS3.SYMB:1571-1573`). [VERIFIED]
5. Reads a given checksum `GCSUM`, compares (`src/TSS3.SYMB:1574-1577`); mismatch →
   "CHECKSUM ERROR" (`MS2`, `src/TSS3.SYMB:1532,1583`); out-of-range →
   "PROGRAM OUT OF BOUNDS" (`MS1`, `src/TSS3.SYMB:1531,1582`). [VERIFIED]

**Object format** `[VERIFIED, src/TSS3.SYMB:1557-1577]`: a byte-stream binary with a
header, a `!`-introduced load block carrying a big-endian *load address* and *word
count*, the data words, and a trailing checksum. `[ASSUMPTION]` this is the ND
"BPUN" (binary punch) format family described in `docs/MAC-BPUN-Analysis.md`
(same big-endian 2-byte packing); I did not byte-match the two formats here.

### 3.2 Running: `PLACE`, `GOTO-USER`, `LOAD-BINARY`

- `LOAD-BINARY` (`LOAD`, `src/TSS2.SYMB:418`) is literally `PLACE` **then** `GOTOX`:
  `JPL I (PLACE; JMP LSSX; JPL I (GOTOX; JMP LF` (`src/TSS2.SYMB:418-420`). So
  `LOAD-BINARY` = load and run. [VERIFIED]
- `PLACE-BINARY` (`PLACE`) loads without starting. [VERIFIED, §3.1]
- `GOTO-USER` (`GOTO`, `src/TSS2.SYMB:387`) starts execution: it takes the start
  address from the command line (`PCF`), sets the paging registers via `SMPR`,
  sets the process to run (`SAA 2; MST PID`) and switches in
  (`src/TSS2.SYMB:387-405`). It first validates the address with `CKMEM`
  (`src/TSS2.SYMB:396`). [VERIFIED]

### 3.3 The user address space

User programs run in an **8-page** space: `NPGS=10` octal = 8 (`src/TSS1.SYMB:4659`),
based at `MSTRT=40000` octal (`src/TSS1.SYMB:4649,4651`). `RESET` walks all `NPGS`
page-table slots (`src/TSS2.SYMB:439-443`), and `SMPR` ("SET MPR REGISTER") programs
the memory-protect/paging registers for those pages before `GOTO`
(`src/TSS1.SYMB:4494-4513` for the N10 variant). [VERIFIED] This matches the
"8-page space" in the task brief.

`MEMORY` (`MEM`, `src/TSS3.SYMB:1596`) lets the user set how many of those pages are
mapped. [VERIFIED name/routine; detailed behaviour partially read]

`SAVE-CORE`/`GET-CORE` (`SAVE`/`GET`, table rows 48-49) `[ASSUMPTION]` save and
reload a user core image to/from a file — the complement to `PLACE`. Routine bodies
`NOT DETERMINED FROM SOURCE` in this pass.

---

## 4. Editors, compilers, language tools — a finding

**No text editor, assembler, compiler, or other language tool ships in, or is
invoked from, this TSS source.** [VERIFIED by absence]

Evidence:

- The complete 60-entry command table (`src/TSS5.SYMB:1643-1702`) contains **no**
  `EDIT`, `ASSEMBLE`, `COMPILE`, `FORTRAN`, `BASIC`, `MAC`, or similar command. The
  commands are exclusively session control, file management, user/account
  administration, device and system operation. [VERIFIED]
- The `EDIT` symbol that exists (`GOVER OV1C,E1,EDIT`, `src/TSS3.SYMB:1075`; monitor
  call #12) is a **command-line editor**: its header is "EDIT … FRETURN IF LINE
  ENDS WITH &L" and it works on the "OLD LINE"/"NEW OLD LINE" command-line strings
  (`src/TSS3.SYMB:1070-1098`); the command loop calls it to let the user recall and
  edit the previous command line (`src/TSS5.SYMB:1845`). It is **not** a file/text
  editor. [VERIFIED]
- A grep for `EDITOR|COMPIL|ASSEMBL|MACRO` across `src/*.SYMB` returns only
  MAC-assembler *directives/comments* (e.g. `%…(MACRO)` overlay macros,
  "ASSEMBLY WITH MACF"), i.e. the language this OS is *written in*, not tools it
  *provides*. [VERIFIED, `src/TSS3.SYMB:10,53,65`; `src/TSS1.SYMB:49`]

`[ASSUMPTION]` Program development happened elsewhere (the ND MAC assembler /
BPUN toolchain — see `docs/MAC-BPUN-Analysis.md`), and finished binaries were
brought in and run via `PLACE-BINARY` / `LOAD-BINARY`. The source does not show a
resident toolchain. Whether any tool was loaded *as a user program* at a site is
`NOT DETERMINED FROM SOURCE`.

---

## 5. Mail and other user-facing subsystems

### 5.1 Mail (`MAILD` mailbox)

Mail is a per-user on-disk mailbox area `MAILD` (`MAILD=ACCTD+200`,
`src/TSS1.SYMB:2606`, comment "MAILBOX"; size `MAILS`, used at
`src/TSS4.SYMB:288,325`). It is manipulated by four routines in `OV4A`:

- `IMAIL` — "INITIALIZE MAILBOX" (monitor #55), privileged: it checks `WHUSR` and
  rejects with "YOU MAY NOT DO THIS" otherwise (`src/TSS4.SYMB:241-260`). [VERIFIED]
- `PMSG` — "PUT A MESSAGE INTO THE USER'S MAILBOX", `A`=user number, `X`=message
  address; walks up to `MAILS` slots, fails if the mailbox is full
  (`src/TSS4.SYMB:263-296`). [VERIFIED]
- `GMAIL` — "GET USER'S MAIL" (monitor #56), `X`=buffer address; copies queued
  messages out and clears them (`src/TSS4.SYMB:299-330`). [VERIFIED]
- `CMAIL` — "DOES THE USER HAVE MAIL?" — success/failure return
  (`src/TSS4.SYMB:333-353`). [VERIFIED] (Internal helper; not in `MCTBL`.)

Mailbox access is serialized with a lock word `MAILK`/`KMAIL`
(`src/TSS4.SYMB:253-254,277-278`). [VERIFIED]

`[ASSUMPTION]` There is no separate "MAIL" *command* in the table; mail is delivered
through these monitor calls, and the user-facing entry is `[ASSUMPTION]` the
`LINK-TO` / broadcast facilities and any program that calls `GMAIL`. A dedicated
`MAILD`-named *program* is `NOT DETERMINED FROM SOURCE` — `MAILD` here is the data
region, not a program.

### 5.2 Inter-user messaging

- `BROAD` (monitor #51) — broadcast a message. [VERIFIED name, `src/TSS1.SYMB:4309`]
- `LINK-TO` / `BREAK-LINKS` commands (`LINKS`/`BRKLK`, rows 23-24) `[ASSUMPTION]`
  connect two terminals for a live chat, using `PMSG`/`BROAD`. Routine bodies not
  read in this pass.
- `WHO-IS-ON` (`WHOS`, row 27) and the `WHUSR`/`GUNAM`/`GUNUM` calls give the user
  directory. [VERIFIED names]

### 5.3 Accounting

`INIT-ACCOUNTING` / `LIST-ACCOUNTS` / `TIME-USED` commands (rows 28,29,31) plus the
`RWACM`, `TTIM`, `RCTIM` monitor calls and the `ACCTD` accounting region
(`src/TSS1.SYMB:2606`, `MAILD=ACCTD+200`) form the accounting subsystem. [VERIFIED
names/region]; detailed behaviour `NOT DETERMINED FROM SOURCE` in this pass.

---

## 6. Cross-references

- Login / session bring-up: `docs/LOGON-PATH.md`, `docs/DISK-INIT-USERS-AND-BOOT.md`.
- Overlay mechanism (`GOVER`/`OVERL`/`OVERX`, how these routines page in):
  `docs/OVERLAY-DISC-SPEC.md`.
- Object/binary format background: `docs/MAC-BPUN-Analysis.md`.
- Overall architecture: `docs/TSS-Analysis.md`, `docs/PROJECT-DESCRIPTION.md`.

---

## 7. Open questions (not determined from this source)

1. Privilege model: which of the 60 commands require operator/system privilege is
   not annotated at the command table. `NOT DETERMINED FROM SOURCE`.
2. Why command slot 31 (`SC31`/`XC31`) was retired. `NOT DETERMINED FROM SOURCE`.
3. Exact byte-level identity between `PLACE`'s object format and ND BPUN.
   `[ASSUMPTION]` they are the same family; not byte-matched here.
4. Bodies of the many admin commands (rows 10-45, 47-50, 54, 56-60) were not each
   read; their "what it does" is `[ASSUMPTION]` from the command name.
