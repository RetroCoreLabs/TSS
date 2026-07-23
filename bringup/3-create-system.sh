#!/bin/bash
# STEP 3 — cold-start TSS and create the SYSTEM user.
#
# THE KEY FACT: we start at ADDRESS 7 (the real disk-boot cold-start vector),
# NOT at the BPUN's own autostart (ISTRT). ISTRT is the normal-running re-entry
# and SKIPS the operator-switch (OPR) test that decides to run SINIT. Address 7
# runs SYSSV -> INIT -> scheduler -> S10 -> the OPR test.
#
#   --opr=131313  presets the operator's-panel switches -> S10 branches to SINIT,
#                 which creates user SYSTEM (passwordless).
#   --start=7     enters at the real cold-start (overrides the ISTRT autostart).
#
# This launches in the LOCAL CONSOLE. On success every terminal prints:
#     @ENTER
# SYSTEM has been created on the disc. STOP with Ctrl-C to flush, then verify:
#     python3 verify-disc.py       (expect: 15 free tracks + SYSTEM on disc)
#
# Do a full first-boot ONCE with this script; afterwards use ./4-login.sh
# (normal boot) to log in.
source "$(dirname "$0")/_common.sh"

[ -f "$CDC" ] || { echo "Run ./1-prepare-disc.sh and ./2-format-minit.sh first." >&2; exit 1; }

echo
echo ">>> Cold-start (create SYSTEM). Watch for @ENTER, then Ctrl-C to flush the disc."
echo
exec "$ND" --boot=bpun --image="$BPUN" --cdc="$CDC" --drum="$DRUM" \
     --opr=131313 --start=7
