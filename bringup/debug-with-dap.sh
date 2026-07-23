#!/bin/bash
# Launch a TSS bring-up stage under the DAP debugger (for scripted/LLM control or
# a DAP client). The emulator holds at the entry until a debugger connects and
# continues. Console I/O is driven over DAP (terminal 192), NOT the local console.
#
# Usage:
#   ./debug-with-dap.sh minit     # MINIT formatter, port 1777
#   ./debug-with-dap.sh cold      # cold-start (--start=7 --opr=131313), create SYSTEM
#   ./debug-with-dap.sh login     # normal boot (--start=7), reach @ENTER
#
# Then connect a DAP client to 127.0.0.1:1777 and: enable console on terminal 192,
# attach, continue. Send input as raw bytes; a carriage return MUST be the byte
# 0x0D (a literal "\r" is echoed as backslash-r). Examples:
#   "SYSTEM"+CR  = hex 53 59 53 54 45 4D 0D
#   "4470"+CR    = hex 34 34 37 30 0D
# Key addresses (octal): LOGON 032736, SINIT 022057, CRUSE 031272, S10 07271,
#   OPR test 07307, XDISK 021137, DWAIT 010313, ABLKP 032444, MINIT 000206.
source "$(dirname "$0")/_common.sh"

mode="${1:-}"
case "$mode" in
  minit) IMG="$MINIT_BPUN"; EXTRA=() ;;
  cold)  IMG="$BPUN";       EXTRA=(--drum="$DRUM" --opr=131313 --start=7) ;;
  login) IMG="$BPUN";       EXTRA=(--drum="$DRUM" --start=7) ;;
  *) echo "Usage: $0 {minit|cold|login}" >&2; exit 1 ;;
esac

[ -f "$CDC" ] || { echo "Run ./1-prepare-disc.sh first." >&2; exit 1; }

echo
echo ">>> $mode under DAP on port $DAP_PORT — connect a debugger, then continue."
echo
exec "$ND" --boot=bpun --image="$IMG" --cdc="$CDC" "${EXTRA[@]}" \
     --debugger --port="$DAP_PORT"
