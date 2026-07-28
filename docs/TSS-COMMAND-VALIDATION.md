# NORD TSS 3.0 — command validation plan

**Status: EXECUTED 2026-07-25. RE-RUN IN FULL 2026-07-27 from a clean
bring-up, after the mode-6 assembler fix. Results in PART II; the re-run
delta is in PART V.**

> **CURRENCY WARNING — read this before trusting a transcript.** The PART II
> and PART V transcripts were captured **before** the NLZ/DNZ assembler fix
> (commit `01dce34`) and describe a binary that no longer exists. The sweep
> was re-run in full against the fixed binary — **PART VII holds the current
> results**. PART VI records the fix itself.

## Headline (2026-07-27 re-run — clean bring-up, 89 invocations)

| | |
|---|---|
| commands in `HELP` | **60** |
| distinct commands exercised | **60 (full coverage)** |
| return to the `@` prompt | **88 of 89 invocations** |
| **HANG — never return** | **1** — `LOAD-SYSTEM` |
| end the session by design | 1 — `LOGOUT` (signs off; **ESC** brings back `@ENTER`) |
| crashes | **0** — the emulator never died and the console never became unusable |

**Two entries below changed at the re-run.** `MEMORY <lower> <upper>` no
longer hangs — it was a `mac-c` code-generation bug, not a TSS bug (see
defect 1). And `LOGOUT` does not leave the terminal dead — **ESC** revives
it, which is the ordinary SINTRAN convention (see D2). The earlier
`ALREADY EXISTS` on `CREATE-USER` also does not reproduce once `SYSTEM`
holds quota, exactly as the D1 analysis predicted.

The 2026-07-25 figures were: 57 of 60 returning, **2** hangs
(`MEMORY <lower> <upper>` and `LOAD-SYSTEM`).

### Defects found

1. ~~**`MEMORY <lower> <upper>` hangs forever.**~~ **RESOLVED 2026-07-27 —
   this was never a TSS defect.** The swapper's page scan was miscompiled:
   `mac-c` encoded addressing mode 6 (`,I ,X`) as X-relative when it is
   *indirect P-relative indexed*, so nine backward references in TSS1's
   swapper addressed fixed words of code instead of `PDTBL[J]`. The scan
   therefore never saw a free frame, `IDXA`/`IDXB` stayed `-1`, and `SW6`
   fell into `FTLER`, killing the process. Full account in
   `/mnt/e/Dev/Ronny/TSS/docs/MAC-ASSEMBLER.md` §6.7. `MEMORY 40000 44000`
   now assigns the page and returns; `MEMORY 0` then lists it as assigned,
   `EXAMINE` reads it, and `DELETE-MEMORY` releases it.

   Worth recording *why* this stayed open for three sessions: the runtime
   measurement (`IDXA = IDXB = -1`, `J = 36`, 35 of 36 frames free) was
   correct all along and was repeatedly dismissed as impossible, because
   everyone reasoned from the **source** rather than checking that the
   **emitted code matched it**.
2. **`LOAD-SYSTEM` never returns.** Console shows the echo and nothing more —
   no reboot, no sign-on banner. Documented as "reloads (reboots) the TSS
   system from disk".
3. ~~**The time/date arithmetic is broken.**~~ **RESOLVED 2026-07-27 — this
   was never a TSS defect either, and the cause first recorded here was
   wrong.** The observed symptoms were real: after
   `DEFINE-DATE 26,7,2026,8,30,0` the date read `10 JULY 2026 23495:232`, the
   **day field changed on every read** (63 → 71 → 60 → 10 → 128 → 230 → 227),
   and `TIME-USED`, `RESPONSE-TIME` and `LOGOUT` printed **negative seconds**
   (`-26204 SECS`). The cause was one instruction word: `mac-c` classed
   `NLZ`/`DNZ` as plain 16-bit sums, so the negative scaling in `DNZ -20`
   borrowed out of the 8-bit scaling field and into the opcode —
   `0152000 - 0000020 = 0151760`, which **is `NLZ`**. The machine normalised
   where the program asked it to denormalise, at all twelve sites, every one
   of them `TBANG`'s float-to-integer step. Full account in
   `/mnt/e/Dev/Ronny/TSS/docs/MAC-ASSEMBLER.md` §6.8; verified end to end in
   PART VI. **The 32-bit/48-bit floating-point theory in PART III §3 is
   retracted** — it was labelled PROVEN and was not.
4. **`SET-REGISTER` sets the wrong register.** `SET-REGISTER A 1234` left
   `A = 0` and set `STS = 234`; `SET-REGISTER X 777` left `X = 0`. No error
   reported.
5. **`CREATE-USER` reports a false error.** On a disc holding only `SYSTEM`,
   `CREATE-USER TESTU` printed `ALREADY EXISTS` — and then created the user
   (`LIST-USERS` → `1 SYSTEM`, `2 TESTU`).
6. **No file can be created.** `OPEN-FILE "SCRATCH:DATA,WX` gives
   `BAD FILENAME` on **both** the command line and the prompt path;
   `ALLOCATE` gives `NO SUCH FILE`; `DUMP`, `SAVE-CORE`, `MAKE-REENTRANT`
   and `GET-CORE` all fail the same way. `DUMP` is *supposed* to create its
   output file. **No working file-creation route was found.**

### Confirmed working (a selection)

The user lifecycle is sound: `CREATE-USER` → `CREATE-FRIEND` →
`LIST-FRIENDS` → `DELETE-FRIEND` → `DEFINE-UIB-ACCESS` → `CLEAR-PASSWORD` →
`DELETE-USER`, with the user table verified back to `SYSTEM` only.
`RESERVE`/`RELEASE`, `PASSWORD`, `SYSDOWN`/`SYSUP`, `CLOCK-ON`/`CLOCK-OFF`,
`INIT-ACCOUNTING`, `DEFINE-VERSION`, `MODE`, `RESET` and `PAUSE` all behave.

`DISK-SPACE` reports `15 TRACKS (30K WORDS) LEFT OUT OF 4096` — an exact
independent match for `bringup/verify-disc.py`'s reading of the MIB. Disc
accounting is sound even though time accounting is not.

**`PAUSE` was predicted from source and confirmed live:** a bare CR escapes
it (`src/TSS4.SYMB:788-800`).

### Device names really are file names — confirmed live

Predicted from `OPEN` (§3a) and confirmed in two commands: `RBLOAD` accepted
`TAPE-READER` and read the paper tape (reaching `PROGRAM OUT OF BOUNDS`),
and `MODE` accepted `TELETYPE` for both its input and output files.

**But not universally.** `PLACE-BINARY` and `LOAD-BINARY` both *reject*
`TAPE-READER`, re-prompting and then reporting `BAD FILENAME`. Two commands
that call the same `OPEN` disagree — unexplained, and worth a source dive.

### Manual corrections required

| command | `TSS-USER-MANUAL.md` says | actual |
|---|---|---|
| `MEMORY` | "with no size it lists the page map" | prompts `LOWER BOUND:`; **`0` triggers the listing** (`JAZ M2`) |
| `STATUS` | order `P X T A D L STS B` | actual order `STS D P B L A T X` |
| `CONTINUE` | "returns silently" | prints `?` |
| `LINK-TO` | "fails silently" | prints `?` |
| `WHERE-IS` | prints `FREE TO USE` | **silent** for a free device |
| `LIST-FILE` | — | **silent** for a missing file, while `RENAME`/`DELETE-FILE`/`DEFINE-FILE-ACCESS` print `NO SUCH FILE` |
| `LIST-TRACKS` | "optional user name" argument | prompts `USER NAME:`; an empty answer is **not** "you" |
| `DELETE-MEMORY` | `DELETE-MEMORY (ADDRESS)…` | ignores the command-line argument and prompts anyway |
| `PLACE-BINARY` | argument: file name | ignores the command-line argument and prompts anyway |
| §5 scratch files | `OPEN-FILE "SCRATCH:DATA,WX` (marked *inferred*) | **disproven** — `BAD FILENAME` |
| §4 devices | listed only under `RESERVE`/`RELEASE`/`WHERE-IS` | **device names are valid file names** (§3a) |

Also established: the trailing **`?` is a failure-return indicator**, not
"unknown command" — it follows defined errors such as `ILLEGAL ADDRESS` and
`PROGRAM OUT OF BOUNDS`. And TSS's **full prompt table is `PARPT`/`SC1`–`SC36`**
(`src/TSS4.SYMB:1690-1731`), with `S[]` = string, `IB[]` = octal,
`ID[]` = decimal — which is why addresses are octal but dates decimal.

### Still open

- Why `LOAD-SYSTEM` does not come back.
- How a file is created on this system at all.
- Why `PLACE-BINARY`/`LOAD-BINARY` reject a device name that `RBLOAD` accepts.
- `LIST-ACCOUNTS LINE-PRINTER` produced no file in `--printdir`. Inconclusive:
  `INIT-ACCOUNTING` had just cleared the accounting file. Re-test in the
  other order.

Goal: log in as `SYSTEM` and exercise **all 60 command-processor commands**,
recording for each whether it *works*, *hangs*, or *crashes*, comparing the
observed behaviour against
[`TSS-USER-MANUAL.md`](TSS-USER-MANUAL.md) — which was written from the
source, not from a running system — and correcting the manual wherever
reality differs.

Full absolute paths used throughout:

| thing | path |
|---|---|
| repo | `/mnt/e/Dev/Ronny/TSS` |
| verified disc set (do not touch) | `/mnt/e/Dev/Ronny/TSS/Build/bringup/` |
| per-phase disposable copies | `/mnt/e/Dev/Ronny/TSS/Build/cmdtest/<phase>/` |
| results doc | `/mnt/e/Dev/Ronny/TSS/docs/TSS-COMMAND-VALIDATION.md` (this file) |
| manual under test | `/mnt/e/Dev/Ronny/TSS/docs/TSS-USER-MANUAL.md` |

---

## 1. Method

**The verified disc is never used directly.** `Build/bringup/` cost a format
plus a cold-start to produce and is the only known-good state. Each phase
runs on a fresh `cp -a` copy under `Build/cmdtest/`, so a command that
corrupts the disc costs one copy, not the baseline.

**One emulator run per phase.** A hang or a crash then invalidates only that
phase's remaining commands, not everything already recorded.

**Verdict definitions** — these must be mechanical, not judgement calls:

| verdict | test |
|---|---|
| **WORKS** | command echoes, produces output consistent with the manual, and the `@` prompt returns within 20 s |
| **WORKS-UNDOCUMENTED** | `@` returns, but output differs from the manual → manual is wrong, record both |
| **REJECTED** | TSS prints a defined error (`?`, `AMBIGUOUS`, `ILLEGAL ADDRESS`, `YOU MAY NOT DO THIS`, …) and `@` returns — a working refusal, not a failure |
| **HANG** | no new console bytes for 20 s **and** no `@` prompt |
| **CRASH** | emulator exits, CPU halts, or console dies permanently |
| **BLOCKED** | could not be reached because an earlier command in the phase hung/crashed |

Every command's **verbatim console transcript** is recorded, not a summary.

**Timing.** The emulated machine is slow: a command can take >4 s to answer.
The earlier probe read `WHO` as silent at 4 s and correct at 12 s. Per-command
wait is therefore 12 s, with the 20 s rule above reserved for the hang test.

---

## 2. Risk classification

Five commands can end or hijack the session and are deliberately last:

| command | why |
|---|---|
| `MODE` | redirects command **input and output** to files — can detach the console entirely |
| `PAUSE` | locks the terminal until the password is re-entered. Escape **verified in source** (`src/TSS4.SYMB:788-800`): a bare CR gives `PASSW=0`, SYSTEM's stored word is 0, so it matches and resumes. Not a hang. |
| `LOGOUT` | ends the session by design |
| `LOAD-SYSTEM` | reboots TSS from disk |
| `RECOVER` / `GOTO-USER` / `LOAD-BINARY` | transfer control to a loaded program; without a valid program the outcome is unknown |

Four are documented as build-conditional. This build is **NORD-10 + CDC disc
+ drum** (`Build/drum/tss-drum.bpun`), so the expectations are:

| command | manual says | expected here |
|---|---|---|
| `BACKUP` | NORD-1 only, no-op on NORD-10 | no-op |
| `PIN-DEVICE` | no-op on NORD-10 builds | no-op |
| `DEFINE-VERSION` | CDC-disc builds only | present and functional |
| `LOAD-SYSTEM` | CDC-disc builds only | present and functional |

---

## 3. The plan — one todo per command

`IN` = exactly what is sent. `EXPECT` = what
[`TSS-USER-MANUAL.md`](TSS-USER-MANUAL.md) says should happen; the test is
whether reality matches it.

### Phase 1 — read-only (19 commands, one login, no disc mutation)

