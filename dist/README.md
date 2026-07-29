# dist/ — what ships in a release

These files are packaged with the built disc images by
[`.github/workflows/release.yml`](../.github/workflows/release.yml) whenever a
`v*` tag is pushed. Nothing here is generated; they are maintained by hand.

| file | purpose |
|---|---|
| `tss.cfg` | the validated nd100x configuration — boot type, image, CDC and drum paths |
| `run-tss.sh` | starts nd100x with `--mms=1`, which the config file cannot express |
| `QUICKSTART.md` | becomes the `README.md` inside the release archive: install the emulator, run, log in, known limitations |

The release archive also contains `tss.bpun`, `cdc.img` and `drum.img`, taken
from `Build/bringup/` after a clean `make auto`.

**Why `--mms=1` is not in `tss.cfg`:** nd100x accepts `mms` only as a
command-line flag. TSS is 1973 code that programs the MMU the
Paging-System-I way; under the default MMS2 the machine boots and then hangs
in the overlay loader.

**Why there is no `tss.ini`:** nd100x's sectioned `.ini` describes a machine
whose `[boot] device` is a disc controller. TSS is booted from a BPUN tape
image, and its CDC disc has no boot sector, so the INI form cannot express
this setup. The flat `--config=` file can.
