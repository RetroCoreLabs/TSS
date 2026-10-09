# NORD TSS 3.0 — User Manual (Command Reference)

*Reconstructed from the recovered TSS source (`src/TSS1–TSS5.SYMB`). Every command
below is documented from its actual handler routine and the structured pseudo-code
comments beside the code — not from the command name or from any other OS.
Anything not directly stated in the source is marked **(inferred)**.*

The dispatch table that defines these commands is `CMD` in `src/TSS5.SYMB` (near
line 1791); the names shown by `@HELP` come from the parallel `SCn` string table.

---

## 1. What TSS is

NORD TSS 3.0 is a standalone timesharing operating system for the Norsk Data
NORD-1 / NORD-10 (Bo Lewendal, 1973). It owns the machine. Every teletype is a
process with its own 8-page virtual memory, swapped between core and disk. After
you log in you talk to the **command processor**, whose prompt is `@`.

The original manual by the author, *Reference Manual for the NORD
Timesharing System* (ND-60.039.01), is in this folder as
[`ND-60.039.01_Reference_Manual_for_the_NORD_Timesharing_System_16_February_1973_ocr.pdf`](ND-60.039.01_Reference_Manual_for_the_NORD_Timesharing_System_16_February_1973_ocr.pdf).
This manual describes the system as it was rebuilt and tested; where the two
differ, this one records what the running system does.

## 2. Getting to the `@` prompt (logging in)

The console (TTY1) prints its sign-on at boot. Every other teletype is
**dead** until **ESC** (033 octal) is typed on it - the level-6 scanner
(`src/TSS1.SYMB` `LEV6`) reads one character per scan from a dead line and
starts the process only for ESC (`AND (177; SUB (33; JAF L2`); anything else
is dropped. **[VERIFIED]** on nd100x TERMINAL 8 over telnet.
The shipped build has 10 teletypes (`TEL10`): on nd100x the console and
TERMINAL 5-10 (= TSS TTY5-TTY10) work; TTY2-TTY4 do not (nd100x's terminals
at IOX 0310-0330 answer idents 0121-0123, not the 5-7 TSS dispatches on).

Each terminal prints its sign-on and waits:

```
NORD TSS VERSION 3.0A IS UP
@ENTER <user name>
PROJECT NUMBER P-<n>
```

- **`@ENTER`** — type your user name (e.g. `SYSTEM`). If the account has a
  password you are then prompted `PASSWORD`.
- **`PROJECT NUMBER P-`** — type a **positive** project number (used for
  accounting). `0`/blank re-prompts.
- On a freshly formatted disc the first login prints **`AMBIGUOUS FILENAME`**
  right after the project number. This is **harmless**: `LOGON` tries to open your
  per-user scratch file `SCRATCH:DATA`, which doesn't exist yet, so the open fails
  with error 31 and login simply continues. See **§5 Scratch files** for what this
  file is, what it's for, and how it's created.
- **First login only:** `TYPE IN DATE (DD,MM,YYYY,HH,MM,SS):` — set the clock,
  then you reach `@`.

## 3. Using the command processor

- At the **`@`** prompt, type a command name and any arguments on one line, then
  ⏎. Most commands take their arguments on the command line; a few prompt you
  (those are noted below).
- **Abbreviation:** commands are matched against the command table by a block
  search, so a **unique prefix** is enough. If a prefix matches more than one
  command the match is ambiguous and rejected.
- **Unknown command:** the processor prints **`?`**. **[VERIFIED]**
  `?` is the general **failure-return** indicator, not only "unknown command":
  it also follows defined errors such as `ILLEGAL ADDRESS` (`GOTO-USER`) and
  `PROGRAM OUT OF BOUNDS` (`RBLOAD`), and appears alone from `CONTINUE` and
  a refused `LINK-TO`.
- **Line editing / recall:** the previous command line is retained and can be
  recalled/edited (the `EDIT` routine, `src/TSS5.SYMB` command loop `CLOOP`).
- Commands marked **SYSTEM only** below check the caller's user number and refuse
  (error `YOU MAY NOT DO THIS`) for anyone but user 1 (`SYSTEM`).

---

## 4. Command reference

**How arguments work.** Most commands take their arguments on the same line,
separated by spaces or commas (e.g. `OPEN-FILE MYFILE,RW`). If you leave a
required argument out, TSS **prompts** you for it from the fixed table
`PARPT` / `SC1`–`SC36` (`src/TSS4.SYMB:1690-1731`). The type prefix in that
table governs the radix: **`S[]` = string, `IB[]` = octal, `ID[]` = decimal**
— which is why addresses are octal but dates are decimal.

