# TSS 3.0 — Users, Login, and Accounting

**Status / marking convention.** Every claim below is tagged. `[VERIFIED]` = read
directly from the cited source line(s). `[ASSUMPTION]` = my interpretation of
verified code, clearly marked as such. `NOT DETERMINED FROM SOURCE` = I could not
establish it from the archive and did not guess. Citations are `src/FILE.SYMB:LINE`.

This document is the deep, routine-level reference for everything about users. It
**does not repeat** the operator-switch / MINIT-format / cold-boot how-to — that
lives in [`DISK-INIT-USERS-AND-BOOT.md`](DISK-INIT-USERS-AND-BOOT.md) and the
handoff [`HANDOFF-LOGIN-BRINGUP.md`](HANDOFF-LOGIN-BRINGUP.md). Read those first for
the boot sequence; read this for the internals of the routines and tables.

---

## 1. User identity model

### 1.1 The two on-disc tables

TSS keeps user information in **two independent disc structures**, both indexed by
the same 1-based user number. This split is the crux of login (see §4).

| table | disc base address | entry stride | holds | written by |
|---|---|---|---|---|
| **`USTBL`** | `DKBIT + block` | 8 words/entry (`SHA 3`) | user **name** (7 words) + track ownership | `RUTBL`/`WUTBL`, i.e. `CRUSE`/`FCRUS`, `USARR`, `GETUN`, `UNDK` |
| **`USRDK`** | `1460₈` (`= MAILD+MAILS+400`) | 8 words/entry (`SAD 10`) | password word at **offset 7**; access/track words | `LOGON`, `PASWD`, `CPASW`, `CRUSR` via `XDISK` into `USRTB` |

- **[VERIFIED]** `USRDK = MAILD+MAILS+400` — `src/TSS1.SYMB:2610`. With
  `MAILS = 200₈` (`src/TSS1.SYMB:2607`) this resolves to the `1460₈` cited in the
  disk-init doc. `USRTB = QBUF` is the in-core copy — `src/TSS2.SYMB:218`.
- **[VERIFIED]** `USTBL` disc base uses `DKBIT`. Two conditional definitions exist:
  `DKBIT = 0` under the `"NCR` mark and `DKBIT = 50` under the `"CDC` mark —
  `src/TSS1.SYMB:2596-2599`. For the archived CDC build `DKBIT = 50₈`.
- **[VERIFIED]** In-core `USTBL` is a one-track scratch buffer preceded by a 4-word
  header (`LOCK`, `IN CORE FLAG`, `DEVICE ID`, `INDEX 1..7`) and sized `BSS 400₈` —
  `src/TSS1.SYMB:4592-4598`. The header words are addressed as `USTBL-4`…`USTBL-1`.

### 1.2 Entry layout within `USTBL`

**[VERIFIED]** An entry is 8 words: name in words 0–6, track/ownership word at
offset 7. The name copy is 7 words (`SAA 7; JPL I (BCOPY`) starting at
`USTBL + idx*8` (`SHA 3`) — `src/TSS3.SYMB:660-661`. The password lookup in the
`USRDK`/`USRTB` image also uses offset 7 (`SAX 7; LDA I OBJ,B,X`) —
`src/TSS4.SYMB:562`. So **offset 7 in `USTBL` = track word; offset 7 in `USRTB`
(the `USRDK` image) = password word** — two different tables, same stride and
same offset-7 slot.

### 1.3 User number, SYSTEM, and the maximum

- **[VERIFIED]** User numbers are **1-based**. Everywhere a user number is turned
  into a table index the code does `AAA -1` first (e.g. `src/TSS4.SYMB:559`,
  `src/TSS2.SYMB:3091`).
- **[VERIFIED]** The **first user created is SYSTEM**, made by `SINIT` with the
  packed name in `SINQ` — `src/TSS2.SYMB:858-860`. `[ASSUMPTION]` Because it is
  created first into an empty table, SYSTEM receives user number **1** (the create
  path fills the lowest free slot; §2).
