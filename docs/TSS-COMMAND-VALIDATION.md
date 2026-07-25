# NORD TSS 3.0 — command validation plan

**Status: EXECUTED 2026-07-25. All 60 commands run. Results in PART II.**

## Headline

| | |
|---|---|
| commands in `HELP` | **60** |
| distinct commands exercised | **60 (full coverage)** |
| return to the `@` prompt | **57** |
| **HANG — never return** | **2** — `MEMORY <lower> <upper>`, `LOAD-SYSTEM` |
| end the session by design | 1 — `LOGOUT` (prints its sign-off correctly) |
| crashes | **0** — the emulator never died and the console never became unusable |

### Defects found

1. **`MEMORY <lower> <upper>` hangs forever.** No output at all. The listing
   form (`MEMORY 0`) is fine. Source path: `MEM` (`src/TSS3.SYMB:1600`) →
   `CKMEM` (`src/TSS2.SYMB:458`) → `CRMEM` (`src/TSS2.SYMB:489`), which
   writes a page-table entry and posts `SAA 40; MST PID` to **interrupt
   level 5**, the resident page reader. Both loops terminate by inspection,
   so the wedge is in the level-5 hand-off. Root-causing it needs a CPU
   trace. **This is the most serious finding.**
2. **`LOAD-SYSTEM` never returns.** Console shows the echo and nothing more —
   no reboot, no sign-on banner. Documented as "reloads (reboots) the TSS
   system from disk".
3. **The time/date arithmetic is broken.** Reproducible across four commands
   and unaffected by input: after `DEFINE-DATE 26,7,2026,8,30,0` the date
   still reads `10 JULY 2026 23495:232`, and the **day field changes on every
   read** (63 → 71 → 60 → 10 → 128 → 230 → 227). `TIME-USED`,
   `RESPONSE-TIME` and `LOGOUT` all print **negative seconds**
   (`-26204 SECS`). One defect in the formatting/arithmetic, not four.
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

- Root cause of the `MEMORY`/level-5 hang (needs a CPU trace).
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
| 1 | `MEMORY <lo> <hi>` hangs | CPU spins in the **swapper**, before any I/O | open — see below |
| 2 | `LOAD-SYSTEM` never returns | works as designed; needs front-panel LOAD hardware | **no** — emulator gap |
| 3 | date/time garbage | TSS is a **32-bit-float** program; nd100x has 48-bit FP only | **no** — CPU-model mismatch |
| 4 | `SET-REGISTER` no effect | `"N10` variant drives **live** CPU registers, not the saved block | **yes** (plus an off-by-one) |
| 5 | `CREATE-USER` false error | all `CRUSE` failures wired to the `ALREADY EXISTS` label | **yes** |
| 6 | no file can be created | create marker is a **quoted pair**; then blocked on track quota | **no** — doc error + provisioning |

## 3. Date/time — a 32-bit vs 48-bit floating-point mismatch  **[PROVEN]**

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
- **Proven:** nd100x implements **48-bit FP only**
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

## 2. `LOAD-SYSTEM` — works as designed  **[PROVEN]**

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

## 4. `SET-REGISTER` / `STATUS` — the NORD-10 variant is wrong  **[PROVEN]**

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

## 1. The MEMORY hang — CLOSED: a fatal-error trap in the swapper  **[PROVEN]**

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
5. `FTLER` is at **address 000013** and its entire body is
   **`FTLER, JMP *`** (`src/TSS1.SYMB:165`) — jump-to-self. An infinite loop
   with no diagnostic.

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