> **[VERIFIED]** some commands **ignore a command-line argument
> and prompt anyway** — `DELETE-MEMORY 40000` and `PLACE-BINARY TAPE-READER`
> both re-prompted. When in doubt, issue the bare command and answer the
> prompts. Numeric addresses/values are **octal**;
dates are **decimal**. Where noted, a parenthesised `(USER)` before a file or
friend name targets another user's object. **SYSTEM only** means the command
checks the caller's user number and refuses anyone but user 1 (`SYSTEM`) with
`YOU MAY NOT DO THIS`.

### 4.1 Help & session control

#### HELP
Lists all available command names (one per line). No arguments.
`src/TSS3.SYMB:1038`

#### LOGOUT
Logs you out. Reports `TIME USED IS <t> OUT OF <total>`, closes and deletes your
temporary files/tracks, releases your address space, and idles the terminal. If
you have unread mail it first prints `****YOU HAVE MAIL****` and returns so you
can read it before logging out again. No arguments. `src/TSS4.SYMB:1065`

#### PAUSE
Locks the terminal in a paused state; prompts `PASSWORD IS` and will not resume
until you re-enter your password correctly (wrong entry silently re-prompts).
No arguments. `src/TSS4.SYMB:782`

#### CONTINUE
Resumes the currently suspended program at its saved restart address.
**[VERIFIED]** with nothing to continue it prints **`?`** (the
failure-return indicator), not silence. No arguments. `src/TSS2.SYMB:1668`

#### MODE
Redirects where the command processor reads commands and writes results — opens
an **INPUT FILE** and an **OUTPUT FILE** and installs them as the command
input/output devices (this is how batch/command files are run). Args: input file,
output file. `src/TSS3.SYMB:398`

#### DATE
Prints `DATE IS <current date and time>`. No arguments. `src/TSS5.SYMB:1897`

### 4.2 Files

#### OPEN-FILE
Opens a file and prints `FILE NUMBER = <n>` (use that number with CLOSE-FILE).
Syntax: `OPEN-FILE FILENAME,(R/W)[X]` — the mode field accepts `R`, `W`, and an
optional `X`. `src/TSS3.SYMB:1635`

#### CLOSE-FILE
Closes an open file. Syntax: `CLOSE-FILE FILE-NUMBER`. `src/TSS3.SYMB:1476`

