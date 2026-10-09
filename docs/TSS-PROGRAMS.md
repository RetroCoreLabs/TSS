# Programs that ran under NORD TSS

**This file:** `docs/TSS-PROGRAMS.md`

TSS has two kinds of commands. The **utility commands** belong to the
command processor itself; TSS 3.0 has 60 of them, listed in
[`TSS-USER-MANUAL.md`](TSS-USER-MANUAL.md) and compared with the 1973 manual
in [`TSS-COMMAND-VALIDATION.md`](TSS-COMMAND-VALIDATION.md) PART X. The
**subsystems** are ordinary user programs kept as files. This page is about
the subsystems.

## How a subsystem is run

The 1973 manual ([ND-60.039.01](ND-60.039.01_Reference_Manual_for_the_NORD_Timesharing_System_16_February_1973.md),
chapter 6): "A subsystem is started with the RECOVER command. The user may
generate his own subsystems with the DUMP command." Section 5.2.2.9 adds that
the word `RECOVER` may be left out, so typing `MAC` means `RECOVER MAC`.
Whether TSS 3.0's command processor still does this was not tested.

## The standard subsystems (1973 manual, chapter 6)

| program | what the manual says |
|---|---|
| QED | the text editor: insert, delete and change lines, line editing, symbolic search, tabs |
| MAC | "the assembler and interactive debugging system for the NORD-1 computer" |
| FTN4 | the FORTRAN IV compiler |
| FLDR | the FORTRAN IV loader and runtime system |
| BASIC | an interactive BASIC system |
| DESCRIBE | describes any command, e.g. `DESCRIBE OPEN` |
| COPY | `COPY <destination> <source>`, files or peripheral devices |
| PRINT-FILE | `PRINT-FILE <file name> [, <tab specification>]` on the line printer |
| KRYSREF | listing with line numbers and page headings, then a cross-reference of all symbols |
| MAIL | sends a message to one user or to all users |
| CHESS | plays chess, in standard chess notation |
| BONDESJAKK | plays GO-MOKU ("N in a row") |

Two more are documented elsewhere:

- **RUNOFF**, the text formatter. Bo Lewendal wrote it as "a copy of the
  Runoff system from the 940 system"; CERN used it with QED on its TSS in 1973
  (B. Sagnell, *How to write reports on the NORD-TSS*,
  <https://cds.cern.ch/record/66380>). The 1973 manual itself was formatted
  with it.
- **BASIC** as sold with TSS to Norwegian technical schools was "implemented
  by another ND employee (Jørgen Håberg)" (Lewendal 2014, see
  [`TSS-AND-SINTRAN.md`](TSS-AND-SINTRAN.md)).

## What survives

| program | trace |
|---|---|
| QED | Lewendal wrote it for the NORD-1 from the Berkeley 940 editor (Lewendal 2014). CERN's note shows `QED 1.10` running on TSS 3.0 in November 1973. TSS provides its line-edit function as monitor call 12 `EDIT` (`src/TSS3.SYMB`, "EDIT A LINE"). The TSS-era *QED User's Manual*, ND-60.037.01 (September 1972), is catalogued by the Norsk Data library at sintran.com but no copy is known. Later SINTRAN III versions exist (one binary announces `QED 4.3`). |
| MAC | TSS provides MAC's breakpoint support as monitor calls 45-47 `DBRK`, `GBRK`, `SBRK`; the 1973 manual says `DBRK` names "the address of the routine in MAC". The surviving `MAC.BPUN` is a 1978 SINTRAN III build, the binary this project's assembler reconstruction `mac-c` is checked against ([`MAC-ASSEMBLER.md`](MAC-ASSEMBLER.md)). |
| MAIL | TSS provides it as monitor calls `IMAIL`, `GMAIL` and `BROAD`, and has routines headed "GET USER'S MAIL" and "DOES THE USER HAVE MAIL?" (`src/TSS4.SYMB`). |
| RUNOFF | the CERN note above, and the 1973 manual's own colophon |
| FTN4, FLDR, BASIC, DESCRIBE, COPY, PRINT-FILE, KRYSREF, CHESS, BONDESJAKK | no binary and no TSS-era manual found |

None of these programs is on the rebuilt disc, so none was run under the
reconstructed TSS. Later binaries of QED and MAC expect SINTRAN III's monitor
calls, of which TSS 3.0 provides only part
([`TSS-AND-SINTRAN.md`](TSS-AND-SINTRAN.md) section 6.1); whether any of them
runs under TSS is unknown.

## Programs that are not TSS subsystems

These appear in this project but did not run under TSS:

| program | what it is |
|---|---|
| MAC, FMAC | the assembler variants that built TSS itself |
| MINIT | the stand-alone disc formatter, run before the first TSS cold start ([`TSS-BRINGUP.md`](TSS-BRINGUP.md)) |
| TDUMP | the TSS dumper; its run punches the CDBIN bootstrap tape |
| NODAL | the control-language interpreter Lewendal wrote for CERN's NORD-10 control computers |
