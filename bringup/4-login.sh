#!/bin/bash
# STEP 4 — normal boot, to reach the login prompt and log in as SYSTEM.
#
# No --opr this time: the OPR test takes the normal-boot branch straight to LEV2
# -> LOGON. SYSTEM already exists on the disc (created by 3-create-system.sh).
# We still enter at address 7 (the real cold-start).
#
# At the LOCAL CONSOLE you should see, and type:
#     @ENTER SYSTEM              <- type SYSTEM, press Enter
#     PROJECT NUMBER P-1         <- type a POSITIVE number (e.g. 1), press Enter
#     OK
#     @                          <- the command processor; you are logged in
#
#  * SYSTEM is passwordless, so no PASSWORD prompt.
#  * A blank/zero project number re-prompts — enter a positive number.
#
# NOTE (2026-07-23): over the DAP debugger the interactive login currently stalls
# after the name echo (see docs/TSS-LOGIN-FLOW.md "Where a login could stall").
# Trying it in the LOCAL CONSOLE here is one of the things to validate.
source "$(dirname "$0")/_common.sh"

[ -f "$CDC" ] || { echo "Run steps 1-3 first." >&2; exit 1; }

echo
echo ">>> Normal boot. At @ENTER type: SYSTEM <Enter>  then  1 <Enter>"
echo
exec "$ND" --boot=bpun --image="$BPUN" --cdc="$CDC" --drum="$DRUM" --start=7