#### LIST-FILE
Prints a file's directory/object information. Syntax: `LIST-FILE [(USER)]
FILENAME` — an optional `(USER)` lists another user's file. Prints `NO SUCH USER`
if that user can't be resolved. `src/TSS3.SYMB:1675`

#### DELETE-FILE
Deletes a file. Syntax: `DELETE-FILE [(USER)] FILENAME`. Prints `AMBIGUOUS` if the
name matches more than one file. `src/TSS4.SYMB:1248`

#### RENAME
Renames a file. Syntax: `RENAME [(USER)] OLD-NAME NEW-NAME`. `src/TSS4.SYMB:1836`

#### LIST-OBJECTS
Lists the object-file table: for each entry, the entry number, owner name, file
name, and its attribute words. No arguments. `src/TSS4.SYMB:1373`

#### ALLOCATE  *(SYSTEM only)*
Allocates an absolute (fixed-location) file over specific disk tracks. Syntax:
`ALLOCATE FILENAME,TRACK-ADDRESS,NUMBER-OF-TRACKS` (octal). `src/TSS4.SYMB:1337`

### 4.3 Programs & memory (load, run, debug)

#### PLACE-BINARY
Loads a binary (BPUN-format) program file into memory **without** starting it, and
returns its start address. Verifies the load range and checksum. Errors:
`PROGRAM OUT OF BOUNDS`, `CHECKSUM ERROR`. Arg: file name. `src/TSS3.SYMB:1501`

#### LOAD-BINARY
Same as PLACE-BINARY, then **starts execution** at the loaded program's start
address. Arg: file name. `src/TSS2.SYMB:412`

#### RBLOAD
Loads an **RB-format (relocatable binary)** program into core at a given base and
returns the start address. Syntax: `RBLOAD FILENAME,START-ADDRESS`. Errors:
`PROGRAM OUT OF BOUNDS`, `CHECKSUM ERROR`, `ILLEGAL CONTROL BYTE`.
`src/TSS5.SYMB:1103`

#### GOTO-USER
Starts (jumps to) your program at a given address, initialising the stack and
activating the process. Arg: start address. Illegal address → `ILLEGAL ADDRESS`.
`src/TSS2.SYMB:378`

#### DUMP
Writes your entire address space to a file (with a dump marker, rubout address and
start location) so it can later be recovered. Arg: file name (optional start
location). `src/TSS4.SYMB:4`

#### RECOVER
Restarts you from a file previously written by DUMP: validates the dump marker,
restores the address space page-by-page, and resumes the program. Prints
`NOT A DUMP FILE` if the marker is wrong. Arg: file name. `src/TSS4.SYMB:78`

#### SAVE-CORE
Saves a range of your core to a core-image file. Syntax: `SAVE-CORE FILENAME,
START-ADDRESS,END-ADDRESS`. Start must be ≤ end (else `ILLEGAL ADDRESS`).
`src/TSS4.SYMB:1930`

#### GET-CORE
Reloads a region of core from a core-image file (the inverse of SAVE-CORE).
Syntax: `GET-CORE FILENAME,START-ADDRESS,END-ADDRESS`. `src/TSS4.SYMB:1975`

#### MAKE-REENTRANT
Marks one or more code blocks in a dump file as reentrant. Syntax:
`MAKE-REENTRANT FILENAME (ADDRESS)…`. Prints `NOT A DUMP FILE` if the file isn't a
dump. `src/TSS4.SYMB:992`

#### MEMORY
Sets the size of your address space. Takes a **lower bound** and an **upper
bound**; if either is omitted it prompts (`LOWER BOUND:`, `UPPER BOUND:`).
**A lower bound of `0` lists the page map instead** (`STA M0,B; JAZ M2`),
showing each page as `READ ONLY`, `READ/WRITE`, or `EMPTY`. Bad address →
`ILLEGAL ADDRESS SPACE`. `src/TSS3.SYMB:1590`

> **[VERIFIED — DEFECT] The assigning form hangs.**
> `MEMORY 40000 44000` produces no output and never returns; the terminal is
> lost. Only the `MEMORY 0` listing form is safe. Path: `MEM` → `CKMEM`
> (`src/TSS2.SYMB:458`) → `CRMEM` (`src/TSS2.SYMB:489`), which posts
> `SAA 40; MST PID` to interrupt level 5. See `TSS-COMMAND-VALIDATION.md`.

#### DELETE-MEMORY
Releases (frees) one or more pages of your address space. Syntax:
`DELETE-MEMORY (ADDRESS)…`. `src/TSS4.SYMB:1034`

#### RESET
Releases your entire address space (frees all your pages). No arguments.
`src/TSS2.SYMB:427`

#### STATUS
Lists your program's saved register block. **[VERIFIED]** the
printed order is `STS, D, P, B, L, A, T, X` — one per line, not the
declaration order. (`MPR` also on NORD-1.) No arguments. `src/TSS3.SYMB:305`

#### SET-REGISTER
Sets a saved register (`P X T A D L B`) or a memory location to a value. The
target is the first field, the value the second; a memory address is range-checked
before writing. `src/TSS3.SYMB:351`

> **[FIXED]** Until this fix the command set the wrong register:
> `SET-REGISTER A 1234` left `A = 0` and set `STS = 234`.
> Root cause: the NORD-10 path read its
> register table with `LDA SSXT,X`, an `(X)+D` instruction whose 8-bit
> displacement cannot hold the table address, so the `IRW` was built with
> register field 0 and wrote STS. The table also lacked the STS slot, so
> `B` read past its end. `src/TSS3.SYMB` now reaches the table through a
> pointer word and holds complete `IRW` words. Verified on nd100x:
> `A 1234`, `X 777`, `B 4321`, `T 55` all read back in `STATUS`,
> `STS` untouched. Details: `TSS-COMMAND-VALIDATION.md` PART III section 4.

#### EXAMINE
Examines core: with one address, prints that location's contents (octal); with a
second address, prints the whole range. `src/TSS4.SYMB:1218`

### 4.4 Peripheral devices

Device names: `TELETYPE`, `TAPE-READER`, `FAST-PUNCH`, `CARD-READER`,
`LINE-PRINTER`, `DIABLO`, `NULL`.

> **A device name is a valid FILE name.** `OPEN` matches its operand against
> the peripheral table `PDEVS` **before** parsing it as a file
> (`src/TSS3.SYMB:1345-1348`, label `O4`): on a match it indexes `DDEVS` for
> the device number and returns open-success; only on no-match does it fall
> through to the file path. So a device can be given wherever a file name is
> expected. Table (`src/TSS2.SYMB:1837-1856`): `TELETYPE`=1, `TAPE-READER`=2,
> `FAST-PUNCH`=3, `CARD-READER`=4, `LINE-PRINTER`=5, `DIABLO`=14, `NULL`=0.
>
> **[VERIFIED]** `RBLOAD` accepted `TAPE-READER` and read the tape;
> `MODE` accepted `TELETYPE` as both input and output file. **But
> `PLACE-BINARY` and `LOAD-BINARY` reject `TAPE-READER`** (`BAD FILENAME`) —
> unexplained, since they call the same `OPEN`.

#### RESERVE
Reserves a named peripheral for your exclusive use. If already taken, prints
`ALREADY RESERVED BY USER <name>`. Arg: device name. `src/TSS4.SYMB:362`

#### RELEASE
Releases a device you reserved (silently does nothing if it isn't yours). Arg:
device name. `src/TSS4.SYMB:401`

#### WHERE-IS
Reports a device's status: `RESERVED BY USER <name>`. **[VERIFIED]**
a **free device produces no output at all** — the documented `FREE TO USE`
message was not observed, for either a free or a just-reserved `TELETYPE`.
Arg: device name. `src/TSS4.SYMB:430`

#### PIN-DEVICE
Pins/activates a device by issuing an I/O start (`IOT PIN`) to it. Arg: device
number. (On NORD-10 builds this is a no-op.) `src/TSS4.SYMB:2015`

### 4.5 Terminals, users & status

#### WHO-IS-ON
Lists every teletype line in use and who is logged on (`LOGGING IN` for a line
still logging in). No arguments. `src/TSS4.SYMB:639`

#### LIST-USERS
Lists all authorised user numbers and their names. No arguments.
`src/TSS4.SYMB:967`

#### LIST-TRACKS
Prints `<n> TRACKS LEFT` for a user. **[VERIFIED]** the user name is
**not** optional in practice: with no argument it prompts `USER NAME:`, and an
empty answer does **not** mean "you" — it yields `0 TRACKS LEFT`. Give the name
explicitly (`LIST-TRACKS SYSTEM`). Note `SYSTEM` holds no track *quota* on a
fresh disc (`0 TRACKS LEFT`) even though `DISK-SPACE` shows 15 free — which is
also why `TRANSFER` answers `NO MORE TRACKS AVAILABLE`. `src/TSS5.SYMB:1606`

#### TIME-USED
Prints `TIME USED IS <t> OUT OF <limit>` — your compute time against your
allotment. No arguments. `src/TSS4.SYMB:1121`

#### DISK-SPACE
Reports free disk space: `<n> TRACKS (<k>K WORDS) LEFT OUT OF <total>`. No
arguments. `src/TSS4.SYMB:1300`

#### LINK-TO
Couples your terminal to another user/terminal (terminal-to-terminal link). Arg:
user name or TTY number. **[VERIFIED]** on failure it prints **`?`**,
not silence (`LINK-TO SYSTEM`, i.e. linking to self, is refused this way).
`src/TSS4.SYMB:459`

#### BREAK-LINKS
Breaks your terminal link, clearing both ends of the coupling. No arguments.
`src/TSS4.SYMB:500`

### 4.6 Passwords, friends & access

#### PASSWORD
Changes your own password: prompts `OLD PASSWORD IS` then `NEW PASSWORD IS`
(a wrong old password re-prompts). No command-line arguments. `src/TSS4.SYMB:606`

#### CLEAR-PASSWORD  *(SYSTEM only)*
Clears (blanks) a specified user's password. Arg: user name. `src/TSS4.SYMB:675`

#### CREATE-FRIEND
Registers another user as a "friend" of an account (granting friend-level access).
Syntax: `CREATE-FRIEND [(USER)] FRIEND-NAME` — owner defaults to you.
`src/TSS4.SYMB:1427`

#### DELETE-FRIEND
Removes a friend from an account. Syntax: `DELETE-FRIEND [(USER)] FRIEND-NAME`.
`src/TSS4.SYMB:1471`

#### LIST-FRIENDS
Lists an account's friends. Syntax: `LIST-FRIENDS [(USER)]` — defaults to you.
`src/TSS4.SYMB:1515`

#### DEFINE-UIB-ACCESS
Sets the 9-bit access word in a user's User Information Block (their access
rights). Syntax: `DEFINE-UIB-ACCESS (USER) ACCESS-WORD` (9 bits, octal).
`src/TSS4.SYMB:1561`

#### DEFINE-FILE-ACCESS
Sets the 9-bit protection word on a file. Syntax: `DEFINE-FILE-ACCESS [(USER)]
FILENAME ACCESS-WORD` (9 bits, octal). `src/TSS4.SYMB:1602`

### 4.7 System administration *(SYSTEM only)*

#### CREATE-USER
Creates a new user account. Arg: new user name.
Errors include `ALREADY EXISTS`, `TOO LONG`, `NO MORE ROOM`. `src/TSS4.SYMB:920`

> **[VERIFIED — DEFECT] False error.** On a disc holding only
> `SYSTEM`, `CREATE-USER TESTU` printed **`ALREADY EXISTS`** and no
> `USER NUMBER = <n>` — yet the user **was** created (`LIST-USERS` then showed
> `1 SYSTEM`, `2 TESTU`, and `DELETE-USER TESTU` removed it cleanly). Treat
> `ALREADY EXISTS` from this command as unreliable; check `LIST-USERS`.

#### DELETE-USER
Deletes a user account. Arg: user name. `src/TSS4.SYMB:1899`

#### TRANSFER
Transfers disk-track quota between users. Prompts `TO USER:` and (for SYSTEM)
`FROM USER:` and `NUMBER OF TRACKS:`. A non-privileged user may only move tracks
between their own account and another. `src/TSS5.SYMB:1570`

#### INIT-ACCOUNTING
Initialises (clears) the accounting file on disk. No arguments.
`src/TSS4.SYMB:704`

#### LIST-ACCOUNTS
Writes a usage report (`TIMESHARING SYSTEM USAGE REPORT` — compute time and
console time per project) to a named output file. Args: output file, title.
`src/TSS4.SYMB:813`

#### RESPONSE-TIME
Prints `AVERAGE RESPONSE TIME IS <n.n> SECONDS OVER A PERIOD OF <t>`. No
arguments. `src/TSS5.SYMB:1946`

#### DEFINE-DATE
Sets the system date/time. Prompts `DAY: MONTH: YEAR: HOUR: MINUTE: SECOND:`
(decimal). Once the clock is running, only SYSTEM may change it. `src/TSS5.SYMB:1872`

#### CLOCK-ON
Turns the system clock on. No arguments. `src/TSS2.SYMB:1809`

#### CLOCK-OFF  *(SYSTEM only)*
Turns the system clock off. No arguments. `src/TSS2.SYMB:1820`

#### SYSUP
Makes the system available to new users (clears the "unavailable" flag). No
arguments. `src/TSS5.SYMB:1019`

#### SYSDOWN
Makes the system **unavailable** to new users (they then see `SYSTEM IS
UNAVAILABLE`). No arguments. `src/TSS5.SYMB:1037`

#### DEFINE-VERSION
Selects which coreload (version **A** or **B**) the front-panel LOAD button loads.
Prompts `VERSION (A OR B):`. *(CDC-disc builds only.)* `src/TSS5.SYMB:578`

#### LOAD-SYSTEM
Reloads (reboots) the TSS system from disk. No arguments. *(CDC-disc builds
only.)* `src/TSS5.SYMB:609`

#### BACKUP
Disk-pack backup: copies/verifies tracks and reports `DISK ERROR AT <addr>`. No
arguments. *(NORD-1 builds only; a no-op on NORD-10.)* `src/TSS5.SYMB:741`

---

## 5. Scratch files — the per-user work area

### What a scratch file is
Each user has a **scratch file** called **`SCRATCH:DATA`** — a random-access
`DATA` file in the user's *own* directory. In the source it is the string
`()SCRATCH` (the empty `()` user-group means "my own user"), and its type comes
from `MTP5, 'DATA'`. (`src/TSS4.SYMB:544`; `src/TSS1.SYMB:4735`.)

### What it is used for
It is the user's disk-backed **scratch / work area** — temporary page storage for
programs that need more than core. The kernel routines `RDISK` / `WDISK`
("READ / WRITE A PAGE FROM / ONTO THE USER'S SCRATCH AREA", `src/TSS2.SYMB:866`
and `885`) read and write pages **by page number** into it. `LOGON` opens it at
login as file number **100** (the first open file) for **random write**
(`src/TSS3.SYMB`, OPEN mode 2). Its contents are transient — there is no fixed
format and no required initial content.

### How a scratch file is created

**It is *not* created automatically** — verified in the source:

- **Login** (`LOGON`) *opens* `()SCRATCH` expecting it to already exist; it does
  not create it. On a freshly formatted disc that open fails and login simply
  continues — this is the harmless **`AMBIGUOUS FILENAME`** shown on the first
  login (see §2).
- **`CREATE-USER` / `CRUSE`** and cold-start **`SINIT`** build the user's
  user-table entry and give one track of quota, but do **not** make a scratch
  file (`src/TSS5.SYMB:61`, `src/TSS2.SYMB:856`).

In TSS a file is created by opening it with a **create marker** — a leading `"`
on the file name. `OPEN` sets its "new file" (create) flag only when the name
carries that `"` (`src/TSS3.SYMB`, label `O4A`); without it, `OPEN` looks up an
*existing* file. So a `SCRATCH:DATA` comes into being one of two ways:

