# `tools/analysis/`

Read-only reporters. Each one parses something the build already produced —
a MAC `)LIST` symbol dump, or the `Build/` tree — and prints a derivation.
None of them boots the emulator, writes to `src/`, or takes part in `make`.

They were moved out of `mac-c/`, which is the assembler's directory: these
analyse the *system that was assembled*, not the assembler.

---

### `overlay_layout.py` — where each overlay lives on the CDC disc

```
python3 tools/analysis/overlay_layout.py ../../reference/ASYMB.SYMB
```

Reads a golden symbol dump (`reference/ASYMB.SYMB` or `BSYMB.SYMB`) and
prints, for every overlay symbol `OVn`, the pair of 256-word disc sectors it
occupies, using the run-time/`SOVER` contract:

```
disc_sector(n) = OVDK + 2*n      and  disc_sector(n) + 1
```

`OVDK`, `ROVER` and the overlay numbers all come from the dump itself, so the
output is a **re-derivation from the oracle**, not a hand-typed table. All
values are octal, as MAC prints them.

This is the generator of the overlay→sector table in
[`../../docs/TSS-ARCHITECTURE.md`](../../docs/TSS-ARCHITECTURE.md) (overlay
chapter) — regenerate it here rather than editing the table by hand.

---

### `cold_boot_init.py` — the cells involved in cold start

```
python3 tools/analysis/cold_boot_init.py [path-to-DSYMB.SYMB]
```

Defaults to `../../Build/drum/DSYMB.SYMB` (the DRUM/N10 build). Prints the
addresses of the cells involved in TSS's cold-boot, interrupt and
overlay-dispatch bring-up, plus the emulator init derived in
`docs/TSS-ARCHITECTURE.md` (cold-start chapter).

Its header carries the findings that make the output meaningful — that `INIT`
(address `0301`) is the real interrupt bring-up while `ISTRT` is only the
per-user restart address, and that the `OVERX` macro presets
`OVLAY`/`OVLAX`/`SYSOV` to `RQR-1`, so on a cold image the first
`GOVER OV19` falsely looks already-loaded. Read it before trusting a
cold-start theory.

---

### `check_bootable.sh` — can the current build boot at all?

```
tools/analysis/check_bootable.sh
```

Answers three questions in order: does `Build/` contain any image or tape
today; do the build scripts ever ask `mac-c` for one (`-o` / `-b`); and
what actually comes out if you assemble the corpus right now with the
standard mark/define preamble. Useful when "is the build bootable?" needs a
factual answer rather than an assumption. It runs `mac-c`, so run plain
`make` in `mac-c/` first.
