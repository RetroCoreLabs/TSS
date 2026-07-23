#!/usr/bin/env bash
# dkadr_trace.sh -- capture the running DKADR logical->physical pairs.
# Boots the DRUM+N10 TSS on nd100x with --trace and extracts, in order:
#   * DKADR entry region (PC 010006..010060) with A/T registers
#   * every IOX 503 (CDC block-address load) with the A register
# so we can read the true logical (T at DKADR entry) -> physical (A at IOX 503)
# mapping off the LIVE, now-correctly-compiled DKADR.
set -uo pipefail
ND=${ND:-$HOME/repos/nd100x/build/bin/nd100x}
BUILD=/mnt/e/Dev/Ronny/TSS/Build/drum
MAXI=${1:-4000000}
OUT=${OUT:-/mnt/e/Dev/Ronny/TSS/mac-c/dkadr_trace_$$.txt}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cp "$BUILD/tss-drum.img" "$T/drum.img"
cp "$BUILD/tss-cdc.img"  "$T/cdc.img"
# nd100x --trace writes the instruction trace to STDERR; keep it in OUT.
"$ND" --mms1 --boot=bpun --image="$BUILD/tss-drum.bpun" \
      --drum="$T/drum.img" --cdc="$T/cdc.img" \
      --start=000301 --trace --max-instr="$MAXI" 2> "$OUT" 1>/dev/null || true
echo "=== trace lines total: $(wc -l < "$OUT") ==="
echo "=== DKADR-region lines (PC 01000x..01005x) count ==="
grep -cE '^0100[0-5][0-9] ' "$OUT" || true
echo "=== IOX 503 lines (CDC block-address load) ==="
grep -nE 'IOX 503' "$OUT" | head -60 || true
echo "=== all distinct IOX opcodes seen ==="
grep -oE 'IOX [0-7]+' "$OUT" | sort | uniq -c | sort -rn | head -30 || true