- [ ] 1. `HELP` — IN `HELP` — EXPECT: all 60 command names, one per line
- [ ] 2. `DATE` — IN `DATE` — EXPECT: `DATE IS <date and time>`; must reflect the `25,07,2026,12,00,00` set at first login
- [ ] 3. `STATUS` — IN `STATUS` — EXPECT: saved register block `P X T A D L STS B`
- [ ] 4. `MEMORY` — IN `MEMORY` — EXPECT: page map listing each page `READ ONLY` / `READ/WRITE` / `EMPTY`
- [ ] 5. `EXAMINE` — IN `EXAMINE 1000` — EXPECT: octal contents of location 1000
- [ ] 6. `EXAMINE` (range) — IN `EXAMINE 1000 1010` — EXPECT: the whole range printed
- [ ] 7. `WHO-IS-ON` — IN `WHO-IS-ON` — EXPECT: `1  SYSTEM` (confirmed already)
- [ ] 8. `LIST-USERS` — IN `LIST-USERS` — EXPECT: user numbers + names; only `SYSTEM` on a fresh disc
- [ ] 9. `LIST-TRACKS` — IN `LIST-TRACKS` — EXPECT: `<n> TRACKS LEFT`
- [ ] 10. `TIME-USED` — IN `TIME-USED` — EXPECT: `TIME USED IS <t> OUT OF <limit>`
- [ ] 11. `DISK-SPACE` — IN `DISK-SPACE` — EXPECT: `<n> TRACKS (<k>K WORDS) LEFT OUT OF <total>`; cross-check against `verify-disc.py`'s 15 free tracks
- [ ] 12. `LIST-OBJECTS` — IN `LIST-OBJECTS` — EXPECT: object table: entry no., owner, file name, attribute words
- [ ] 13. `LIST-FRIENDS` — IN `LIST-FRIENDS` — EXPECT: friends of SYSTEM; empty on a fresh disc
- [ ] 14. `RESPONSE-TIME` — IN `RESPONSE-TIME` — EXPECT: `AVERAGE RESPONSE TIME IS <n.n> SECONDS OVER A PERIOD OF <t>`
- [ ] 15. `WHERE-IS` — IN `WHERE-IS TELETYPE` — EXPECT: `FREE TO USE` or `RESERVED BY USER <name>`
- [ ] 16. `LIST-FILE` — IN `LIST-FILE SCRATCH` — EXPECT: an error — `SCRATCH:DATA` does not exist on a fresh disc. **Also settles manual §5's open question: is it `AMBIGUOUS FILENAME` (err 31) or `NO SUCH FILE` (err 8)?**
- [ ] 17. `CONTINUE` — IN `CONTINUE` — EXPECT: returns silently (nothing suspended)
- [ ] 18. `BREAK-LINKS` — IN `BREAK-LINKS` — EXPECT: returns silently (no link established)
- [ ] 19. `LINK-TO` — IN `LINK-TO SYSTEM` — EXPECT: fails silently (linking to self)

### Phase 2 — reversible state changes (17 commands, fresh disc copy)

Ordered so each mutation is undone by a later command in the same phase.

- [ ] 20. `RESERVE` — IN `RESERVE TELETYPE` — EXPECT: silent success
- [ ] 21. `WHERE-IS` (after reserve) — IN `WHERE-IS TELETYPE` — EXPECT: now `RESERVED BY USER SYSTEM` — proves 20 took effect
- [ ] 22. `RELEASE` — IN `RELEASE TELETYPE` — EXPECT: silent success; re-check with `WHERE-IS` → `FREE TO USE`
- [ ] 23. `OPEN-FILE` — IN `OPEN-FILE "SCRATCH:DATA,WX` — EXPECT: `FILE NUMBER = <n>`. **Tests the manual's inferred create incantation (§5) — currently marked "not yet confirmed on a running system".**
- [ ] 24. `LIST-FILE` (after create) — IN `LIST-FILE SCRATCH` — EXPECT: directory entry now exists
- [ ] 25. `CLOSE-FILE` — IN `CLOSE-FILE <n from 23>` — EXPECT: silent success
- [ ] 26. `DEFINE-FILE-ACCESS` — IN `DEFINE-FILE-ACCESS SCRATCH:DATA 777` — EXPECT: sets the 9-bit protection word
- [ ] 27. `RENAME` — IN `RENAME SCRATCH:DATA TESTF:DATA` — EXPECT: silent success; confirm with `LIST-FILE TESTF`
- [ ] 28. `DELETE-FILE` — IN `DELETE-FILE TESTF:DATA` — EXPECT: silent success; disc returns to its pre-phase state
- [ ] 29. `SET-REGISTER` — IN `SET-REGISTER A 1234` — EXPECT: silent success
- [ ] 30. `STATUS` (verify) — IN `STATUS` — EXPECT: `A` now reads `001234` — proves 29 took effect
- [ ] 31. `MEMORY` (assign) — IN `MEMORY 1 40000` — EXPECT: assigns a page; bad address → `ILLEGAL ADDRESS SPACE`
- [ ] 32. `DELETE-MEMORY` — IN `DELETE-MEMORY 40000` — EXPECT: frees that page; confirm with `MEMORY`
- [ ] 33. `CREATE-USER` — IN `CREATE-USER TESTU` — EXPECT: `USER NUMBER = <n>`
- [ ] 34. `LIST-USERS` (verify) — IN `LIST-USERS` — EXPECT: `TESTU` now listed
- [ ] 35. `CREATE-FRIEND` — IN `CREATE-FRIEND TESTU` — EXPECT: silent success
- [ ] 36. `LIST-FRIENDS` (verify) — IN `LIST-FRIENDS` — EXPECT: `TESTU` listed
- [ ] 37. `DELETE-FRIEND` — IN `DELETE-FRIEND TESTU` — EXPECT: silent success
- [ ] 38. `DEFINE-UIB-ACCESS` — IN `DEFINE-UIB-ACCESS (TESTU) 777` — EXPECT: sets the 9-bit access word
- [ ] 39. `CLEAR-PASSWORD` — IN `CLEAR-PASSWORD TESTU` — EXPECT: silent success (SYSTEM-only path exercised)
- [ ] 40. `TRANSFER` — IN `TRANSFER` then prompts `TO USER:` `TESTU`, `FROM USER:` `SYSTEM`, `NUMBER OF TRACKS:` `1` — EXPECT: quota moves; confirm with `LIST-TRACKS TESTU`
- [ ] 41. `DELETE-USER` — IN `DELETE-USER TESTU` — EXPECT: silent success; `LIST-USERS` back to just `SYSTEM`

### Phase 3 — system administration (10 commands, fresh disc copy)

- [ ] 42. `CLOCK-OFF` — IN `CLOCK-OFF` — EXPECT: silent success (SYSTEM only)
- [ ] 43. `DATE` (after clock off) — IN `DATE` — EXPECT: reveals whether the clock actually stopped
- [ ] 44. `CLOCK-ON` — IN `CLOCK-ON` — EXPECT: silent success; `DATE` advances again
- [ ] 45. `DEFINE-DATE` — IN `DEFINE-DATE` then `DAY:`…`SECOND:` = `26,07,2026,08,30,00` — EXPECT: prompts in sequence, clock set; verify with `DATE`
- [ ] 46. `SYSDOWN` — IN `SYSDOWN` — EXPECT: silent success; system now unavailable to *new* users (our session continues)
- [ ] 47. `SYSUP` — IN `SYSUP` — EXPECT: silent success; restores availability
- [ ] 48. `INIT-ACCOUNTING` — IN `INIT-ACCOUNTING` — EXPECT: clears the accounting file. **Destructive to accounting data — hence a disposable disc**
- [ ] 49. `LIST-ACCOUNTS` — IN `LIST-ACCOUNTS LINE-PRINTER TESTRUN` — EXPECT: writes `TIMESHARING SYSTEM USAGE REPORT` to the printer device; collected by nd100x `--printdir`. Exercises the device-as-file route on the OUTPUT side
- [ ] 50. `ALLOCATE` — IN `ALLOCATE ABSF:DATA,4500,1` — EXPECT: allocates a fixed-location file over specific tracks. **Changes disc layout**
- [ ] 51. `BACKUP` — IN `BACKUP` — EXPECT: **no-op** on this NORD-10 build; any `DISK ERROR AT <addr>` output is a finding

### Phase 4 — program load / control (9 commands, fresh disc copy)

These need a loadable program. `Build/minit/minit.bpun` is a real BPUN and is
the natural test payload.

