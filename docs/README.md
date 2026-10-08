# docs/ — the documents

## The system

| document | contents |
|---|---|
| [`TSS-SOURCE-FILES.md`](TSS-SOURCE-FILES.md) | **per-file reference — start here if you have a filename**: what every `.SYMB` file is, what it contains, whether it is built, and where it is described in depth. Covers `TDUMP.SYMB` in full. |
| [`TSS-PSEUDOCODE.md`](TSS-PSEUDOCODE.md) | **every routine in the corpus rewritten as readable pseudo-C**, with the source line range beside each one, plus the 60-command map |
| [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) | the OS, top to bottom: memory/paging, scheduler, interrupt levels, cold start, overlays, terminal I/O and login, file system, users/accounting, command processor, device drivers, and the emulated CDC/drum devices |
| [`TSS-USER-MANUAL.md`](TSS-USER-MANUAL.md) | **user manual / command reference** — logging in, the command processor, and all 60 `@`-prompt commands documented from their source handlers, plus the full error-message table and device list |
| [`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md) | history and provenance: what TSS 3.0 is, authorship, the `.ORG`-vs-`.SYMB` restoration, how it was built and booted in 1973 |

## Running and validating it

| document | contents |
|---|---|
| [`TSS-BRINGUP.md`](TSS-BRINGUP.md) | from clean checkout to a logged-in `@` prompt: build, emulator, MINIT format, cold-start, login, validation and debugging (scripts: [`../bringup/`](../bringup/README.md)) |
| [`TSS-COMMAND-VALIDATION.md`](TSS-COMMAND-VALIDATION.md) | **the evidence record for the running system**: all 60 commands exercised against the emulator, verbatim transcripts, root causes, and the corrections they forced on the user manual. PART VII holds the current results; earlier parts are historical and marked as such. |
| [`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md) | the assembler: MAC/FMAC family, `MAC.BPUN` reverse engineering, and the full `mac-c` defect record |

## What is not settled

| document | contents |
|---|---|
| [`OPEN-QUESTIONS.md`](OPEN-QUESTIONS.md) | **the live list** — every unanswered question, what is known, and what would settle it. Answered items leave this file for the document that owns the subject. |
| [`TSS-FLOAT-FORMAT.md`](TSS-FLOAT-FORMAT.md) | the one question big enough for its own document: was TSS built for a 32-bit or a 48-bit FPP? Its own source answers both ways, and the time of day cannot advance as a result. Includes the history of wrong answers. |
| [`TSS-AND-SINTRAN.md`](TSS-AND-SINTRAN.md) | NORD TSS and SINTRAN III: the history (the Taimyr SINTRAN, Bo Lewendal and NORD TSS, the 1973 CERN contract, CERN's own 1973-1975 notes on running TSS and choosing SINTRAN III, SINTRAN III replacing SINTRAN II and TSS) with timeline and lineage diagrams and working links to every CERN document, then the evidence of shared design: the monitor-call convention and 18 identical calls, 24 identical command names, the file-name grammar, friends and login. Every statement graded by source |

---

`TSS-Style-Authorship.pdf` is imported archive material (authorship analysis);
its findings are absorbed into `PROJECT-DESCRIPTION.md`.

For a guided architecture tour, see [`../ppt/Intro to TSS.pdf`](../ppt/).
