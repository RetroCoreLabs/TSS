#!/bin/bash
# STEP 2 — format the CDC disc with MINIT (lay down the MIB free-track bitmap).
#
# This launches MINIT in the emulator's LOCAL CONSOLE. Type the dialogue yourself:
#
#     MASS STORAGE INIT
#     FIRST DISK ADDRESS (NCR): 4470      <- start of the user area (FSYS)
#     LAST  DISK ADDRESS (NCR): 4670      <- 16 free tracks (widen for more users)
#     INITIALIZE OR UPDATE OR REGENERATE (I OR U OR R): I
#     INITIALIZE
#     FINISHED
#
#  * Type the numbers in OCTAL, press Enter after each.
#  * When you see FINISHED, STOP the emulator with Ctrl-C — that flushes the CDC
#    surface back to cdc.img. (If you kill it any other way the format is lost.)
#
# Then verify with:  python3 verify-disc.py     (expect: 16 free tracks)
#
# Prefer scripted/automated? Use ./debug-with-dap.sh instead and drive it over the
# DAP debugger (see README.md "Driving it over DAP").
source "$(dirname "$0")/_common.sh"

[ -f "$CDC" ] || { echo "Run ./1-prepare-disc.sh first." >&2; exit 1; }

echo
echo ">>> MINIT — type: 4470 <Enter>  4670 <Enter>  I <Enter>  then Ctrl-C at FINISHED"
echo
exec "$ND" --boot=bpun --image="$MINIT_BPUN" --cdc="$CDC"
