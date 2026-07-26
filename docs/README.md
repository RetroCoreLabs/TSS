# docs/ — the documents

| document | contents |
|---|---|
| [`TSS-SOURCE-FILES.md`](TSS-SOURCE-FILES.md) | **per-file reference — start here if you have a filename**: what every `.SYMB` file is, what it contains, whether it is built, and where it is described in depth. Covers `TDUMP.SYMB` in full. |
| [`TSS-PSEUDOCODE.md`](TSS-PSEUDOCODE.md) | **every routine in the corpus rewritten as readable pseudo-C**, with the source line range beside each one, plus the 60-command map |
| [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) | the OS, top to bottom: memory/paging, scheduler, interrupt levels, cold start, overlays, terminal I/O and login, file system, users/accounting, command processor, device drivers, and the emulated CDC/drum devices |
| [`TSS-USER-MANUAL.md`](TSS-USER-MANUAL.md) | **user manual / command reference** — logging in, the command processor, and all 60 `@`-prompt commands documented from their source handlers, plus the full error-message table and device list |
| [`TSS-BRINGUP.md`](TSS-BRINGUP.md) | from clean checkout to a logged-in `@` prompt: build, emulator, MINIT format, cold-start, login, validation and debugging (scripts: [`../bringup/`](../bringup/README.md)) |
| [`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md) | history and provenance: what TSS 3.0 is, authorship, the `.ORG`-vs-`.SYMB` restoration, how it was built and booted in 1973 |
| [`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md) | the assembler: MAC/FMAC family, MAC.BPUN reverse engineering, and the full mac-c defect record |

[`HANDOFF-2026-07-26.md`](HANDOFF-2026-07-26.md) — session handoff: what changed,
what was found, what is still open and why, and how to run the emulator experiments.

`TSS-Style-Authorship.pdf` is imported archive material (authorship analysis);
its findings are absorbed into `PROJECT-DESCRIPTION.md`.

For a guided architecture tour, see [`../ppt/Intro to TSS.pdf`](../ppt/).
