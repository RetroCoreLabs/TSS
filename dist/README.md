# dist/ — what ships in a release

These files are packaged with the built artifacts by
[`.github/workflows/release.yml`](../.github/workflows/release.yml) whenever a
`v*` tag is pushed. The config, script and QUICKSTART are maintained by
hand; `cdc-jumpstart.img.gz` is a disc image produced locally by running
the documented bring-up, committed so releases can ship a ready system.

| file | purpose |
|---|---|
| `tss.cfg` | the validated nd100x configuration — boot type, image, CDC and drum paths |
| `run-tss.sh` | starts nd100x with `--mms=1`, which the config file cannot express |
| `QUICKSTART.md` | becomes the `README.md` inside the release archive: install the emulator, jump start or the one-time bring-up (bootstrap tape → MINIT format → cold start), run, log in, known limitations |
| `cdc-jumpstart.img.gz` | a disc that has already been through the bring-up (bootstrap installed, MINIT-formatted, cold-started, `SYSTEM` created, verified to boot from disc alone) — made locally with exactly the commands the QUICKSTART documents |

The release archive also contains `tss.bpun`, `minit.bpun`,
`cdbin-boot.bpun` (all built by `make build`), plus `cdc.img` (the built
overlay disc, padded to 8192 sectors), the unpacked `cdc-jumpstart.img`,
and a blank `drum.img`. **CI runs no emulator**: the shipped `cdc.img` is
unformatted, and the user either copies the jump-start disc over it or
performs the same bring-up a 1973 operator did, following the QUICKSTART.

**Why `--mms=1` is not in `tss.cfg`:** nd100x accepts `mms` only as a
command-line flag. TSS is 1973 code that programs the MMU the
Paging-System-I way; under the default MMS2 the machine boots and then hangs
in the overlay loader.

**Why there is no `tss.ini`:** nd100x's sectioned `.ini` describes a machine
whose `[boot] device` is a disc controller. TSS's normal start is a BPUN
tape image with its own autostart address, which the INI form cannot
express. The flat `--config=` file can.