- **On demand (inferred):** a program that needs scratch opens `"SCRATCH:DATA`
  itself — the normal way a program obtains work space.
- **Manually — DISPROVEN.** `OPEN-FILE "SCRATCH:DATA,WX` was previously
  inferred here. **[VERIFIED]** it returns **`BAD FILENAME`**, on
  the command line *and* through the `FILE NAME:` prompt. Without the quote,
  `OPEN-FILE SCRATCH:DATA,W` correctly returns `NO SUCH FILE`. So the command
  processor rejects the create marker.

> **[RESOLVED — supersedes the note below.] Files CAN be created.**
> The name must be quoted on **both** sides: `OPEN-FILE "MYFILE",W` returns
> `FILE NUMBER = 100`, charges one track to the user's quota and one to the
> disc, and `LIST-FILE` then shows `MYFILE:SYMB`. The earlier test used
> `"SCRATCH:DATA,WX` — an opening quote with **no closing quote** — which
> `FFOPE` rejects at `src/TSS3.SYMB:1376` (`O16`, name exhausted while
> `NEWF < 0`) with error `0o57` = `BAD FILENAME`. The `"` is a delimiter pair,
> not a prefix flag. Full derivation and transcript:
> `docs/TSS-COMMAND-VALIDATION.md` D4.
>
> Note the file cannot be created by a user with **zero track quota** — a
> freshly created user gets `NO MORE TRACKS AVAILABLE`. See D5.

