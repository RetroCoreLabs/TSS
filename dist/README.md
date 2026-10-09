# dist/ — what ships in a release

These files are packaged by
[`.github/workflows/release.yml`](../.github/workflows/release.yml) whenever a
`v*` tag is pushed. The config, script and QUICKSTART are maintained by
hand; `cdc-jumpstart.img.gz` is a disc image produced locally by running
the documented bring-up, committed so releases can ship a ready system.

| file | purpose |
|---|---|
| `tss.cfg` | the validated nd100x configuration (sectioned INI): `[machine] rtc = wall`, `[boot] device = cdc`, `[runtime]` CDC and drum paths |
| `run-tss.sh` | starts nd100x with `--config=tss.cfg --mms=1 --rtc=wall` |
| `QUICKSTART.md` | becomes the `README.md` inside the release archive: install the emulator, run, log in, terminals, known limitations |
| `cdc-jumpstart.img.gz` | a disc that has already been through the bring-up (bootstrap installed, MINIT-formatted, cold-started, `SYSTEM` created, verified to boot from disc alone); since 2026-10-08 built from the `TEL10` (10-teletype) system (`make bringup-auto`). Shipped as `cdc.img` |
| `cdc-jumpstart-tel4.img.gz` | the previous jump-start disc, built from the single-user `TEL4` system (`make build TEL=4`). Not shipped |

The release archive contains exactly: `cdc.img` (the unpacked
`cdc-jumpstart.img.gz`), a blank 1 MB `drum.img`, `tss.cfg`, `run-tss.sh`
and `README.md` (this folder's `QUICKSTART.md`). No BPUN tapes are shipped.
**CI runs no emulator**: it builds and scores the system from source, but
the disc it ships is the committed jump-start image.

**Why `--mms=1` is not in `tss.cfg`:** nd100x 1.0.15 and later accept
`[machine] mms = 1`, but older builds reject the key and refuse the whole
file. TSS is 1973 code that programs the MMU the Paging-System-I way; under
the default MMS2 the machine boots and then hangs in the overlay loader.
