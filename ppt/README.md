# `ppt/` — Intro to TSS

`Intro to TSS.pdf` / `.pptx` — a guided tour of the whole system, from the
kernel down to the register bits of the swapping drum. The clearest starting
point for understanding *what TSS actually does*; the markdown in `../docs/`
covers provenance, authorship and the rebuild instead.

## What it covers

| section | contents |
|---|---|
| Introduction | what TSS is, the source archive, the big picture |
| 01 · Kernel | interrupt levels, memory management, scheduling, swapping, monitor calls, buffers and the driver pattern, the file system |
| 02 · User space | the 8-page process image, command processor, overlays |
| 03 · Devices | TTY, tape, cards, printer, CDC disk — registers and bits |
| 04 · The drum | `XDRUM` at `IOX 540` — full register and bit walkthrough |
| 05 · Build & run | marks → assemble → save to disk → boot standalone |

## The numbers it establishes

- **5** source parts, ≈**14,000** lines of MAC assembler
- **81** monitor calls in `MCTBL`
- **24** teletypes maximum, one logged-on user per process
- **8** pages of 2K words per process at `40000₈`, swap pool 36–72 pages
- Round-robin scheduling on level 5; `QUANT = -17`, `LQUA = 24`
- Three page modes drive swap policy: scratch, read-only (shared between
  processes via a reference count), read/write

The key architectural point it makes: **scheduling and swapping are one
mechanism** — a context switch *is* a call to `SWAPR`, because switching
processes means swapping memory. Every transfer funnels through one routine,
`TRSFR`, which is exactly where the drum plugs in.