> **Bring-up implication.** Because nothing in the login / user-creation /
> cold-start path creates it, a freshly-MINIT'd disc has no `SCRATCH:DATA`, so the
> first login prints `AMBIGUOUS FILENAME`. That is expected and harmless. If you
> want clean logins, create a `SCRATCH:DATA` for each user once as part of
> site setup; otherwise it is created on demand by whatever program first needs
> scratch space. There is **no dedicated "make scratch files" init routine** in
> the recovered source.

### Monitoring & operating scratch files
- **`LIST-FILE SCRATCH`** — show the scratch file's directory entry (size, owner).
- **`LIST-OBJECTS`** — list every object-table entry (owner, name, attributes).
- **`DISK-SPACE`** / **`LIST-TRACKS`** — how much disk it (and everything else)
  consumes.
- **`DELETE-FILE SCRATCH`** — remove it.
- **`LOGOUT`** closes files and frees the user's temporary tracks
  (`src/TSS4.SYMB`, `LOGOUT`), reclaiming transient scratch space at sign-off.

*Verified live:* the inferred create command **does not work** (see
above). `NO SUCH FILE` (error 8) demonstrably exists and is what `RENAME`,
`DELETE-FILE` and `DEFINE-FILE-ACCESS` return for a missing file — while
`LIST-FILE` returns **nothing at all** for the same missing file. Why `LOGON`'s
failed open reports `AMBIGUOUS FILENAME` (error 31) instead is therefore still
open, as is whether `SCRATCH:DATA` is meant to persist across sessions.