- **[VERIFIED]** "SYSTEM" privilege is tested as **user number == 1**: routines that
  must be run by SYSTEM do `JPL I (WHUSR; AAA -1; JA{Z/F} …` — e.g. `CRUSR`
  `src/TSS4.SYMB:943`, `IACCT` `src/TSS4.SYMB:715`, `CPASW` `src/TSS4.SYMB:690`.
  `WHUSR` returns the caller's own user number — `src/TSS2.SYMB:2337-2339`.
- **[VERIFIED]** Maximum users: the create/scan loops run **8 table blocks × 32
  entries**. `CRUSE` inner loop tests `CNTX ... AAA -40` (32 decimal entries per
  block) — `src/TSS5.SYMB:92` — and outer loop `CNT ... AAA -10` (8 blocks) —
  `src/TSS5.SYMB:94`. The user number is reconstructed as
  `(block-1)*32 + entry + 1` via `AAA -1; SHA 5` (×`40₈`=32) `ADD ACNTX; AAA 1` —
  `src/TSS5.SYMB:101`. `[ASSUMPTION]` therefore the ceiling is **256 users**
  (8×32); `USTBL`'s `BSS 400₈` = 256 words = one block of 32×8-word entries, and the
  8 blocks are the 8 tracks the create loop reads via `RUTBL` index 1..8.
