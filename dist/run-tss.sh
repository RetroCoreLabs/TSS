#!/usr/bin/env bash
# Start NORD TSS 3.0 on nd100x with the validated settings.
#
#   ./run-tss.sh [/path/to/nd100x]
#
# Falls back to nd100x on PATH, or $ND100X.
set -euo pipefail
cd "$(dirname "$0")"

ND=${1:-${ND100X:-$(command -v nd100x || true)}}
if [ -z "$ND" ] || [ ! -x "$ND" ]; then
    echo "nd100x not found. Build it from https://github.com/HackerCorpLabs/nd100x" >&2
    echo "then: ./run-tss.sh /path/to/nd100x   (or set ND100X)" >&2
    exit 1
fi

for f in cdc.img drum.img tss.cfg; do
    [ -f "$f" ] || { echo "missing $f - run this from the unpacked release" >&2; exit 1; }
done

echo "Log in as SYSTEM, project 1, no password."
echo "Date prompt format: DD,MM,YYYY,HH,MM,SS"
echo
# --mms=1 is also set in tss.cfg ([machine] mms = 1, read by nd100x 1.0.15
# and later); the flag is kept here for older builds.
# --rtc=wall makes the clock interrupt fire every 20 ms of real time, so DATE
# keeps real time. nd100x defaults to wall since 2026-10-08; the flag is here
# for older builds, whose "ticks" default ran the TSS clock about 7x too fast.
exec "$ND" --config=tss.cfg --mms=1 --rtc=wall