- [ ] 52. `PLACE-BINARY` — IN `PLACE-BINARY TAPE-READER` — EXPECT: reads the BPUN off the emulated paper tape (`--tape=…/minit.bpun`), loads it **without** starting, returns the start address (MINIT's entry is `000206`); or `PROGRAM OUT OF BOUNDS` / `CHECKSUM ERROR`
- [ ] 53. `DUMP` — IN `DUMP DMP:DATA` — EXPECT: writes the whole address space with a dump marker
- [ ] 54. `MAKE-REENTRANT` — IN `MAKE-REENTRANT DMP:DATA (1000)` — EXPECT: marks a block reentrant; `NOT A DUMP FILE` if 53 failed
- [ ] 55. `SAVE-CORE` — IN `SAVE-CORE CORE:DATA,1000,2000` — EXPECT: saves that range; start > end → `ILLEGAL ADDRESS`
- [ ] 56. `GET-CORE` — IN `GET-CORE CORE:DATA,1000,2000` — EXPECT: reloads the range saved by 55
- [ ] 57. `RBLOAD` — IN `RBLOAD RB:DATA,20000` — EXPECT: `ILLEGAL CONTROL BYTE` or `CHECKSUM ERROR` if no RB-format file exists — a defined rejection is a pass
- [ ] 58. `RESET` — IN `RESET` — EXPECT: releases the entire address space; confirm with `MEMORY` → all `EMPTY`
- [ ] 59. `PIN-DEVICE` — IN `PIN-DEVICE 1` — EXPECT: **no-op** on NORD-10
- [ ] 60. `GOTO-USER` — IN `GOTO-USER 177777` — EXPECT: `ILLEGAL ADDRESS`. Deliberately an invalid address: a *valid* one transfers control and ends the session

### Phase 5 — session-ending / hijacking (5 commands, one disc copy each)

Each gets its own emulator run because each may not return.

- [ ] 61. `LOAD-BINARY` — IN `LOAD-BINARY TAPE-READER` — EXPECT: loads from paper tape **and starts** it; MINIT takes over the console and prints `MASS STORAGE INIT`. Own run — do NOT answer its prompts, this is a load test not a format.
- [ ] 62. `RECOVER` — IN `RECOVER DMP:DATA` — EXPECT: restores the address space and resumes; `NOT A DUMP FILE` if the marker is wrong. Own run.
- [ ] 63. `DEFINE-VERSION` — IN `DEFINE-VERSION` then `VERSION (A OR B):` `A` — EXPECT: selects the LOAD-button coreload. Own run (changes what a front-panel load does).
- [ ] 64. `LOAD-SYSTEM` — IN `LOAD-SYSTEM` — EXPECT: reboots TSS from disc — session ends, sign-on banner should reappear. Own run.
- [ ] 65. `MODE` — IN `MODE IN:DATA OUT:DATA` — EXPECT: command I/O redirects to files; **the console may go silent by design**. Own run, last but one.
- [ ] 66. `PAUSE` — IN `PAUSE`, then at `PASSWORD IS` send a bare CR — EXPECT: terminal locks, then resumes to `@` on the empty line. Escape verified in source (`src/TSS4.SYMB:788-800`): chars accumulate as `PASSW<<3+char` until CR, compared against `USRTB+7`; empty = 0 = SYSTEM's stored word. Own run anyway.
- [ ] 67. `LOGOUT` — IN `LOGOUT` — EXPECT: `TIME USED IS <t> OUT OF <total>`, files closed, terminal idle, sign-on returns. Runs last.

> Numbering exceeds 60 because `EXAMINE`, `WHERE-IS`, `STATUS`, `LIST-USERS`,
> `LIST-FILE`, `LIST-FRIENDS` and `DATE` each appear more than once — as the
> command under test and again as the *verifier* that a preceding mutation
> took effect. All 60 distinct commands are covered exactly once as a subject.

---

## 3a. RESOLVED: how a program gets in — device names ARE file names

**Verified in the source, 2026-07-25.** `OPEN` (`src/TSS3.SYMB:1345-1348`,
label `O4`) matches the operand against the peripheral-name table `PDEVS`
**before** trying the file path:

```
O4,   LDX PTR,B; LDT (STARR; SAA 1; COPY DD SA
      LDA (PDEVS; JPL I (ABLKP; JMP O4A     % no match -> ordinary file path
      SHA 1; ADD (DDEVS; COPY DX SA
      LDA 0,X; STA AREG,B; JMP I (OSS        % match  -> open the DEVICE
```

On a match it indexes `DDEVS` for the device number and returns open-success;
only on no-match does it fall through to `O4A` and parse the name as a file
(`"` create marker, `(USER)` group, `:TYPE`). `PLACE-BINARY` calls this same
`OPEN` (`LDA (MTP1; SAT 1; JPL I (OPEN`) and then reads with `INBT`.

**Consequence: a peripheral device name is valid anywhere a file name is
expected.** Device table (`src/TSS2.SYMB:1837-1856`): `TELETYPE`=1,
`TAPE-READER`=2, `FAST-PUNCH`=3, `CARD-READER`=4, `LINE-PRINTER`=5,
`DIABLO`=14, `NULL`=0.

nd100x provides the host side: `--tape=FILE` (paper tape reader input,
`.bpun`), `--tapedir=DIR` (punch output), `--printdir=DIR` (printer output).

So Phase 4/5 emulator runs add:

```
--tape=/mnt/e/Dev/Ronny/TSS/Build/minit/minit.bpun
```

and items 52 / 61 use `TAPE-READER` as the file name. Nothing has to be
pre-planted on the TSS disc.

**This is missing from `TSS-USER-MANUAL.md`** — §4.2 describes `OPEN-FILE` as
opening a file, and §4.4 lists the devices only under
`RESERVE`/`RELEASE`/`WHERE-IS`/`PIN-DEVICE`. That devices double as file names
is undocumented and belongs in the manual regardless of how the run goes.

## 4. Open questions

Both original blockers are now resolved from the source: how a program gets
in (§3a, device names are file names) and whether `PAUSE` can be escaped
(§2, bare CR). One remains, and it can only be answered by running:

1. **Argument syntax is inferred in several places** — the manual gives the
   shape (`ALLOCATE FILENAME,TRACK-ADDRESS,NUMBER-OF-TRACKS`) but not a worked
   example. Where a command rejects my syntax, that is a *manual* gap to
   record, not a command failure.

---

## 5. Deliverables

1. This document, with every checkbox resolved and each command's verbatim
   console transcript.
2. A summary table: 60 commands × verdict.
3. **Corrections to `/mnt/e/Dev/Ronny/TSS/docs/TSS-USER-MANUAL.md`** wherever
   observed behaviour contradicts it — in particular the §5 items currently
   marked *(inferred)* or *not yet verified live*.
4. A list of anything found that hangs or crashes, with the reproduction.

## 6. Estimated cost

~67 command invocations at 12 s, plus 8 emulator boots at ~40 s each ≈
**20–25 minutes of wall clock**, spread over 8 runs.

---

# PART III — ROOT CAUSES

Investigated 2026-07-25 after the sweep. Each entry separates what is
**proven** (read from source, or measured) from what is **inferred**.

## Summary

| # | defect | root cause | is it a TSS bug? |
|---|---|---|---|
| 1 | `MEMORY <lo> <hi>` hangs | **CLOSED** — `mac-c` emitted mode 6 (`,I ,X`) as X-relative; nine backward refs in the swapper addressed code, not `PDTBL[J]` | **no** — assembler bug |
| 2 | `LOAD-SYSTEM` never returns | works as designed; the working disc carries no bootstrap | **no** — build-variant gap |
| 3 | date/time garbage | **CLOSED** — `mac-c` classed `NLZ`/`DNZ` as plain sums, so `DNZ -20` borrowed into the opcode and assembled as `NLZ` | **no** — assembler bug |
| 4 | `SET-REGISTER` no effect | `"N10` variant drives **live** CPU registers, not the saved block | **yes** (plus an off-by-one) |
| 5 | `CREATE-USER` false error | all `CRUSE` failures wired to the `ALREADY EXISTS` label | **yes** |
| 6 | no file can be created | create marker is a **quoted pair**; then blocked on track quota | **no** — doc error + provisioning |

## 3. Date/time — RETRACTED. The cause was an assembler bug, not FP width

> **[RETRACTION 2026-07-27]** This section was headed **[PROVEN]** and was
> **wrong**. The real cause is in `mac-c`: `NLZ`/`DNZ` were classed as plain
> 16-bit sums, so the negative scaling in `DNZ -20` borrowed out of the 8-bit
> scaling field into the opcode and assembled as `NLZ` (`0152000 - 020 =
> 0151760`). ND-60.096.01 §3.2.2.4 states the correction explicitly ("Bit 7
> is now examined and it is 1, so 400 is added"). All twelve affected sites
> are `TBANG`'s float-to-integer step. See
> `/mnt/e/Dev/Ronny/TSS/docs/MAC-ASSEMBLER.md` §6.8 and PART VI below.
>
> **FP width was ruled out by experiment**, not by argument. nd100x gained
> `--fpp=32|48`; both widths produced the same garbage over six paired runs.
> TSS in fact *requires* 48-bit: `NORM` builds an `040000`-biased exponent in
> T, and `FPDAT` unpacks year and month out of that T word.
>
> **Why the old reasoning looked airtight and still failed.** Every bullet
> below is individually true. The partition argument — "integer fields print
> correctly, floating-point fields are garbage, so the fault is in the FP" —
> is the one that misled: `DNZ` sits exactly on that boundary, so a bug in
> the *instruction* mimics a bug in the *format*. A correct correlation is
> not a cause. What settled it was patching a single constant in memory and
> watching one variable, after a full `-F48` rebuild had produced a **false
> negative** by shifting every address past the first `[`.
>
> One thing in the old text survives and is still true: TSS's `[` constants
> are 2-word (32-bit) while every consumer is 48-bit. That is **historical,
> not ours** — the golden dump gives `RDATE=022700`, and 3-word constants
> would put it at `022704`. It cannot be corrected without diverging from the
> archive. It is invisible at small elapsed times, but it means the day and
> hour counters can never advance. Untested; recorded in the handoff.

### (superseded analysis follows)

`TBANG` (`src/TSS2.SYMB:1123`) is the **only floating-point code in TSS**. It
converts the tick counter using four `[` constants:

```
K1, [4.32E6   ticks/day     K2, [1.8E5   ticks/hour
K3, [3000     ticks/minute  K4, [50      ticks/second
```

- **Proven:** the archived golden dump shows `TBANG..RDATE` spanning **55
  words**, which only balances if K1–K4 are **2 words each** (4x2=8, not
  4x3=12) — recorded in `mac-c/src/mac_stmt.c:600-608`. The 1978 build was
  therefore a **32-bit-float** system, where `STF`/`LDF` move 2 words and the
  `DATA TIME,3` buffers simply carry slack.
- **Measured, with a caveat:** no 32-bit FP option was found in this
  nd100x build
  (`~/repos/nd100x/src/cpu/float.c`: "48-bit floating point arithmetic for
  the ND-100"), with no CPU-model dependence — `--cputype` cannot change it.
- **Proven by partition:** in `RDATE`, year and month are produced by
  **integer** arithmetic and print **correctly**; day, hour, minute and
  second come from `TBANG`'s **floating-point** results and are garbage. The
  failure follows the FP boundary exactly.
- **Ruled out:** clock speed. Re-running with `--throttle` (CPU at real-time
  0.5275 MHz) produced the same garbage (`64 JULY` -> `120 JULY` -> `30 JULY`).

This single cause explains all four symptoms — `DATE`, `TIME-USED`,
`RESPONSE-TIME` and `LOGOUT` all route through `TBANG`/`FOTIM`.

**Fix options:** run TSS on a 32-bit-FP machine model, or add 32-bit FP to
nd100x, or rebuild TSS with `mac_state.float48` so the constants match the
emulated FP (this shifts addresses and breaks the golden-dump match, so it is
a runtime-only variant).

## 2. `LOAD-SYSTEM` — works as designed; the DISC has no bootstrap  **[PROVEN]**

> **[CORRECTION 2026-07-26]** This section previously said the command
> "needs front-panel LOAD hardware nd100x does not provide". **That was
> invented and is retracted.** No front-panel hardware is involved.

`SDISK`'s contract (`src/TSS1.SYMB:3955`) is
`%A = OPERATION (1=READ, 3=WRITE)  %X = CORE ADDRESS  %T = DISK ADDRESS`, and
`LOAD-SYSTEM` calls it as:

```
L2,	SAX 0; SAT 0; SAA 1; JPL I (SDISK   % X=0, T=0, A=1 -> READ disc page 0 into core 0
	IOF; RCLR DP                        % I/O off; RCLR DP clears P -> execute from address 0
```

`RCLR DP` is simply "clear the P register", so the next instruction is
fetched from location 0. It is a **self-contained software reboot**: load the
boot page from disc, jump to it.

Disc page 0 is supposed to hold **`DBOOT`** (`src/TSS3.SYMB:265`), which
reloads the coreloads and enters `INIT`:

```
DBOOT, STZ DBCOR; LDA (CORLD; STA DBDKA
DBLOP, LDX DBCOR; LDT DBDKA; JPL RDKOP
   MIN DBDKA; LDA (400; ADD DBCOR; STA DBCOR
   SUB (MSTRT; JAN DBLOP; JMP I (301
```

**Measured:** sector 0 of `Build/bringup/cdc.img` is **512 bytes of zeros**.
So `LOAD-SYSTEM` loads zeros into core 0 and executes them with interrupts
already disabled — hence the silent wedge.

**Why page 0 is empty — the bootstrap is compiled out, twice.** Mark regions
in `src/TSS3.SYMB`:

```
239: "CDC NMACF     <- contains DKRST, RDKOP, DBOOT (the disc bootstrap)
275: "
277: "TSBIN         <- defines CDBIN
280: "NCR           <- )KILL CDBIN
284: "CDBIN         <- the two )8DUMP writes, to disc pages 0 and 2
297: "
```

Our builds set `CDC MACF DIAB K14 TEL4` (+ `DRUM N10` for the drum variant):

1. `"CDC NMACF` is **FALSE**, because `MACF` *is* set — so `DKRST`, `RDKOP`
   and `DBOOT` are **never assembled**. The bootstrap code is not in the
   image. (Their absence from the symbol dump is explained by this, not by
   the `)PCL DBOOT` purge.)
2. `CDBIN` is defined only inside `"TSBIN`, which we do not set — so the two
   `)8DUMP` writes to disc pages 0 and 2 are compiled out as well.

**The archived 1978 golden builds also set `MACF`.** The system this project
reproduces is therefore the **tape-loaded** variant, not the
self-booting-disc variant. `LOAD-SYSTEM` is the only command that assumes the
other build.

**Classification: not a defect at all — a build-variant mismatch.** Nothing in `bringup/` or the
Makefile writes a disc bootstrap, which is and `)8DUMP` is in any case an intentional no-op in mac-c because it needs
real ND-100 execution. The command itself is correct and would work on a disc
built from the `NMACF` + `TSBIN`/`CDBIN` variant.

**To make `LOAD-SYSTEM` work** you would either build that variant and
implement `)8DUMP` in mac-c, or synthesise pages 0 and 2 host-side from a
build that does assemble `DKRST`/`DBOOT`.

`LOADV` (`src/TSS5.SYMB:609`) deliberately stops the machine and restarts it
from location 0:

```
	IOF                            % I/O off
	LDA (*+6; IRW 0 DP; POF        % paging off
	SAA -1; MCL PIE; INTEN; INTDS
L2,	SAX 0; SAT 0; SAA 1; JPL I (SDISK
	IOF; RCLR DP                   % clear P -> restart at 0
```

`RCLR DP` clears the P register, handing control to the machine's front-panel
LOAD/bootstrap logic. nd100x provides no such hardware, so the CPU runs into
cleared memory and stops. Not a TSS defect.

## 4. `SET-REGISTER` / `STATUS` — RETRACTED AND REVISED

> **[RETRACTION 2026-07-25]** This section previously claimed the `"N10`
> variant drives the **live** CPU registers instead of a saved block. **That
> is wrong.** `IRR`/`IRW` bits 3-6 are a **program level** field
> (ND-06.014.2A, ND-100 Reference Manual: *"Bits 3-6 specify the program
> level number"*). TSS assembles `ORA (IRR 10` / `ORA (IRW 10`, and
> `0153600 + 010 = 0153610` decodes to **level = 1** — the user program's
> saved register block. That is the correct intent and the exact equivalent
> of the NORD-1 variant's `RBLOK`. The original analysis is withdrawn.

> **[SECOND RETRACTION 2026-07-26]** The ring/privilege explanation below is
> also **wrong**. Measured at runtime, **every program level reports
> `Ring[3]`**, which passes nd100x's `CheckPriv()`. Privilege gating is
> therefore not why `IRR`/`IRW` appear inert. **Defect 4's mechanism is
> currently UNEXPLAINED.** The `SSXT` index-7 overrun below remains a real,
> independent bug.

**Superseded explanation [WRONG]:** `IRR`/`IRW` are
**privileged**. nd100x gates them on the **ring field of PCR**
(`CheckPriv()`; `cpu_instr.c:3713` — *"Real PCR content is ring (0-1), the
MMS2 enable (2) and PT/APT (7-14)"*), requiring ring 2 or 3. Rings are an
**ND-100** protection concept; TSS 3.0 is a **NORD-10** program that drives
the MMU the Paging-System-I (MMS1) way and has no notion of them. If it
leaves the ring field at 0, every `IRR`/`IRW` silently becomes a no-op —
which is exactly what was measured (reads return nothing, writes never
stick, no error reported).

If that holds, defect 4 is an **ND-10 vs ND-100 architecture difference**,
not a TSS bug. **To verify:** read `STS`/`PONI` and `PCR[CurrLEVEL]` at the
moment `STATUS` executes and check the ring bits.

**Still a genuine bug, independent of the above:** the `B` decode overruns
the table. `STATUS`'s `REGM` gives the machine numbering
`0=STS 1=D 2=P 3=B 4=L 5=A 6=T 7=X`; `SETX`'s `SSXT, 2; 7; 6; 5; 1; 4; 3`
has **seven entries, indices 0-6**, with `B` at index 6 — but the decoder
does `SAT ##B; SKP IF DA UEQ ST; SAX 7`, index **7**, one past the end.
Fix: `SAX 6`.

### (superseded analysis follows)

Two independent bugs, both in the `"N10` conditional block:

**(a) Live registers instead of the saved block.** The `"NN10` (NORD-1)
variant reads and writes `RBLOK`, the user's **saved** register block. The
`"N10` variant instead executes `IRR`/`IRW` against the **live CPU
registers**:

```
"N10
   LDA CNT,B; ORA (IRR 10; EXR SA        % REGS  (STATUS)
   LDA SSXT,X; ORA (IRW 10; STA ADR,B    % SETX  (SET-REGISTER)
```

So `STATUS` reports the command interpreter's own registers (hence all zeros)
and `SET-REGISTER`'s write is destroyed the moment it returns. Measured:
`P`, `T`, `B` and `X` all had no effect; `A 1234` left a transient
`STS = 234`.

**(b) Table overrun for `B`.** `STATUS`'s own `REGM` table gives the machine
register numbering `0=STS 1=D 2=P 3=B 4=L 5=A 6=T 7=X`. `SETX`'s translation
table is `SSXT, 2; 7; 6; 5; 1; 4; 3` — **seven entries, indices 0-6**, with
`B` correctly at index 6. But the letter decoder assigns
`SAT ##B; SKP IF DA UEQ ST; SAX 7` — index **7**, one past the table.
`SET-REGISTER B` therefore builds its `IRW` from whatever word follows
`SSXT`. Fix: `SAX 6`.

## 5. `CREATE-USER` — every failure reported as ALREADY EXISTS  **[PROVEN]**

`CRUSE` (`src/TSS4.SYMB`) documents three distinct failure codes:

```
%A = 1 IF NO MORE TRACKS   %A = 2 IF TOO MANY USERS   %A = 3 IF USER ALREADY EXISTS
```

and allocates the new user's track with `JPL I (GTRK; JMP CF1`. But the
caller `CRUSR` discards `A` and sends the failure return to a single label:

```
	JPL I (CRUSE; JMP CF3
CF3,	LDX (MS3; JMP CFF        % MS3 = 'ALREADY EXISTS'
```

So a **track-allocation failure prints ALREADY EXISTS**. The user-table entry
is written before the allocation, which is why `TESTU` both errored and
appeared in `LIST-USERS`.

## 6. File creation — a documentation error over a provisioning gap  **[PROVEN]**

**The create marker is a quoted PAIR, not a leading quote.** In `OPEN`
(`src/TSS3.SYMB`):

```
O16,  LDA NEWF,B; JAP *+2; JMP I (OF8   % name ended normally + create -> BAD FILENAME
O15,  LDA NEWF,B; JAZ *+2; JMP O17      % second '"' seen + create -> SUCCESS path
```

`O15` is reachable only when a **second `"`** appears. Measured:
`OPEN-FILE "SCRATCH:DATA,WX` -> `BAD FILENAME`, but
`OPEN-FILE "SCRATCH:DATA",WX` -> **`NO MORE TRACKS AVAILABLE`** — a different
error, proving the create path was reached.

The remaining blocker is **track quota**, not syntax: `LIST-TRACKS SYSTEM`
reports `0 TRACKS LEFT`, and `TRANSFER` refuses for the same reason.

Also recovered: `OPEN`'s error codes — `OF8` = `57`, `OFX` = `10` (NO SUCH
FILE) or `37` = **31 decimal = AMBIGUOUS FILENAME**, which is exactly the
error `LOGON` reports on a fresh disc.

## 1. The MEMORY hang — CLOSED: swapper finds no free page -> FTLER  **[PROVEN by memory read]**

> **Correction history:** this section first named the `PDTBL[..]$COUNT<=0`
> guard, was then wrongly retracted on the basis of a flat-`.img` lookup
> (the image file is **not** 1:1 with memory, and `PC` is a *virtual*
> address), and is now re-established by reading memory **through the MMU**
> and decoding the instructions. The call site is `SW6`, not the `PDTBL`
> guard.

**Measured at the hang** (DAP, level-5 context):

```
P = 000013   L = 006764   A = 177777 (-1)   PIL = 5   PVL = 6
000013: 124000      <- JMP *   (FTLER)
000014: 124000      <- JMP *   (TRERR)
```

`124000` is `JMP` with displacement 0 — jump-to-self — matching
`FTLER, JMP *` / `TRERR, JMP *` (`src/TSS1.SYMB:165`). Confirmed by reading
through the MMU, not from the image file.

**The call site, decoded from memory at `L-1`:**

```
006757: 044342   LDA IDXA
006760: 130004   JAP SWP6
006761: 044341   LDA IDXB
006762: 130002   JAP SWP6
006763: 135330   JPL I 9FTLE     (opcode 23 = JPL, indirect)
006764: 004332                    <- L (return address) points here
```

instruction-for-instruction identical to

```
SW6,	LDA IDXA; JAP SWP6; LDA IDXB; JAP SWP6; JPL I 9FTLE
%       FTLER() IF IDXA<0 AND IDXB<0
```

and corroborated by `A = 177777` — `IDXB` still holding the `-1` that `SW4`
initialised it to.

**The chain:** `MEMORY <lo> <hi>` -> `CKMEM` -> `CRMEM` writes the `VCTBL`
entry and posts `SAA 40; MST PID` to level 5 -> the swapper runs -> `SW4`
scans `PDTBL` for a physical page to back the new virtual page -> **none is
found**, so `IDXA` and `IDXB` both stay `-1` -> `SW6` calls `FTLER` ->
`JMP *`.

**Why the whole system dies, not just the process:** `PIL = 5`. The spin is
at interrupt **level 5**, which blocks every lower level including the
command processor — hence the permanently dead console. (`JMP *` is a
preemptible idle spin, the ND idiom for "stop this program level"; higher
levels still run.)

**Open — and now BLOCKED on tooling.** Why the scan finds no free page is
**not** explained. Investigated 2026-07-26:

*Measured at the hang (all read through the MMU):*

| fact | value |
|---|---|
| `IDXA` (`006721`), `IDXB` (`006722`) | both `177777` (-1) — hence the trap |
| `J` (`006723`) | `000044` = 36 = `NDPGS` — **the scan ran to completion** |
| `J2` (`006724`) | `000106` = 2x35 — indices were computed correctly |
| `PDTBL` frames | **35 of 36 have `W0 = 0`** — abundant `IDXA` candidates |
| `X` at trap | `014607` = `PDTBL` — correct table base |
| pointers | `9VCTB`@`006704`=`015063`, `9PCTB`@`006705`=`014577`, `9PDTB`@`006706`=`014607` |

Those facts are **mutually contradictory** under `SW5` as written: with 35
free frames the very first iteration should have latched `IDXA`.

*Hypotheses tested and eliminated:*

1. `PDTBL[..]$COUNT<=0` guard — wrong call site; the trap is `SW6`.
2. Physical page pool exhausted — refuted, 35 of 36 frames free.
3. Track/quota exhaustion — refuted, unrelated to `PDTBL`.
4. nd100x computing pre-indexed instead of post-indexed indirect — refuted:
   `cpu.c:316` is `eff_addr = gX + ReadIndirectVirtualMemory(...)`, correct.
5. mac-c mis-encoding the addressing mode — refuted: the instruction decodes
   to mode 6 = `((P)+disp)+(X)`, exactly right.

*Why it is blocked:* locating `SW4`/`SW5`/`SW6` in memory requires mapping
source lines to addresses, and **`mac-as` cannot emit an address listing** —
`-l` writes the *symbol* list only. Every hand-mapping attempt (P-relative
displacement arithmetic) produced a wrong address; a breakpoint placed at the
computed "SW5" landed in an unrelated loop whose indirect word held `172776`,
an instruction word rather than an index.

**Unblocking step:** add a listing mode to `mac-as` emitting
`address | emitted words | source line`. With that, `SW4`/`SW5`/`SW6` can be
located exactly, a breakpoint set on the real scan, and the loaded value
observed per iteration. Without it this question should not be answered by
further inference.

## 1b. (superseded analysis)

The chain, every step evidenced:

1. `MEMORY <lo> <hi>` -> `MEM` -> `CKMEM` (`src/TSS2.SYMB:458`), which calls
   `CRMEM` for each absent page.
2. `CRMEM` (`src/TSS2.SYMB:489`) writes the page-table entry
   **`VCTBL[i] = 160000`** — X bit set, and the INDX field
   (`160000 AND 1777`) = **0** — then posts `SAA 40; MST PID`, an interrupt
   request to **level 5**.
3. Level 5 runs the swapper `SWAPR` (`src/TSS1.SYMB:2794`). Its `SW1` page
   loop reaches this guard:

   ```
   %  FTLER() IF PDTBL[VCTBL[I]$INDX]$COUNT<=0;
      AND K1777; SHA 1; ADD 9PDTB; COPY DX SA
      LDA 0,X; AND K77; JAF *+2; JPL I 9FTLE
   ```

4. It reads `PDTBL[0]` — the entry the INDX field points at — whose usage
   count is **0**, because `CRMEM` never allocated a physical page to back
   the entry it just created. The guard fires.
5. `FTLER` is at **address 000013**, labelled `%FATAL ERROR` in the source,
   and its entire body is **`FTLER, JMP *`** (`src/TSS1.SYMB:165`), as is
   `TRERR` beside it. Mechanically `JMP *` is a **preemptible idle spin**,
   not a CPU halt: higher-level interrupts still run, and the level resumes
   the same instruction afterwards. It is the ND idiom for "stop this
   program level". **Open:** which level it spins on. If it is level 5 (the
   swapper's level) it blocks every lower level, which would explain the
   totally dead console; if lower, only the user's process is stuck.

**Every observation is accounted for:**

| observation | explained by |
|---|---|
| no console output whatsoever | `FTLER` prints nothing |
| PC frozen at `000013` across 30 single-steps | `JMP *` never advances |
| caller frame `006762` = `SWAPR`+207 | the swapper's `SW1` loop |
| `drum.img` / `cdc.img` unmodified | it traps *before* any `TRSFR` I/O |
| terminal dead permanently | nothing ever clears the loop |

**This is a real TSS bug, and it is NOT the track-quota condition** — the
earlier inference is retracted. `CRMEM` marks a `VCTBL` entry as present
(X bit) with `INDX = 0` while leaving `PDTBL[0]` unbacked, which the swapper
treats as a fatal inconsistency.

**Why login still works:** the overlay path (`GOVER`/level 5) uses `VCTBL`
entries that *do* have `PDTBL` backing. Only `CRMEM` — reached solely by the
assigning form of `MEMORY` — creates the malformed entry. That is why the
system boots, logs in and runs 59 of 60 commands, and dies only here.

## 1b. Earlier partial analysis (superseded)

**Proven by measurement.** Pausing the CPU over DAP four times, seconds
apart, gives an identical stack every time:

```
PC 000013 , caller 006762
```

`006762` is **`SWAPR`+207** (`SWAPR` = `006443` in
`Build/drum/DSYMB.SYMB`) — the **swapper**. The CPU is in a tight loop there
and never leaves.

**Proven by source.** `MEM` -> `CKMEM` (`src/TSS2.SYMB:458`) -> `CRMEM`
(`src/TSS2.SYMB:489`), which writes a page-table entry and then posts
`SAA 40; MST PID` — an interrupt request to **level 5**, the resident page
reader. Both loops in `CKMEM` terminate by inspection, so the wedge is on the
level-5 / swapper side, not in the command.

**Proven by measurement.** Neither `drum.img` nor `cdc.img` was modified
during the hang. The swapper therefore spins **before performing any I/O** —
it is waiting on a resource, not on a missing completion interrupt.

**Inferred, not proven.** The resource is most likely backing store for the
new page: the same zero-track-quota condition that blocks defects 5 and 6.
Three independent commands report `NO MORE TRACKS AVAILABLE` on this disc.

**The discriminating experiment** (not yet run): give `SYSTEM` a real track
quota, then retry `MEMORY 40000 44000`. If it completes, the hang and defects
5 and 6 are one provisioning bug. If it still hangs, the swapper has an
independent fault.


## The track-quota question — RESOLVED, and my earlier inference was WRONG

**Measured 2026-07-25** (`Build/cmdtest/quota`, one session):

```
DISK-SPACE           15 TRACKS LEFT      LIST-TRACKS SYSTEM   0 TRACKS LEFT
CREATE-USER U1  ->   ALREADY EXISTS      DISK-SPACE           14 TRACKS LEFT
CREATE-USER U2  ->   ALREADY EXISTS      DISK-SPACE           13 TRACKS LEFT
LIST-TRACKS U1  ->   0 TRACKS LEFT
```

**`GTRK` is NOT failing.** Every `CREATE-USER` allocates a disc track — the
free count drops by one each time — and the user still ends with **zero
quota**. The track is allocated and then **leaked**. The earlier inference
that these commands failed for lack of free tracks is therefore **wrong** and
is retracted.

The real cause is in `CRUSE` (`src/TSS4.SYMB`):

```
	LDT AREG,B; LDA DREG,B; SAX 0
	JPL I (IUSER; JPL I (FTLER      % IUSER: %X = NUMBER OF TRACKS -> SAX 0 = ZERO
	LDA AREG,B; AAA -1; JAZ *+5     % user number == 1 (SYSTEM) -> SKIP the grant
	SAA 1; SAT 1; JPL I (UTRK; JMP CF
	MIN LREG,B; JMP CF+1
```

1. **Every user is initialised with `X = 0` tracks.** `IUSER`'s documented
   interface is `%X = NUMBER OF TRACKS`, and `CRUSE` passes `SAX 0`.
2. **User 1 (SYSTEM) is explicitly skipped** from the `UTRK` grant that
   follows — `JAZ *+5` jumps straight to the success return.
3. For every other user the `UTRK` grant is attempted, **fails**, and sends
   the whole of `CRUSE` to its failure return — leaking the track just
   obtained from `GTRK`, and surfacing as `ALREADY EXISTS` via the
   error-label bug (defect 5).

**Consequence:** a freshly cold-started TSS has an operator account with no
track quota, and **no command can grant it any** — `TRANSFER` needs a donor,
and every other account ends at zero too. That is why nothing can be created
(defect 6). It is a property of the recovered source, not of the bring-up
procedure.

## The MEMORY hang — the discriminating experiment did NOT discriminate

`MEMORY 40000 44000` was retried after the above. It **hung identically**.

But the experiment's precondition was never met: `TRANSFER` could not give
SYSTEM a quota (no donor had one), so the retry ran under the *same*
zero-quota state as before. **It therefore tells us nothing new**, and the
question "is the swapper hang a consequence of zero quota, or an independent
fault?" remains **open**.

To actually discriminate, quota must be granted by a route that does not go
through the command processor — patch the user-table entry in `cdc.img`
directly, or single-step level 5 under DAP from the `MST PID` in `CRMEM`.

## The open contradiction (SUPERSEDED — see above)

`DISK-SPACE` reports **15 free tracks out of 4096**, and
`bringup/verify-disc.py` independently confirms 15 free in the MIB — yet
**every allocation fails**. So either cold-start (`SINIT`) leaves the
per-user quota pool empty while the disc itself has space, or `GTRK` and the
MIB disagree about what "free" means. Resolving that one question would close
defects 5, 6 and probably 1.

---

# PART II — RESULTS

Executed 2026-07-25 on copies of the verified disc under
`/mnt/e/Dev/Ronny/TSS/Build/cmdtest/`. Every transcript below is the
verbatim console, captured over DAP terminal 192.

## Phase 1 — read-only

### 1. HELP

```
HELP
RESET
RECOVER
DUMP
GOTO-USER
LOAD-BINARY
PLACE-BINARY
MEMORY
LOGOUT
HELP
OPEN-FILE
CLOSE-FILE
LIST-FILE
DELETE-FILE
RENAME
STATUS
SET-REGISTER
EXAMINE
RESERVE
RELEASE
WHERE-IS
CLOCK-ON
CLOCK-OFF
LINK-TO
BREAK-LINKS
CREATE-USER
PASSWORD
WHO-IS-ON
INIT-ACCOUNTING
LIST-ACCOUNTS
LIST-USERS
TIME-USED
PAUSE
CONTINUE
DISK-SPACE
ALLOCATE
LIST-OBJECTS
CREATE-FRIEND
DELETE-FRIEND
LIST-FRIENDS
DEFINE-UIB-ACCESS
DEFINE-FILE-ACCESS
MODE
MAKE-REENTRANT
DELETE-MEMORY
CLEAR-PASSWORD
RESPONSE-TIME
DELETE-USER
SAVE-CORE
GET-CORE
PIN-DEVICE
DEFINE-DATE
DATE
RBLOAD
TRANSFER
LIST-TRACKS
BACKUP
DEFINE-VERSION
LOAD-SYSTEM
SYSUP
SYSDOWN
@
```

### 2. DATE

```
DATE
DATE IS 63 JULY 2026   218154:96
@
```

### 3. STATUS

```
STATUS
STS = 0
D = 0
P = 0
B = 0
L = 0
A = 0
T = 0
X = 0
@
```

### 4. MEMORY

```
MEMORY
LOWER BOUND: 0
 40000: EMPTY
 44000: EMPTY
 50000: EMPTY
 54000: EMPTY
 60000: EMPTY
 64000: EMPTY
 70000: EMPTY
 74000: EMPTY
@
```

### 5. EXAMINE

```
EXAMINE 1000
46000
@
```

### 6. EXAMINE-range

```
EXAMINE 1000 1010
 1000:    46000
 1001:    70346
 1002:   124005
 1003:   156577
 1004:   146157
 1005:    46000
 1006:   156570
 1007:   146404
 1010:    54004
@
```

### 7. WHO-IS-ON

```
WHO-IS-ON
 1   SYSTEM
@
```

### 8. LIST-USERS

```
LIST-USERS
  1   SYSTEM
@
```

### 9. LIST-TRACKS

```
LIST-TRACKS
USER NAME: 
0 TRACKS LEFT
@
```

### 10. TIME-USED

```
TIME-USED
TIME USED IS 990 HOURS 20 MINS -26204 SECS
OUT OF 815 HOURS 36 MINS -12960 SECS
@
```

### 11. DISK-SPACE

```
DISK-SPACE
15 TRACKS (30K WORDS) LEFT OUT OF 4096 TRACKS (8192K WORDS)
@
```

### 12. LIST-OBJECTS

```
LIST-OBJECTS
@
```

### 13. LIST-FRIENDS

```
LIST-FRIENDS
@
```

### 14. RESPONSE-TIME

```
RESPONSE-TIME
AVERAGE RESPONSE TIME IS 0.00 SECONDS
OVER A PERIOD OF 849 HOURS 4 MINS -22276 SECS
@
```

### 15. WHERE-IS

```
WHERE-IS TELETYPE
@
```

### 16. LIST-FILE

```
LIST-FILE SCRATCH
@
```

### 17. CONTINUE

```
CONTINUE
?
@
```

### 18. BREAK-LINKS

```
BREAK-LINKS
@
```

### 19. LINK-TO

```
LINK-TO SYSTEM
?
@
```


## Phase 2 — reversible state changes

### 20. RESERVE

```
RESERVE TELETYPE
@
```

### 21. WHERE-IS-after-reserve

```
WHERE-IS TELETYPE
@
```

### 22. RELEASE

```
RELEASE TELETYPE
@
```

### 23. OPEN-FILE

```
OPEN-FILE "SCRATCH:DATA,WX
BAD FILENAME
@
```

### 24. LIST-FILE-after-create

```
LIST-FILE SCRATCH
@
```

### 25. CLOSE-FILE

```
CLOSE-FILE {FNUM}
                 ^
BAD PARAMETER
@
```

### 26. DEFINE-FILE-ACCESS

```
DEFINE-FILE-ACCESS SCRATCH:DATA 777
NO SUCH FILE
@
```

### 27. RENAME

```
RENAME SCRATCH:DATA TESTF:DATA
NO SUCH FILE
@
```

### 28. DELETE-FILE

```
DELETE-FILE TESTF:DATA
NO SUCH FILE
@
```

### 29. SET-REGISTER

```
SET-REGISTER A 1234
@
```

### 30. STATUS-verify

```
STATUS
STS = 234
D = 0
P = 0
B = 0
L = 0
A = 0
T = 0
X = 0
@
```

### 31. MEMORY-assign

```
MEMORY 40000 44000
```

- **MEMORY-list-after** `MEMORY 0` — BLOCKED (a preceding command hung)
- **DELETE-MEMORY** `DELETE-MEMORY 40000` — BLOCKED (a preceding command hung)
- **CREATE-USER** `CREATE-USER TESTU` — BLOCKED (a preceding command hung)
- **LIST-USERS-verify** `LIST-USERS` — BLOCKED (a preceding command hung)
- **CREATE-FRIEND** `CREATE-FRIEND TESTU` — BLOCKED (a preceding command hung)
- **LIST-FRIENDS-verify** `LIST-FRIENDS` — BLOCKED (a preceding command hung)
- **DELETE-FRIEND** `DELETE-FRIEND TESTU` — BLOCKED (a preceding command hung)
- **DEFINE-UIB-ACCESS** `DEFINE-UIB-ACCESS (TESTU) 777` — BLOCKED (a preceding command hung)
- **PASSWORD** `PASSWORD` — BLOCKED (a preceding command hung)
- **CLEAR-PASSWORD** `CLEAR-PASSWORD TESTU` — BLOCKED (a preceding command hung)
- **LIST-TRACKS-named** `LIST-TRACKS SYSTEM` — BLOCKED (a preceding command hung)
- **TRANSFER** `TRANSFER` — BLOCKED (a preceding command hung)
- **DELETE-USER** `DELETE-USER TESTU` — BLOCKED (a preceding command hung)
- **LIST-USERS-final** `LIST-USERS` — BLOCKED (a preceding command hung)

## Phase 2b — remainder after the MEMORY hang

### 23b. OPEN-FILE-via-prompt

```
OPEN-FILE
FILE NAME: "SCRATCH:DATA
R,W,RX OR WX: WX
BAD FILENAME
@
```

### 23c. OPEN-FILE-noquote

```
OPEN-FILE SCRATCH:DATA,W
NO SUCH FILE
@
```

### 24. LIST-FILE-after-create

```
LIST-FILE SCRATCH
@
```

### 25. CLOSE-FILE

```
CLOSE-FILE {FNUM}
                 ^
BAD PARAMETER
@
```

### 26. DEFINE-FILE-ACCESS

```
DEFINE-FILE-ACCESS SCRATCH:DATA 777
NO SUCH FILE
@
```

### 27. RENAME

```
RENAME SCRATCH:DATA TESTF:DATA
NO SUCH FILE
@
```

### 28. DELETE-FILE

```
DELETE-FILE TESTF:DATA
NO SUCH FILE
@
```

### 29b. SET-REGISTER-X

```
SET-REGISTER X 777
@
```

### 30b. STATUS-verify-X

```
STATUS
STS = 0
D = 0
P = 0
B = 0
L = 0
A = 0
T = 0
X = 0
@
```

### 32. MEMORY-list

```
MEMORY 0
 40000: EMPTY
 44000: EMPTY
 50000: EMPTY
 54000: EMPTY
 60000: EMPTY
 64000: EMPTY
 70000: EMPTY
 74000: EMPTY
@
```

### 33. DELETE-MEMORY

```
DELETE-MEMORY 40000
CORE ADDRESS OF BLOCK: 
@
```

### 34. CREATE-USER

```
CREATE-USER TESTU
ALREADY EXISTS
@
```

### 35. LIST-USERS-verify

```
LIST-USERS
  1   SYSTEM
  2   TESTU
@
```

### 36. CREATE-FRIEND

```
CREATE-FRIEND TESTU
@
```

### 37. LIST-FRIENDS-verify

```
LIST-FRIENDS
TESTU
@
```

### 38. DELETE-FRIEND

```
DELETE-FRIEND TESTU
@
```

### 39. DEFINE-UIB-ACCESS

```
DEFINE-UIB-ACCESS (TESTU) 777
@
```

### 40. PASSWORD

```
PASSWORD
OLD PASSWORD IS 
NEW PASSWORD IS 
@
```

### 41. CLEAR-PASSWORD

```
CLEAR-PASSWORD TESTU
@
```

### 42. LIST-TRACKS-named

```
LIST-TRACKS SYSTEM
0 TRACKS LEFT
@
```

### 43. TRANSFER

```
TRANSFER
TO USER: TESTU
FROM USER: SYSTEM
NUMBER OF TRACKS: 1
NO MORE TRACKS AVAILABLE
@
```

### 44. DELETE-USER

```
DELETE-USER TESTU
@
```

### 45. LIST-USERS-final

```
LIST-USERS
  1   SYSTEM
@
```


## Phase 3 — system administration

### 46. CLOCK-OFF

```
CLOCK-OFF
@
```

### 47. DATE-after-clockoff

```
DATE
DATE IS 71 JULY 2026   16845:158
@
```

### 48. CLOCK-ON

```
CLOCK-ON
@
```

### 49. DATE-after-clockon

```
DATE
DATE IS 60 JULY 2026   82108:160
@
```

### 50. DEFINE-DATE

```
DEFINE-DATE
DAY: 26
MONTH: 7
YEAR (E.G. 1973): 2026
HOUR: 8
MINUTE: 30
SECOND: 0
@
```

### 51. DATE-after-define

```
DATE
DATE IS 10 JULY 2026   23495:232
@
```

### 52. SYSDOWN

```
SYSDOWN
@
```

### 53. SYSUP

```
SYSUP
@
```

### 54. INIT-ACCOUNTING

```
INIT-ACCOUNTING
@
```

### 55. LIST-ACCOUNTS

```
LIST-ACCOUNTS
FILE NAME: LINE-PRINTER
TITLE: TESTRUN
@
```

### 56. ALLOCATE

```
ALLOCATE
FILE NAME: ABSF:DATA
TRACK ADDRESS: 4500
NUMBER OF TRACKS: 1
NO SUCH FILE
@
```

### 57. BACKUP

```
BACKUP
@
```

### 58. DISK-SPACE-after

```
DISK-SPACE
15 TRACKS (30K WORDS) LEFT OUT OF 4096 TRACKS (8192K WORDS)
@
```

### 59. LIST-OBJECTS-after

```
LIST-OBJECTS
@
```


## Phase 4 — program load / control

### 60. PLACE-BINARY-tape

```
PLACE-BINARY TAPE-READER
FILE NAME: 
BAD FILENAME
@
```

### 61. STATUS-after-place

```
STATUS
STS = 0
D = 0
P = 0
B = 0
L = 0
A = 0
T = 0
X = 0
@
```

### 62. MEMORY-after-place

```
MEMORY 0
 40000: EMPTY
 44000: EMPTY
 50000: EMPTY
 54000: EMPTY
 60000: EMPTY
 64000: EMPTY
 70000: EMPTY
 74000: EMPTY
@
```

### 63. DUMP

```
DUMP
FILE NAME: DMP:DATA
NO SUCH FILE
@
```

### 64. MAKE-REENTRANT

```
MAKE-REENTRANT
FILE NAME: DMP:DATA
NO SUCH FILE
@
```

### 65. SAVE-CORE

```
SAVE-CORE
FILE NAME: CORE:DATA
NO SUCH FILE
@
```

### 66. GET-CORE

```
GET-CORE
FILE NAME: CORE:DATA
NO SUCH FILE
@
```

### 67. RBLOAD

```
RBLOAD
FILE NAME: TAPE-READER
START ADDRESS: 20000
PROGRAM OUT OF BOUNDS
?
@
```

### 68. PIN-DEVICE

```
PIN-DEVICE
@
```

### 69. GOTO-USER-illegal

```
GOTO-USER 177777
ILLEGAL ADDRESS
?
@
```

### 70. RESET

```
RESET
@
```

### 71. MEMORY-after-reset

```
MEMORY 0
 40000: EMPTY
 44000: EMPTY
 50000: EMPTY
 54000: EMPTY
 60000: EMPTY
 64000: EMPTY
 70000: EMPTY
 74000: EMPTY
@
```


## Phase 4b — PLACE-BINARY via the prompt path

### 60b. PLACE-BINARY-prompt-tape

```
PLACE-BINARY
FILE NAME: TAPE-READER
FILE NAME: 
BAD FILENAME
@
```

### 61b. STATUS-after-place

```
STATUS
STS = 0
D = 0
P = 0
B = 0
L = 0
A = 0
T = 0
X = 0
@
```

### 62b. MEMORY-after-place

```
MEMORY 0
 40000: EMPTY
 44000: EMPTY
 50000: EMPTY
 54000: EMPTY
 60000: EMPTY
 64000: EMPTY
 70000: EMPTY
 74000: EMPTY
@
```

### 63b. EXAMINE-load-area

```
EXAMINE 206 216
 206:        0
 .         .
 .         .
 .         .
 216:        0
@
```


## Phase 5a — session-affecting

### 72. DEFINE-VERSION

```
DEFINE-VERSION
VERSION (A OR B): A
@
```

### 73. RECOVER

```
RECOVER
FILE NAME: DMP:DATA
NO SUCH FILE
@
```

### 74. PAUSE

```
PAUSE

PASSWORD IS 

@
```

### 75. DATE-after-pause

```
DATE
DATE IS 128 JULY 2026   200119:106
@
```

### 76. MODE

```
MODE
INPUT FILE: TELETYPE
OUTPUT FILE: TELETYPE
@
```

### 77. DATE-after-mode

```
DATE
DATE IS 230 JULY 2026   14682:130
@
```

### 78. LOGOUT

```
LOGOUT
227 JULY 2026   0679:232
TIME USED IS 728 HOURS 12 MINS -16378 SECS
OUT OF 1025 HOURS 20 MINS -23192 SECS
```


## Phase 5b — LOAD-BINARY (isolated run)

### 79. LOAD-BINARY-tape

```
LOAD-BINARY
FILE NAME: TAPE-READER
FILE NAME: 
BAD FILENAME
ILLEGAL ADDRESS
?
@
```


## Phase 5c — LOAD-SYSTEM (isolated run)

### 80. LOAD-SYSTEM

```
LOAD-SYSTEM
```




---

# PART IV — defects found on 2026-07-26

Two defects observed while empirically validating the `LOGON` master-password
finding (`docs/PROJECT-DESCRIPTION.md` §9). Both were reproduced on a **fresh
disc copied from `Build/bringup`**, under nd100x, and both have a source-level
explanation.

## D1. `CREATE-USER` succeeds but reports `ALREADY EXISTS`

**Observed.** On a disc containing only user `SYSTEM`:

```
@LIST-USERS
  1   SYSTEM
@CREATE-USER SECURE
ALREADY EXISTS
@LIST-USERS
  1   SYSTEM
  2   SECURE
```

`LIST-USERS` before the call shows the name does **not** exist. The command
prints `ALREADY EXISTS`. `LIST-USERS` after the call shows the user **was
created** — and it is fully usable: `SECURE` subsequently logged in and set a
password successfully.

So the message is wrong, and an operator following it would conclude the
account was not created when it was.

**Cause, read from source.** `CRUSR` (`src/TSS4.SYMB:940-957`) reaches its
`ALREADY EXISTS` message from **two** places:

```
        LDA (USRTB; JPL I (ABLKP; JMP *+2; JMP CF3   /* name found -> correct  */
        AAA 2; JAZ CF2
        RCLR DD; SAX XTR; RADD DX SB; JPL I (CRUSE; JMP CF3
                                                    /*  ^^^ ANY CRUSE failure */
CF3,    LDX (MS3; JMP CFF                           /* MS3 = 'ALREADY EXISTS' */
```

Remember the inverted skip return: the word immediately after `JPL I (CRUSE`
is the **failure** path. So `CRUSE`'s three distinct failure codes —
`A=1` no more tracks, `A=2` too many users, `A=3` user already exists
(`src/TSS5.SYMB:68-70`) — are **all** collapsed onto the single message
`ALREADY EXISTS`. The specific code in `A` is discarded.

**Why the user still exists.** `CRUSE` writes the user-table entry, allocates
the UIB track and calls `IUSER` *before* it charges one track to user 1
(`src/TSS5.SYMB:97-106`). A failure in that final `UTRK` therefore returns the
failure path with the account already fully created and committed to disc.

**Which failure fires — ANSWERED 2026-07-26.** The experiment was run:

```
@DISK-SPACE
15 TRACKS (30K WORDS) LEFT OUT OF 4096 TRACKS (8192K WORDS)
@LIST-TRACKS
USER NAME: SYSTEM
0 TRACKS LEFT                      <-- SYSTEM's quota is ZERO
@CREATE-USER SECURE
ALREADY EXISTS
@LIST-USERS
  1   SYSTEM
  2   SECURE                       <-- created anyway
@LIST-TRACKS
USER NAME: SYSTEM
0 TRACKS LEFT
@LIST-TRACKS
USER NAME: SECURE
0 TRACKS LEFT                      <-- new user also gets zero
@DISK-SPACE
14 TRACKS (28K WORDS) LEFT ...     <-- one track spent on the UIB, correctly
```

**`SYSTEM`'s track quota is `0` after bring-up.** So the final step of `CRUSE`,
`UTRK(+1, user 1)` (`src/TSS5.SYMB:106`), computes `0 - 1 = -1`, is refused by
`UTRK` (`src/TSS2.SYMB:3118`), and returns `A = 1` "NO MORE TRACKS". `CRUSR`
then prints `ALREADY EXISTS` for it. The trigger is now **CONFIRMED**: it is
the track-quota charge, not a name clash.

Note the global free-track count behaved correctly throughout — `15 -> 14`, one
track for the new user's index block. Only the *quota* accounting failed.

**Severity note.** This also invalidated two earlier runs of this experiment,
where `CREATE-USER TEST` and `CREATE-USER TESTA` were abandoned on seeing
`ALREADY EXISTS`. Those users had almost certainly been created. Any future
test of `CREATE-USER` must confirm with `LIST-USERS` on both sides rather than
trusting the return message.

## D2. ~~`LOGOUT` leaves the terminal dead~~ — RETRACTED 2026-07-27: press ESC

**This is not a defect.** The original probe only ever sent carriage
returns. Waking a quiescent terminal with **ESC** is the ordinary SINTRAN
convention, and TSS follows it. Verified 2026-07-27 by sending a graded
sequence of inputs after `LOGOUT` and waiting 10 s after each:

| input sent | result |
|---|---|
| `CR` (`\r`) | silent — this is what produced the false "dead terminal" |
| **`ESC` (`\x1b`)** | **`NORD TSS VERSION 3.0A IS UP` … `@ENTER`** — session ready |

So `LOGOUT` behaves correctly end to end: it prints its sign-off, releases
the session, and the terminal re-arms on ESC. The methodology note below
about needing a fresh boot per session is **withdrawn** — `LOGOUT` + ESC is
a valid way to start a new session in a multi-session test.

The accounting garbage in the sign-off is a **separate, still-open** issue;
it is preserved as the secondary observation below and is tracked as P3.
It reproduced unchanged on 2026-07-27 (`TIME USED IS 600 HOURS 2 MINS
-24896 SECS`), and per the owner's direction should be re-tested only after
32-bit floating point lands in nd100x, since the emulator currently
implements the 48-bit format only and this has the shape of an arithmetic
fault.

The original (now superseded) observation follows, kept for the record.

---

**Observed [2026-07-25, superseded].** From a logged-in `@` prompt:

```
@LOGOUT
126 JULY 2026   154126:124
TIME USED IS 1013 HOURS 40 MINS -5204 SECS
OUT OF 716 HOURS 32 MINS -19004 SECS
<nothing further>
```

The accounting summary prints, then the terminal produces no further output.
Six carriage returns sent at 6-second intervals produced nothing; the terminal
never reprinted `@ENTER`, so no new session can be started. Reproduced twice,
on separate boots, at ports 1871 and 1873.

Entry 78 of Phase 5a already recorded `LOGOUT` as "returns"; that is correct as
far as it goes — the command itself completes and prints its summary. What was
not tested then was **whether the terminal is usable afterwards**. It is not.

**Consequence for test methodology [WITHDRAWN 2026-07-27 — see above].** This
said a multi-session test must use a fresh boot per session. That was a
consequence of the wrong conclusion; `LOGOUT` followed by **ESC** returns to
`@ENTER` and starts a new session. A fresh boot per session remains a valid
technique (it is how the master-password validation was run) but is no
longer *required*.

**Secondary observation in the same output:** the accounting figures are
visibly wrong — `-5204 SECS` and `-19004 SECS` are negative, and
`1013 HOURS` of time used against a `716 HOURS` allowance is inconsistent. The
date line `126 JULY 2026   154126:124` is also malformed. This suggests the
accounting fields are being formatted from uninitialised or wrongly-scaled
values. `UACCT` (`src/TSS4.SYMB:724`) and `TUSED` (`:1121`) are where to look.
**Not investigated.**

**Cause: UNKNOWN.** Not investigated at source level. `QUIT`
(`src/TSS4.SYMB:1065-1120`) is the handler; `LOGF1` (`src/TSS2.SYMB:1698`)
clears `TTYTB[terminal]` on the login-timeout path and is the closest analogue
of what a clean return-to-`LOGON` should do.


## D3. `SYSTEM` has no track quota after bring-up, and `TRANSFER` to yourself mints more

**Observed.** On a freshly bootstrapped disc, **every** user's quota is zero —
including `SYSTEM`. Consequences, all reproduced:

- `CREATE-USER` always reports the spurious `ALREADY EXISTS` of D1.
- `TRANSFER` **from** `SYSTEM` to anyone fails with `NO MORE TRACKS AVAILABLE`,
  because the debit leg is refused.
- No user can ever be given quota by the normal route.

The cause is structural: `SINIT` creates `SYSTEM` through `CRUSE`, and `CRUSE`
passes literal `0` as the track count to `IUSER` (`src/TSS5.SYMB:103`,
`SAX 0`). User 1 skips the `UTRK` charge that would otherwise fail, so the
account is created — with a zero quota and nothing to draw on.

**The escape hatch, verified.** `TRTRK` (`src/TSS5.SYMB:1589`) reads:

```
T2,	LDA US1,B; SUB US2,B; JAZ T4
T3,	LDT US2,B; LDA CNT,B; JPL I (UTRK; JMP TFX     /* debit  the donor     */
T4,	LDT US1,B; LDA CNT,B; COPY CM2 DA SA; JPL I (UTRK; JMP TFX  /* credit */
```

When the caller is `SYSTEM` **and `TO USER` equals `FROM USER`**, `JAZ T4`
jumps straight to the credit and **skips the debit entirely**. Confirmed:

```
@LIST-TRACKS
USER NAME: SYSTEM
0 TRACKS LEFT
@TRANSFER
TO USER: SYSTEM
FROM USER: SYSTEM
NUMBER OF TRACKS: 20
@LIST-TRACKS
USER NAME: SYSTEM
20 TRACKS LEFT                     <-- created from nothing
@DISK-SPACE
14 TRACKS (28K WORDS) LEFT ...     <-- global count UNCHANGED
```

Ordinary transfers then behave conservatively — `TO=SECURE FROM=SYSTEM 5` gave
`SECURE` 5 and left `SYSTEM` 15.

**Two observations, offered without a verdict on intent:**

1. This is almost certainly the intended administrative bootstrap — there is no
   other command that can increase a quota, so without it the quota system is
   inert on a fresh system.
2. **Quota is completely decoupled from physical free space.** After the
   transfer `SYSTEM` held a 20-track quota on a disc with 14 tracks actually
   free. `UTRK` only checks the quota word; `GTRK` only checks the bitmap. A
   quota can therefore be issued that the disc cannot honour.

**Practical note for bring-up:** after a fresh `SINIT`, an operator must run
`TRANSFER` with `TO USER` = `FROM USER` = `SYSTEM` before the file system is
usable by anyone.

## D4. RESOLVED — files CAN be created; the name needs quotes on BOTH sides

`docs/TSS-USER-MANUAL.md` has recorded since 2026-07-25 that **no way to create
a file was found** from the `@` prompt, after `OPEN-FILE "SCRATCH:DATA,WX`
returned `BAD FILENAME`. **That conclusion is wrong, and the syntax is the
reason.**

The correct form quotes the name on **both** sides:

```
@OPEN-FILE "MYFILE",W
FILE NUMBER = 100
```

**Verified 2026-07-26** on the `Build/cmdtest/quotachain` disc, user `SECURE`
holding a 5-track quota:

```
@LIST-TRACKS
USER NAME: SECURE
5 TRACKS LEFT
@DISK-SPACE
14 TRACKS (28K WORDS) LEFT OUT OF 4096 TRACKS ...
@OPEN-FILE "MYFILE",W
FILE NUMBER = 100                  <-- created and opened
@LIST-TRACKS
USER NAME: SECURE
4 TRACKS LEFT                      <-- quota charged
@DISK-SPACE
13 TRACKS (26K WORDS) LEFT ...     <-- track physically allocated
@LIST-FILE
FILE NAME: MYFILE
  1   MYFILE:SYMB                  <-- it is there
```

### Why the unterminated form fails

`FFOPE`, the `OPEN` monitor call, uses `"` as a **delimiter pair**, not a
prefix flag:

| line | code | meaning |
|---|---|---|
| `src/TSS3.SYMB:1350` | `SAT ##"; SKP IF DA EQL ST; JMP O5` / `SAT -1; STT NEWF,B` | a **leading** `"` sets `NEWF = -1` |
| `src/TSS3.SYMB:1365`, `:1373` | `SAT ##"; SKP IF DA UEQ ST; JMP O15` | a **later** `"` jumps to `O15` |
| `src/TSS3.SYMB:1375` | `O15, LDA NEWF,B; JAZ *+2; JMP O17; JMP I (OF8` | a closing `"` is accepted **only if** `NEWF` is set |
| `src/TSS3.SYMB:1376` | `O16, LDA NEWF,B; JAP *+2; JMP I (OF8` | reached when the name **runs out**; if `NEWF < 0` -> `BAD FILENAME` |

So `"MYFILE` with no closing quote reaches `O16` with `NEWF = -1` and is
rejected. `OF8` is `SAA 57` (`src/TSS3.SYMB:1420`) — error `0o57` = 47 =
`MS47`, `'BAD FILENAME'` (`src/TSS5.SYMB:1090`).

The command processor was never at fault. The 2026-07-25 test used
`"SCRATCH:DATA,WX` — an opening quote with no closing one — which is exactly
the rejected case.

## D5. Confirmed: a user with zero quota cannot create a file

The prediction chained from `CRUSE` -> `IUSER` -> `UTRK` -> `CRFIL` is
**confirmed**, and the control is the same command on the same disc:

```
--- user SECURE, quota 5 ---
@OPEN-FILE "MYFILE",W
FILE NUMBER = 100

--- user POOR, quota 0 (freshly created) ---
@OPEN-FILE "HISFILE",W
NO MORE TRACKS AVAILABLE
```

`CRFIL` charges one track to the file's owner before doing anything else
(`src/TSS5.SYMB:259`), and `UTRK` refuses a charge that would take the quota
below zero (`src/TSS2.SYMB:3118`).

**This also proves D1's cause from the opposite direction.** By this point
`SYSTEM` held a 15-track quota, and:

```
@CREATE-USER POOR
USER NUMBER = 3                    <-- no spurious ALREADY EXISTS
```

Same command, same code, same disc — only the quota differs. With quota,
`CREATE-USER` reports success correctly; without it, the `UTRK` charge fails
and `CRUSR` mislabels the failure as `ALREADY EXISTS`.


---

# PART V. FULL RE-RUN, 2026-07-27 — clean bring-up after the mode-6 fix

## V.1 What was re-run and why

The whole sweep was executed again from a **completely fresh bring-up**, to
confirm that the addressing-mode-6 fix in `mac-c`
(`/mnt/e/Dev/Ronny/TSS/docs/MAC-ASSEMBLER.md` §6.7) resolved the `MEMORY`
hang without disturbing anything else. Nothing was carried over: the disc
set was deleted and rebuilt from scratch.

Bring-up chain, in order, all from `/mnt/e/Dev/Ronny/TSS`:

| step | command | verified end state |
|---|---|---|
| build | `make build` | mac-as + **708 assertions, 0 failures**; TSS, DRUM, MINIT and TDUMP artifacts |
| encoding guard | `make check-encoding` | `Build/drum/tss-drum.bpun: FIXED` |
| wipe | `make clean-bringup` | `Build/bringup/` removed |
| bring-up | `make auto` | MINIT format + cold-start, unattended over DAP |
| verify | `make verify` | **15 free tracks**, `SYSTEM` present in `USTBL` |
| quota | `TRANSFER SYSTEM←SYSTEM 20` | `20 TRACKS LEFT` (the D3 bootstrap step) |

Gate results at that state: mac-c **708 passed / 0 failed**, coverage clean,
golden oracle **679/693 (A)** and **675/689 (B)** with **0** assembly errors,
`verify_repo.sh` fully green.

Each phase then ran on its **own disposable copy** of that verified disc set,
per the PART I method, so a phase that creates users or files cannot perturb
the next.

## V.2 Results

**89 command invocations covering all 60 commands. 87 returned to `@`.**

| phase | invocations | result |
|---|---|---|
| 1 — read-only / informational | 19 | all return |
| 2b — files, users, friends, registers | 23 | all return |
| 3 — clock, accounting, system state | 14 | all return |
| 4 — binaries, core images, devices | 12 | all return |
| 4b — `PLACE-BINARY` via prompt | 4 | all return |
| 5a — risky: version, recover, pause, mode, logout | 7 | 6 return; `LOGOUT` ends the session **by design** |
| 5b — `LOAD-BINARY` from tape | 1 | returns |
| 5c — `LOAD-SYSTEM` | 1 | **does not return** |
| 6 — `MEMORY` assign/list/delete (new) | 8 | all return |

Only **`LOAD-SYSTEM`** fails to return. `LOGOUT` is counted as a non-return
by the automatic scorer because no `@` follows, but that is its defined
behaviour and the terminal re-arms on **ESC** (D2).

## V.3 The three things that changed since 2026-07-25

**1. `MEMORY <lower> <upper>` works.** This was the headline hang. Phase 6
was added specifically to exercise the previously-wedging path end to end:

```
@MEMORY 40000 44000        -> returns to @
@MEMORY 0                  -> shows the page assigned
@EXAMINE 40000 40004       -> reads it
@DELETE-MEMORY 40000       -> releases it
@MEMORY 0                  -> shows it EMPTY again
@MEMORY 50000 54000        -> a second range also works
```

Root cause was in the assembler, not in TSS. See defect 1 in the Headline
and `MAC-ASSEMBLER.md` §6.7.

**2. `LOGOUT` does not kill the terminal.** ESC revives it. D2 retracted.

**3. `CREATE-USER` reports success.** `CREATE-USER TESTU` → `USER NUMBER =
2`, with no spurious `ALREADY EXISTS`. The only difference from the 07-25
run is that `SYSTEM` held quota, which is precisely what the D1 analysis
predicted. D1 is therefore confirmed as *conditional on quota*, not a
constant misbehaviour.

## V.4 Still open after the re-run

- **`LOAD-SYSTEM` does not return.** Unchanged. The command itself is
  correct; the working disc carries no bootstrap for it to reload. This
  needs a different build variant rather than a code fix.
- ~~**Accounting arithmetic is still wrong.**~~ **CLOSED the same day — see
  PART VI.** `LOGOUT` printed `TIME USED IS 600 HOURS 2 MINS -24896 SECS` and
  the date line `126 JULY 2026 154126:124`. The deferral recorded here ("re-test
  only after 32-bit floating point lands in nd100x") was based on the retracted
  PART III §3 theory. 32-bit FP did land, made no difference, and the actual
  cause was the `NLZ`/`DNZ` classification in `mac-c`.
- **`SET-REGISTER` targets the wrong register** (D-part of the 07-25 list) —
  not re-investigated in this run.

## V.5 Harness note

Five phases initially reported `NEVER REACHED @` with a **completely empty
console** — the emulator never printed its banner. That is a launch race
between consecutive emulator starts, not a TSS result: every one of them
passed on the first retry once a `sync` and a five-second settle gap were
inserted between phases. Any future sweep should keep that gap. An empty
console is the signature to look for; a genuine hang always shows the
command echo first.

---

# PART VI. THE NLZ/DNZ ROUND, 2026-07-27 — what was re-verified, and what was not

## VI.1 The fix

`mac-c` classed `NLZ` (`0151400`) and `DNZ` (`0152000`) as `MAC_CLS_PLAIN`,
which sums every term into the full 16-bit word. Their scaling factor is an
**8-bit field**, so a negative scaling borrowed out of that field and into the
opcode:

```
DNZ -20   assembled   0152000 - 0000020 = 0151760      <- this is NLZ + 0360
correct               0152000 + (-020 & 0377) = 0152360
```

Reclassifying both as `MAC_CLS_ARG8` applies the correction ND-60.096.01
§3.2.2.4 spells out. Positive operands were never affected, which is why only
the twelve `DNZ -20` sites broke — and every one of them is the
float-to-integer step in `TBANG`, the routine that decomposes elapsed clock
ticks into days, hours, minutes and seconds. One wrong word corrupted every
clock-derived value in the system.

Commit `01dce34`. Pinned by test `[18]` in
`/mnt/e/Dev/Ronny/TSS/mac-c/tests/test_mac.c`.

## VI.2 Verified live, on a clean rebuild and bring-up

| command | before | after |
|---|---|---|
| `DATE` x6 | `126 JULY 2026   154126:124`, a different day every read | `26 JULY 2026   0830:00`, identical across all six reads |
| `TIME-USED` | `1013 HOURS 40 MINS -5204 SECS` | `0 SECS / OUT OF 2 SECS` |
| `RESPONSE-TIME` | garbage | `AVERAGE RESPONSE TIME IS 0.02 SECONDS / OVER A PERIOD OF 4 SECS` |
| `LOGOUT` | negative seconds, impossible totals | `26 JULY 2026 0830:00 / TIME USED IS 2 SECS / OUT OF 11 SECS` |

Gates at that state: mac-c **718 assertions, 0 failures**, coverage clean,
golden oracle **679/693 (A)** and **675/689 (B)** with **0** assembly errors,
`verify_repo.sh` fully green.

## VI.3 What was NOT re-run at this point

**Only the four commands above.** The 89-invocation sweep in PART V ran
against the pre-fix binary. Nothing suggested the fix disturbed anything else —
the oracle scores and the assertion count were unchanged, and no non-clock
command reaches `TBANG` — but *no evidence of harm is not re-verification*.
**That gap has since been closed: see PART VII.**

## VI.4 A test was asserting the bug

`tests/test_mac.c` contained:

```c
check_word("DNZ -20 (masked)            ",
           asm1_at(&st, 01000, "DNZ -20"),
           (uint16_t)(0152000 - 020));          /* = 0151760 -- the defect */
```

It was labelled *(masked)* though nothing was masked, and it held the wrong
expected value. It did not merely fail to catch the defect; it **locked it
in** — any correct fix would have turned the suite red. Corrected in the same
commit.

Worth carrying forward, given how heavily this project leans on its suite and
on the golden dumps: a green suite can be green *because* it agrees with the
fault, and the golden dumps validate **word counts only**, never encodings.
Both oracles were fully satisfied by a binary whose clock arithmetic was
inverted.

---

# PART VII. FULL RE-RUN, 2026-07-27 (second) — clean bring-up after the NLZ/DNZ fix

This closes the gap flagged in PART VI.3. The entire sweep was executed again
from a completely fresh bring-up, against the post-`01dce34` binary. **These
are the current results.** PART II and PART V are historical.

## VII.1 Bring-up chain

All from `/mnt/e/Dev/Ronny/TSS`:

| step | command | verified end state |
|---|---|---|
| build | `make build` | mac-as + **718 assertions, 0 failures**; TSS, DRUM, MINIT artifacts; **0** assembly errors |
| encoding guard | `make check-encoding` | `Build/drum/tss-drum.bpun: FIXED`, `Build/bringup/tss.bpun: FIXED` |
| wipe | `make clean-bringup` | `Build/bringup/` removed |
| bring-up | `make auto` | MINIT format + cold-start, unattended over DAP |
| verify | `make verify` | **15 free tracks**, `SYSTEM` present in `USTBL` |
| quota | `TRANSFER SYSTEM<-SYSTEM 20` | `20 TRACKS LEFT`, `DISK-SPACE` 15 of 4096 |

Sweep driver: `sweep_postdnz.sh`, one disposable disc copy per phase, with the
PART V.5 settle gap and up to three attempts per phase.

## VII.2 Results — unchanged from PART V, which is the point

**89 invocations covering all 60 commands. 87 return to `@`.**

| phase | invocations | result |
|---|---|---|
| 1 — read-only / informational | 19 | all return |
| 2b — files, users, friends, registers | 23 | all return |
| 3 — clock, accounting, system state | 14 | all return |
| 4 — binaries, core images, devices | 12 | all return |
| 4b — `PLACE-BINARY` via prompt | 4 | all return |
| 5a — version, recover, pause, mode, logout | 7 | 6 return; `LOGOUT` ends the session **by design** |
| 5b — `LOAD-BINARY` from tape | 1 | returns |
| 5c — `LOAD-SYSTEM` | 1 | **does not return** |
| 6 — `MEMORY` assign/list/delete | 8 | all return |

Identical to the pre-fix figures. **No regression**: the NLZ/DNZ change
touched twelve instruction words, all of them inside `TBANG`, and nothing
outside the clock path moved.

The three known-bad behaviours are also unchanged, as expected — none of them
reaches `TBANG`:

- `SET-REGISTER X 777` -> `STATUS` still reports `X = 0`.
- `OPEN-FILE` still gives `BAD FILENAME` (quoted) / `NO SUCH FILE` (unquoted);
  `ALLOCATE` and `DUMP` still give `NO SUCH FILE`.
- `LOAD-SYSTEM` still does not return.

## VII.3 The clock output is now sane in every phase

Verified in the transcripts, not merely by the returns-to-`@` scorer:

```
DATE            DATE IS 25 JULY 2026   1200:00     (the value set at login)
DEFINE-DATE     26,7,2026,8,30,0
DATE            DATE IS 26 JULY 2026   0830:00     (accepts the new base)
TIME-USED       TIME USED IS 2 SECS / OUT OF 13 SECS
RESPONSE-TIME   AVERAGE RESPONSE TIME IS 0.00 SECONDS / OVER A PERIOD OF 18 SECS
LOGOUT          25 JULY 2026   1200:00 / TIME USED IS 2 SECS / OUT OF 14 MINS 44 SECS
```

No negative seconds, no impossible day numbers, no value changing between two
consecutive reads. `CLOCK-OFF` / `CLOCK-ON` no longer perturb the reading.

## VII.4 NEW, and visible only now: the time of day does not advance

With the garbage gone, a second defect is legible underneath it.

**Observed.** `DATE` returns *exactly* the value last set — by the login date
prompt or by `DEFINE-DATE` — and never anything else. In phase 5a, `DATE` read
`1200:00` before `PAUSE` and `1200:00` again after `PAUSE` and `MODE`, while
the `LOGOUT` line from that same session reported **`OUT OF 14 MINS 44 SECS`**.
TSS therefore believes ~15 minutes of its own time elapsed while its
time-of-day display did not move a second.

**What this rules in and out.** The elapsed-time counters (`TIME USED`,
`OUT OF`, `RESPONSE-TIME`'s period) *do* advance and *do* cross into minutes,
so `TBANG`'s decomposition is working. What is lost is the elapsed
contribution to the *absolute* clock.

**CONFIRMED 2026-07-28 by direct experiment — see PART VIII.** The cause is
the constant format: `K1`-`K4` are 2-word (32-bit) floats, `FDV` reads a
3-word (48-bit) operand, every quotient underflows to zero, and all four
fields stay at whatever `DEFINE-DATE` set.

**Note that this defect was invisible before.** While the day field was
randomly garbage on every read it *looked* like it was changing. Fixing the
loud bug is what made the quiet one observable — and the same is likely true
of whatever sits under this one.

---

# PART VIII. THE FROZEN CLOCK — CONFIRMED CAUSE, 2026-07-28

## VIII.1 The measurement chain

Three experiments, each isolating one variable, in the order run.

**1. Is the input moving?** (`klok_probe.py`) `KLOK` at `000016`/`000017`
advances steadily at ~350 ticks/s and never goes backwards. Over 134 seconds
the elapsed count reached **6736 ticks**, which at the nominal 50 Hz is
2 min 14 s — so `DATE` should have read `1202:14`. It read `1200:00`.

The input moves. The arithmetic discards it. That alone rules out the clock
interrupt and moves the fault downstream, into `TBANG`.

(The ~350 ticks/s rather than 50 is the instruction-locked RTC, as the nd100x
team described. It is not a defect and is not relevant here — only the
*monotonic advance* matters.)

**2. Locate `K1` — by search, not by arithmetic.** Reading `042701` out of
memory near `RDATE` put `K1` at **`022650`**. Arithmetic off `RDATE=022700`
minus eight words predicts `022670` — **wrong by 16 words**, because a
16-word table sits between `K4` and `RDATE`. This is the fourth time
hand-derived addressing has failed in this project; the constants were found
by pattern search instead.

**3. Patch one constant.** `K1` overwritten in memory with a well-formed
48-bit float of **3000** (`040014 135600 000000`) — deliberately small, so
the "days" term ticks every 3000 ticks (~9 s) instead of every 4.32e6
(~3.4 h) and the effect is observable in one session.

```
BEFORE   DATE IS 25 JULY 2026   1200:00   (x3, frozen)
PATCH    K1 := 040014 135600 000000       (readback confirmed)
AFTER    DATE IS 28 JULY 2026   1200:00
         DATE IS 29 JULY 2026   1200:00
         DATE IS 30 JULY 2026   1200:00
         DATE IS 31 JULY 2026   1200:00
         DATE IS  1 AUGUST 2026 1200:00   <- month rollover works
         DATE IS  2 AUGUST 2026 1200:00
```

The day field advances the moment `K1` is well-formed, and the calendar
rollover logic is correct. **The cause is the constant format.** The
hour/minute/second fields stay frozen because `K2`-`K4` were left alone —
a 48-bit `K1` needs three words where two were reserved, so the patch
necessarily clobbers `K2`'s word0. That was accepted: the day field alone is
the discriminator.

## VIII.2 Why `--fpp=32` does not settle it

nd100x gained `--fpp=32|48`. Re-running the date probe under both widths, now
that the NLZ/DNZ defect is out of the way:

| width | result |
|---|---|
| 48 | `25 JULY 2026 1200:00` on all six reads — frozen, stable |
| 32 | `26 JULY 1301:01` -> `40 JULY 1004:144` -> `25 JULY 1200:00` -> `26 JULY 1301:01` — moving, **non-monotonic, garbage**, and `TIME-USED` back to `-11056 SECS` |

Neither width produces a correct clock, and **the 32-bit run cannot be used as
evidence about TSS** — but not for the reason first given here.

> **[CORRECTION 2026-07-28]** This section argued that nd100x's FPP32 mode was
> internally inconsistent, because `ndfunc_stf`/`ndfunc_ldf` (`:637`, `:681`)
> move three words while `ndfunc_fad/fsb/fmu/fdv/nlz/dnz` branch on
> `CurrentFPPType` and read two. **nd100x is correct; the criticism was
> wrong.** ND-110 RASK and ND-120 DELILAH-L microcode hardwire `LDF`/`STF` to
> the 3-word `T/A/D` layout, with no entry point for a 2-word load. A
> 32-bit-FPP machine uses `LDD`/`STD` for its `A,D` accumulator instead — two
> register sets, two instruction pairs. A patch to make `LDF`/`STF` branch was
> proposed here and **withdrawn before it was applied**; it would have made
> nd100x diverge from real hardware.

The correct reason the 32-bit run proves nothing about TSS: **TSS is not a
32-bit program.** It uses `STF`/`LDF` throughout, so running it under a 32-bit
FPP mixes 3-word load/store with 2-word arithmetic in the program's own terms,
and can only produce garbage. Full account in
[`TSS-FLOAT-FORMAT.md`](TSS-FLOAT-FORMAT.md) §4.

**This also retires my earlier "FP width is ruled out" claim properly.** That
experiment was run *before* the NLZ/DNZ fix, so the dominant defect masked
both arms, and it was run under a configuration TSS was never built for. What
replaced it is the `K1` patch: one variable, one observable, on an otherwise
untouched system.

## VIII.3 Attribution — this is NOT a `mac-c` defect

The 1978 original had the same 2-word constants:

- `TBANG=022611` and `RDATE=022700` in **both** `reference/ASYMB.SYMB` and our
  `Build/ASYMB.SYMB` — a 55-word span, identical.
- Our build's `K1`-`K4` occupy eight words, read directly out of memory at
  `022650`-`022657`, not inferred.
- Three-word constants would push `RDATE` to `022704`. The golden dump says
  `022700`.

`mac-c` reproduces the archived binary faithfully. Nothing here should be
"fixed" in the assembler — doing so would diverge from the artifact this
project exists to reconstruct.

## VIII.4 What remains genuinely unknown

There is a real tension in the original system, and it should not be papered
over:

- `TBANG`'s constants are in the **2-word** format, which suits a 32-bit FPP.
- `FPDAT` (`src/TSS5.SYMB:520`) does `STF TEMP,B` into a `DATA TEMP,3` buffer
  and unpacks **six** byte fields from **three** words, which suits a 48-bit
  FPP.

Both are read from the source; they point opposite ways. Possible readings:
the 1973 source predates the machine it was finally built for; the `[` format
differed between MAC builds; or one of the two routines was already broken in
1978 and nobody noticed, since a timesharing system that boots and bills in
seconds can run for years with a wrong day counter. **No evidence currently
distinguishes these.** Settling it needs a primary source on the ND-100
`LDF`/`STF` word count per FPP option, which is the next thing to read rather
than reason about.
