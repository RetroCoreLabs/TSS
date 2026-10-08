# `Build/` - the built binaries

This folder holds the output of `make build`, and the files listed below are
**tracked in git on purpose**. A clone therefore carries a ready-to-boot TSS
without building anything. Everything here is regenerated from `../src/` by
the scripts in `../mac-c/scripts/build/`; after a rebuild, `git status`
shows which binaries changed, and committing them updates the snapshot.

Working files of a build (the staged source copies `TSSn.SYMB`, the
`build.err`/`build.out`/`err_*`/`write_*` logs) and the bring-up disc set
in `bringup/` are not tracked; see `../.gitignore`.

```bash
make build            # from the repo root: mac-c, tests, and every folder below
make build TEL=4      # the original single-user (console only) TSS instead of TEL10
```

## `drum/` - the runnable TSS (NORD-10 + swapping drum)

Built by `../mac-c/scripts/build/build_tss_drum.sh` from
`../derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB` (marks
`CDC MACF DIAB K14 TEL10 DRUM N10 CLKFX`). This is the system that runs on
nd100x.

| file | contents |
|---|---|
| `tss-drum.bpun` | **the system**, a BPUN paper tape with its own autostart address (`ISTRT`). Boot it with `nd100x --boot=bpun --image=tss-drum.bpun ...` |
| `tss-cdc.img` | the CDC cartridge disc with the overlays placed on their sectors. Unformatted: run the bring-up (`../docs/TSS-BRINGUP.md`, or `make bringup-auto`) before the first cold start |
| `tss-drum.img` | the same memory image as a flat `MACIMG` file, for inspection (`../mac-c/scripts/verify/macimg_info.sh`) |
| `DSYMB.SYMB` | the symbol dump of this build |
| `ASSYS-DRUM.SYMB` | the exact MAC command stream that was assembled (TEL mark rewritten from `TEL=<n>`) |
| `LIST1.SYMB` ... `LIST5.SYMB` | the list streams, one per source part |

It includes the two fixes of 2026-10-08: `TBANG`'s clock constants in the
48-bit format (`CLKFX` mark, `../docs/TSS-FLOAT-FORMAT.md` section 8) and
the corrected `SET-REGISTER` table (`../docs/TSS-COMMAND-VALIDATION.md`
PART III section 4). Run it with `--rtc=wall` so the clock keeps real time
(`../dist/run-tss.sh` does).

## `minit/` - the disc formatter

Built by `../mac-c/scripts/build/build_minit.sh` from `../src/MINIT.SYMB`.

| file | contents |
|---|---|
| `minit.bpun` | MINIT on BPUN tape: lays down the free-track bitmap on a bare CDC disc before the first TSS cold start (answers `4470`, `4670`, `I`) |
| `minit.img` | the same image as a flat `MACIMG` file |
| `MSYMB.SYMB`, `LISTM.SYMB`, `ASSYS-MINIT.SYMB`, `MINIT.SYMB` | symbol dump, list stream, command stream, staged source |

## `tdump/` - the bootstrap tape

Built by `../mac-c/scripts/build/build_tdump.sh` from `../src/TDUMP.SYMB`.

| file | contents |
|---|---|
| `cdbin-boot.bpun` | the CDBIN tape punched by `mac`'s `)8DUMP`. Booting it makes TSS install its own disc boot sectors (`make prepare`) |
| `tdump.bpun`, `tdump.img` | TDUMP itself, as BPUN tape and `MACIMG` |
| `TSYMB.SYMB`, `LISTT.SYMB`, `ASSYS-TDUMP.SYMB`, `TDUMP.SYMB` | symbol dump, list stream, command stream, staged source |

## Top level - the golden builds (the 1978 oracle check)

Built by `../mac-c/scripts/build/build_tss_assysa.sh`, which runs the
original `ASSYSA`/`ASSYSB` command streams. These are NORD-1 builds and are
not booted; they exist to be scored against `../reference/`.

| file | contents |
|---|---|
| `ASYMB.SYMB` | symbol dump of the version-A build, compare with `../reference/ASYMB.SYMB` |
| `BSYMB.SYMB` | symbol dump of the version-B (`DEBUG`) build |
| `ASSYSA.SYMB`, `ASSYSB.SYMB` | the command streams as run |
| `LIST1.SYMB` ... `LIST5.SYMB` | the list streams |

```bash
cd ../mac-c
./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
```

A good build reports `errors : 0` and **679 of 693** exact symbol matches
(version B: 675 of 689). If that number drops, something regressed; use
`./scripts/verify/first_divergence.sh` to find the first symbol that moved.

## Releases

A tagged release packages these binaries with the jump-start disc and the
run script; its contents are listed in `../dist/README.md`.