- **[VERIFIED]** `USARR` rejects a requested index `> 101₈` (65) with
  `AAA -101; JAP UFF` — `src/TSS3.SYMB:656`. `[ASSUMPTION]` this is a guard on the
  *string-array* accessor's index argument, not the true user ceiling. `NOT
  DETERMINED FROM SOURCE`: why the accessor cap (65) is below the create ceiling
  (256).

---

## 2. User creation

### 2.1 `CRUSE` / `FCRUS` — the create-user primitive (TSS5, overlay OV10)

**[VERIFIED]** `src/TSS5.SYMB:61-114`. Header comment `src/TSS5.SYMB:61-70`:
`D = device id`, `X = pointer to user name`, returns `A = user number`; failure
`A = 1` no more tracks, `A = 2` too many users, `A = 3` user already exists.

Call chain, in order (all `[VERIFIED]` at the cited lines):

1. `LOCK` the table — `src/TSS5.SYMB:82`.
2. **Scan for a duplicate name and remember the lowest free slot**: loop over blocks
   (`RUTBL` index `CNT`, `src/TSS5.SYMB:85`) and within a block over 32 entries at
   `USTBL + CNTX*8` (`src/TSS5.SYMB:87`), comparing the 7-word name with `BEQL`
   (`src/TSS5.SYMB:88`; a match jumps to `CF3` → return 3 "already exists"). A free
   entry (`LDA 0,X; JAF`) whose index is remembered in `ACNT`/`ACNTX`
   (`src/TSS5.SYMB:89-91`). Each scanned block is written back with `WUTBL`
   (`src/TSS5.SYMB:93`).
3. If no free slot was found (`ACNT` still −1) → `CF2` → return 2 "too many users"
   — `src/TSS5.SYMB:95,110`.
4. **`GTRK`** — allocate a free disc track by scanning the MIB bitmap
   (`src/TSS5.SYMB:97`; `GTRK` body `src/TSS2.SYMB:2665-2682`). Failure → `CF1` →
   return 1 "no more tracks" — `src/TSS5.SYMB:97,108`.
5. **Store the name + track** into the chosen `USTBL` entry: `STT 7,X` writes the
   track at offset 7, then `BCOPY` copies the 7-word name in — `src/TSS5.SYMB:98-100`.
6. **`WUTBL`** — write the table block back to disc — `src/TSS5.SYMB:102`.
7. **`IUSER`** — initialise the new user's tracks — `src/TSS5.SYMB:104` (body
   `src/TSS5.SYMB:117-143`: it lays down the UIB block `LDA (731; STA 1,X` and
   clears the track bitmap via `SETB`/`WBLOK`).
8. `UTRK` charges one track against the user, then `UNLOK`; return the user number
   in `A` — `src/TSS5.SYMB:106,112`.

**Note — no password is set.** `CRUSE` touches only `USTBL` (names/tracks) and the
MIB; it never writes `USRDK`. **[VERIFIED]** by absence of any `USRDK`/`USRTB`
reference in `src/TSS5.SYMB:61-114`. This is why a freshly-created user (including
SYSTEM) is **passwordless** — the `USRDK` password word is simply never written and
reads as 0 (see §4).

### 2.2 `SINIT` — cold-create SYSTEM (TSS2)

**[VERIFIED]** `src/TSS2.SYMB:856-862`. `SINIT` loads `X` with `SINQ` (the packed
name "SYSTEM": `#SY; #ST; #EM; 0; 0; 0; 0`), clears `D` (device 0), calls `CRUSE`,
then `JMP I (LEV2`. It is the cold-boot entry selected by the operator switch — see
`DISK-INIT-USERS-AND-BOOT.md` §1/§3; not repeated here.

### 2.3 `CRUSR` — interactive create (TSS4, overlay OV7)

**[VERIFIED]** `src/TSS4.SYMB:920-964`. Header: *"CALLING USER MUST BE 'SYSTEM'"*
(`src/TSS4.SYMB:923`), enforced by reading the caller's TTY user slot and testing
`==1` (`LDA 0,X; AAA -1; JAF CF1` → "YOU MAY NOT DO THIS") — `src/TSS4.SYMB:942-943`.

What it does beyond `CRUSE`:

1. `SETB` marks a 7-word field, reads the `USRDK` table into `USRTB` via `XDISK`
   (`SAA 1` = read) — `src/TSS4.SYMB:944-945`.
2. Parses the requested name from the command line (`PCF`) — `src/TSS4.SYMB:947`.
3. Checks the name is not already present via `ABLKP` over `USRTB`
   (`src/TSS4.SYMB:948-950`; ambiguous → `CF2`, exists → `CF3`).
4. Calls **`CRUSE`** to create the `USTBL` entry and allocate a track —
   `src/TSS4.SYMB:951`.
5. Then **also writes `USRDK`**: it computes the `USRTB` entry for the new user
   (`(uno-1)*8 + USRTB`), `SETB`s the whole 8-word entry (setting the password slot),
   and writes `USRDK` back with `XDISK SAA 3` (= write) — `src/TSS4.SYMB:952-954`.
   `[ASSUMPTION]` the initial password word written here is the `SETB` bit pattern,
   i.e. a *non-zero placeholder*; `NOT DETERMINED FROM SOURCE`: whether that makes a
   `CRUSR`-created user password-protected until `CPASW`/`PASWD` runs, or whether the
   pattern is treated as "no password". (Contrast SYSTEM via `SINIT`, whose `USRDK`
   word is never written and is therefore truly 0/passwordless.)
6. Prints `USER NUMBER = <n>` — `src/TSS4.SYMB:955-956`.

### 2.4 Supporting table primitives (TSS2)

- **`RUTBL`** — read a `USTBL` block from disc into the in-core buffer; caches by
  device+index so a re-read is skipped (`USTBL-3` "valid" flag) —
  `src/TSS2.SYMB:3031-3055`. Touches `USTBL`.
- **`WUTBL`** — write the in-core `USTBL` block back to `DKBIT+index` —
  `src/TSS2.SYMB:3058-3074`. Touches `USTBL`.
- **`GTRK`** — acquire a free track by scanning the MIB bit-table (`BITBL`/`BITM`) —
  `src/TSS2.SYMB:2655-2682`. Touches the MIB, not the user tables. (This is the step
  that fails on an unformatted disc — see `DISK-INIT-USERS-AND-BOOT.md`.)
- **`IUSER`/`FIUSE`** — initialise a new user's tracks/UIB — `src/TSS5.SYMB:117-143`.
- **`UNDK`** — convert user number → disc track address by reading `USTBL` offset 7 —
  `src/TSS2.SYMB:3077-3099`. Touches `USTBL`.
- **`GETUN`/`FGETU`** — name → user number lookup over `USTBL` (like the `CRUSE`
  scan but read-only) — `src/TSS5.SYMB:146-174`. Touches `USTBL`.
- **`UTRK`** — track-quota accounting per user (§5.1) — `src/TSS2.SYMB:3102-3124`.

---

## 3. Name lookup — `USARR` and `ABLKP`

**[VERIFIED]** `USARR` (OV1A) is the *string-array accessor*: given a user index in
`A` and a string descriptor in `X`, it reads block `RUTBL`, copies the 7-word name
out of `USTBL + idx*8` into `USTRG`, and streams it to the caller's string —
`src/TSS3.SYMB:651-674` (name copy at `src/TSS3.SYMB:660-661`). **It reads names
from `USTBL`, never `USRDK`.**

**[VERIFIED]** `ABLKP` (OV1A) is the abbreviation/prefix matcher — header
`src/TSS3.SYMB:715-726`: given a typed string in `X`, an array-accessor function in
`T`, and an array pointer in `A`, it returns the matching index, or `A=-1` no match /
`A=-2` ambiguous. `LOGON` and `CRUSR` pass `USARR` as the accessor so `ABLKP`
effectively searches the **name** table. Body `src/TSS3.SYMB:730-…`.

---

## 4. Login — `LOGON` (TSS4, overlay OV6)

**[VERIFIED]** `src/TSS4.SYMB:524-603`. Prompt strings: `MS1 = '$$@ENTER \ '`,
`MS2 = '$PASSWORD \ '`, `MS3 = 'OK\ '`, `MS4 = '$PROJECT NUMBER P-\ '`,
`MS5 = '*****YOU HAVE MAIL*****'` — `src/TSS4.SYMB:539-544`.

Flow (all `[VERIFIED]` at the cited lines):

1. Set login mode, clear per-session lock/mail/tty/accounting flags; if the clock is
   set, print the date — `src/TSS4.SYMB:547-549`.
2. Print `@ENTER ` (MS1) and read the typed name character-by-character via `TCI`
   until CR (`SAT 15` = CR), echoing with `WCI` — `src/TSS4.SYMB:552-556` (`L2`/`L3`
   loop). **This is the character path that the mac-c `9377` fix restored** — the
   `TCI` ring returned zeroed characters before the fix; see the memory note
   `tss-login-char-path` and `HANDOFF-LOGIN-BRINGUP.md` §1. Not re-derived here.
3. **`L4` — read `USRDK` into `USRTB`** (`SAA 1` = read via `XDISK`), then **match the
   name**: `LDT (USARR; ... JPL I (ABLKP` searches `USTBL` names; no match loops back
   to `L2` (re-prompt) — `src/TSS4.SYMB:557-559`.
4. Convert the matched user number to a `USRTB` entry: `AAA -1 ... SAD 10; ADD
   (USRTB` → `OBJ = USRTB + uno*8` — `src/TSS4.SYMB:559-561`.
5. **Password check at offset 7**: `SAX 7; LDA I OBJ,B,X; JAZ LOGS` — if the password
   word is **0, login succeeds immediately** (`LOGS`) — `src/TSS4.SYMB:562`. This is
   the passwordless path SYSTEM takes.
6. Otherwise prompt `PASSWORD ` (MS2), read it (shifting characters into `PASSW`,
   `SHT 3; RADD`), and compare `USRTB[uno][7]` against the typed word;
   mismatch (`L7`) breaks and re-prompts from `L2` — `src/TSS4.SYMB:563-569,601`.
   Match → print `OK` and fall into `LOGS` — `src/TSS4.SYMB:570-571`.
7. `LOGS`: prompt `PROJECT NUMBER P-` (MS4), read the project number into `PRNUM`
   (`CSN` decimal scan) — `src/TSS4.SYMB:571-578`. Store the logged-in user number
   into the terminal table `TTYTB[cinf]` and clear the login timer slot `TIMTB`;
   announce mail if any (`CMAIL` → MS5) — `src/TSS4.SYMB:579-581`. Then open the
   user's `SCRATCH` file and drop into command mode — `src/TSS4.SYMB:584-592`.

**[VERIFIED]** a special hard-coded password constant: `LDA PASSW,B; SUB (636; JAZ
L6A` — `src/TSS4.SYMB:568`. `[ASSUMPTION]` `636₈` is a master/back-door password
value that bypasses the per-user check (jumps straight to the `OK` path `L6A`).
`NOT DETERMINED FROM SOURCE`: its intended purpose (service/master key vs. sentinel).

**The `@ENTER` re-prompt on unknown name** (the "silently re-prompts" behaviour
noted in the disk-init doc) is the `JMP L2` after a failed `ABLKP` —
`src/TSS4.SYMB:559`.

---

## 5. Password management

- **`PASWD`** (PASSWORD command, OV6) — `src/TSS4.SYMB:606-636`. Reads `USRDK` into
  `USRTB` (`src/TSS4.SYMB:620`), finds the caller's own entry via `WHUSR`
  (`src/TSS4.SYMB:621-623`), prompts `OLD PASSWORD IS ` and verifies the typed old
  password against offset 7 (`src/TSS4.SYMB:624-627`; mismatch loops to `PAA`), then
  prompts `NEW PASSWORD IS `, stores the new word at offset 7, and writes `USRDK`
  back with `XDISK SAA 3` — `src/TSS4.SYMB:628-633`. Touches **`USRDK`** only.
- **`CPASW`** (CLEAR PASSWORD, OV6A) — `src/TSS4.SYMB:675-701`. **SYSTEM only**
  (`WHUSR; AAA -1; JAF CF1` → "YOU ARE NOT AUTHORIZED TO DO THIS") —
  `src/TSS4.SYMB:690,699,685`. Parses the target user name/number (`GUNUM`), zeroes
  offset 7 of that user's `USRTB` entry (`STZ 7,X` → password 0 = passwordless), and
  writes `USRDK` back (`SAA 3`) — `src/TSS4.SYMB:693-697`. Touches **`USRDK`**.
- **`PAUSE`** (OV6A) — `src/TSS4.SYMB:782-804` — re-prompts `PASSWORD IS ` and
  re-verifies the caller's own password (offset 7 of `USRTB`) before resuming a
  suspended session. Read-only on `USRDK` (reads, no write) — `src/TSS4.SYMB:795-801`.

Both `PASWD` and `CPASW` are registered command names in the TSS5 command table —
`src/TSS5.SYMB:1795,1799`.

---

## 6. Accounting

TSS has a real per-user CPU/connect accounting subsystem, distinct from the
per-user **track quota** in `UTRK`.

### 6.1 Track quota — `UTRK` (TSS2)

**[VERIFIED]** `src/TSS2.SYMB:3102-3124`. Header: `A = number of tracks to be
charged`, `T = user number`; returns `A = tracks remaining`, failure `A=1` no more
tracks, `A=4` no such user. It reads the user's track-limit block (`RBLOC`),
subtracts the charge from `BTEMP+2`, and rejects if it would go negative
(`JAN UFX` → return 1) — `src/TSS2.SYMB:3117-3121`. This is a **per-user disc-track
budget**, enforced at track allocation (`CRUSE` calls `UTRK` — `src/TSS5.SYMB:106`).

### 6.2 Access control — `CKACC` (TSS2)

**[VERIFIED]** `src/TSS2.SYMB:3169-3215`. Checks whether the current user has
sufficient access (`OWR` = owner/write/read, 3 bits) to an object, using the UIB
access word and the owner user number. Owner (`SUB TREG,B; JAZ CO`) gets the top
3 bits; non-owners are looked up in the UIB's per-user access list —
`src/TSS2.SYMB:3201-3208`. Called throughout TSS5 file ops (e.g.
`src/TSS5.SYMB:261,324,353,396,438,674`).

### 6.3 CPU / connect-time accounting file — `IACCT`, `UACCT`, `PACCT`

- **`IACCT`** (INIT-ACCOUNTING, OV6A) — `src/TSS4.SYMB:705-721`. **SYSTEM only**
  (`WHUSR; AAA -1; JAZ I1`). Writes a fresh (−1-marked) accounting directory block
  to `ACCTD` via `XDISK SAA 3` — `src/TSS4.SYMB:715-718`. Touches the **accounting
  file `ACCTD`**, not the user tables.
- **`UACCT`** (UPDATE-ACCOUNTING) — `src/TSS4.SYMB:724-779`. Accumulates usage into
  the accounting file, keyed by **project number `PRNUM`** and **user number**
  (`WHUSR`). It reads the current terminal's connect-time base `CSTIM` and the
  process CPU field from `PRT` (`LDX I (PRT; LDD 2,X`), then walks the `ACCTD`
  directory blocks (`QBUF`), finds or creates the project's slot, and does a
  double-precision add of CPU and connect time into the per-user 5-word sub-entry
  (`STF I USENT`, `STD 3,X`) — `src/TSS4.SYMB:742-777`. Locked by `ACCLK`/`KACCL`
  (`src/TSS4.SYMB:747-748,777`). Touches **`ACCTD`**.
  `[ASSUMPTION]` the 5-word per-project/user sub-entry is `{user#, cpu-hi/lo,
  connect-hi/lo}` based on the `INFO,5` layout at `src/TSS4.SYMB:732,743-745,762-767`;
  `NOT DETERMINED FROM SOURCE`: the exact field units.
- **`PACCT`** (PRINT ACCOUNT FILE, OV6B) — `src/TSS4.SYMB:813-…`. Formats and prints
  the accounting file. `[VERIFIED]` header/entry `src/TSS4.SYMB:813-824`.
- Command names: `INIT-ACCOUNTING` (`SC28`) and `LIST-ACCOUNTS` (`SC29`) —
  `src/TSS5.SYMB:1670-1671`.
- The accounting file lives at **`ACCTD = CORL2+200`** on disc —
  `src/TSS1.SYMB:2603`; lock word `ACCLK` — `src/TSS1.SYMB:4556`,
  `KACCL` — `src/TSS2.SYMB:94`. Per-terminal login timer table `TIMTB` and the
  system clock `TIME0` — `src/TSS1.SYMB:4547,4636`; `TIMTB` is reset per login
  (`src/TSS4.SYMB:551,580`).

`NOT DETERMINED FROM SOURCE`: the precise on-disc byte layout of the `ACCTD`
directory (block count, project-entry stride beyond the `-14`/`-4`/`-200` loop bounds
at `src/TSS4.SYMB:770-774`), and whether CPU time is charged in clock ticks or
another unit.

---

## 7. The login character-path dependency (cross-reference only)

Login is only reachable because of the mac-c assembler `9377` fix. That is **fully
covered** in the memory note `tss-login-char-path` and in
[`HANDOFF-LOGIN-BRINGUP.md`](HANDOFF-LOGIN-BRINGUP.md) §1 (the tokenizer bug that
made `AND 9377` assemble as `AND 0`, zeroing every console character). Not
re-derived here. The relevant login-side consumer is the `TCI` read loop at
`src/TSS4.SYMB:555,565,574`.

---

## 8. Master table — user-related routines

| routine | file:line | what it does | table(s) touched |
|---|---|---|---|
| `SINIT` | `src/TSS2.SYMB:858` | cold-create user SYSTEM (calls `CRUSE`) | `USTBL` (via `CRUSE`) |
| `CRUSE`/`FCRUS` | `src/TSS5.SYMB:61` | create-user primitive: dedup, `GTRK`, write name+track, `IUSER` | `USTBL`, MIB |
| `CRUSR` | `src/TSS4.SYMB:920` | interactive create (SYSTEM only); writes both tables | `USTBL` + `USRDK` |
| `IUSER`/`FIUSE` | `src/TSS5.SYMB:117` | initialise a new user's tracks/UIB | user tracks/UIB |
| `GETUN`/`FGETU` | `src/TSS5.SYMB:146` | name → user number (read-only scan) | `USTBL` |
| `RUTBL` | `src/TSS2.SYMB:3031` | read a `USTBL` block from disc (cached) | `USTBL` |
| `WUTBL` | `src/TSS2.SYMB:3058` | write a `USTBL` block back to disc | `USTBL` |
| `UNDK` | `src/TSS2.SYMB:3077` | user number → disc track address | `USTBL` |
| `GTRK` | `src/TSS2.SYMB:2655` | allocate a free disc track from the MIB | MIB bitmap |
| `USARR` | `src/TSS3.SYMB:651` | string-array accessor: read the i-th user name | `USTBL` |
| `GBARR` | `src/TSS3.SYMB:677` | string-array accessor over a user's files | user file index |
| `ABLKP` | `src/TSS3.SYMB:730` | abbreviation/prefix matcher (name lookup) | via `USARR` → `USTBL` |
| `LOGON` | `src/TSS4.SYMB:524` | login: name match, password check (offset 7 of `USRDK`) | reads `USTBL` (name) + `USRDK` (pw) |
| `PASWD` | `src/TSS4.SYMB:606` | PASSWORD command: change own password | `USRDK` |
| `CPASW` | `src/TSS4.SYMB:675` | CLEAR PASSWORD (SYSTEM only): zero a user's password | `USRDK` |
| `PAUSE` | `src/TSS4.SYMB:782` | re-verify own password to resume a session | reads `USRDK` |
| `WHUSR` | `src/TSS2.SYMB:2337` | return caller's own user number (from `TTYTB`) | `TTYTB` |
| `WHOS` | `src/TSS4.SYMB:639` | "who is on": list logged-in users | `TTYTB`, `USARR`→`USTBL` |
| `LUSR` | `src/TSS4.SYMB:967` | list authorized users | `USARR`→`USTBL` |
| `UTRK` | `src/TSS2.SYMB:3102` | per-user track-quota accounting | user track budget |
| `CKACC` | `src/TSS2.SYMB:3169` | per-object access-rights check | UIB access words |
| `IACCT` | `src/TSS4.SYMB:705` | INIT-ACCOUNTING (SYSTEM only) | `ACCTD` |
| `UACCT` | `src/TSS4.SYMB:724` | UPDATE-ACCOUNTING: add CPU+connect time | `ACCTD` |
| `PACCT` | `src/TSS4.SYMB:813` | PRINT/LIST accounting file | `ACCTD` |

---

## 9. Open questions

1. `CRUSR`'s initial `USRDK` write uses `SETB` on the 8-word entry — is a
   `CRUSR`-created user password-protected until `PASWD`/`CPASW`, or is the `SETB`
   pattern treated as "no password"? (§2.3) — **`NOT DETERMINED FROM SOURCE`.**
2. The hard-coded `636₈` password constant in `LOGON` (`src/TSS4.SYMB:568`) — master
   key or sentinel? (§4) — **`NOT DETERMINED FROM SOURCE`.**
3. `USARR`'s index cap of `101₈` (65) vs. the create ceiling of 256 users
   (`src/TSS3.SYMB:656`) — why the accessor limit is lower. (§1.3)
4. Exact on-disc layout and units of the `ACCTD` accounting directory (§6.3).
5. Confirmation (by live trace) that SYSTEM is user **1** and that project numbers
   partition the accounting file as the `UACCT` code implies.
</content>
</invoke>