---

## Appendix A — TSS error messages

`ERMSG` prints a message by number. These are the messages verbatim from
`src/TSS5.SYMB` (the `ERTBL` tables).

| # | Message | | # | Message |
|---|---|---|---|---|
| 1 | NO MORE TRACKS AVAILABLE | | 26 | BAD DISK ADDRESS |
| 2 | TOO MANY USERS | | 27 | FILE MUST BE CLOSED BY EVERYONE |
| 3 | USER ALREADY EXISTS | | 28 | SPECIFIED USER IS ALREADY A FRIEND |
| 4 | NO SUCH USER | | 29 | NO SUCH FRIEND |
| 5 | USER INDEX BLOCK FULL | | 30 | NO MORE ROOM IN FRIEND TABLE |
| 6 | FILE ALREADY EXISTS | | 31 | AMBIGUOUS FILENAME |
| 7 | ILLEGAL OBJECT TYPE | | 32 | BAD CHARACTER COUNT |
| 8 | NO SUCH FILE | | 33 | DEVICE ERROR AT |
| 9 | OBJECT INDEX OUT OF RANGE | | 34 | STACK OVERFLOW |
| 10 | OBJECT TABLE FILLED | | 35 | STACK UNDERFLOW |
| 11 | OPEN FILE TABLE FILLED | | 36 | ILLEGAL ADDRESS |
| 12 | INSUFFICIENT ACCESS | | 37 | DISK ERROR |
| 13 | ALREADY OPEN FOR WRITE | | 38 | YOUR TWO MINUTES ARE UP |
| 14 | BAD FILE NUMBER | | 39 | USER BREAK AT P = |
| 15 | NO SUCH TRACK | | 40 | ILLEGAL INSTRUCTION AT P = |
| 16 | BAD TRACK NUMBER | | 41 | BATCH JOB ABORTED |
| 17 | TRACK ALREADY EXISTS | | 42 | NORD TSS VERSION 3.0A IS UP |
| 18 | NO SUCH PAGE | | 43 | AMBIGUOUS |
| 19 | FATAL ERROR | | 44 | PARITY ERROR |
| 20 | TRANSFER ERROR | | 45 | I/O ERROR |
| 21 | BAD DEVICE TYPE | | 46 | YOU MAY NOT DO THIS |
| 22 | DISK AREA ALREADY IN USE | | 47 | BAD FILENAME |
| 23 | WRITE NOT PERMITTED | | 48 | TRACKS LEFT |
| 24 | NOT A SEQUENTIAL FILE | | 49 | NORD TSS VERSION 3.0B IS UP |
| 25 | BAD CORE ADDRESS | | 50 | SYSTEM IS UNAVAILABLE |

