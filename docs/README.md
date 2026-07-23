# docs/ — the four documents

| document | contents |
|---|---|
| [`TSS-ARCHITECTURE.md`](TSS-ARCHITECTURE.md) | the OS, top to bottom: memory/paging, scheduler, interrupt levels, cold start, overlays, terminal I/O and login, file system, users/accounting, command processor, device drivers, and the emulated CDC/drum devices |
| [`TSS-BRINGUP.md`](TSS-BRINGUP.md) | from clean checkout to a logged-in `@` prompt: build, emulator, MINIT format, cold-start, login, validation and debugging (scripts: [`../bringup/`](../bringup/README.md)) |
| [`PROJECT-DESCRIPTION.md`](PROJECT-DESCRIPTION.md) | history and provenance: what TSS 3.0 is, authorship, the `.ORG`-vs-`.SYMB` restoration, how it was built and booted in 1973 |
| [`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md) | the assembler: MAC/FMAC family, MAC.BPUN reverse engineering, and the full mac-c defect record |

`TSS-Style-Authorship.pdf` is imported archive material (authorship analysis);
its findings are absorbed into `PROJECT-DESCRIPTION.md`.

For a guided architecture tour, see [`../ppt/Intro to TSS.pdf`](../ppt/).