Also present as loader errors: `PROGRAM OUT OF BOUNDS`, `CHECKSUM ERROR`,
`ILLEGAL CONTROL BYTE`.

## Appendix B — Peripheral device names

Names accepted by `RESERVE` / `RELEASE` / `WHERE-IS` (from `PDEVS`,
`src/TSS2.SYMB`): `TELETYPE`, `TAPE-READER`, `FAST-PUNCH`, `CARD-READER`,
`LINE-PRINTER`, `DIABLO`, `NULL`.

## Appendix C — Full command list (as shown by `@HELP`)

RESET · RECOVER · DUMP · GOTO-USER · LOAD-BINARY · PLACE-BINARY · MEMORY ·
LOGOUT · HELP · OPEN-FILE · CLOSE-FILE · LIST-FILE · DELETE-FILE · RENAME ·
STATUS · SET-REGISTER · EXAMINE · RESERVE · RELEASE · WHERE-IS · CLOCK-ON ·
CLOCK-OFF · LINK-TO · BREAK-LINKS · CREATE-USER · PASSWORD · WHO-IS-ON ·
INIT-ACCOUNTING · LIST-ACCOUNTS · LIST-USERS · TIME-USED · PAUSE · CONTINUE ·
DISK-SPACE · ALLOCATE · LIST-OBJECTS · CREATE-FRIEND · DELETE-FRIEND ·
LIST-FRIENDS · DEFINE-UIB-ACCESS · DEFINE-FILE-ACCESS · MODE · MAKE-REENTRANT ·
DELETE-MEMORY · CLEAR-PASSWORD · RESPONSE-TIME · DELETE-USER · SAVE-CORE ·
GET-CORE · PIN-DEVICE · DEFINE-DATE · DATE · RBLOAD · TRANSFER · LIST-TRACKS ·
BACKUP · DEFINE-VERSION · LOAD-SYSTEM · SYSUP · SYSDOWN
