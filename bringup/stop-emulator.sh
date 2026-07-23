#!/bin/bash
# Stop the nd100x bring-up emulator on the TSS DAP port with SIGINT (= Ctrl-C),
# which flushes the CDC surface back to cdc.img, and wait for it to exit.
# Only targets the nd100x instance on port 1777 (this project's reserved port),
# never any other emulator instance.
set -u
PORT="${DAP_PORT:-1777}"
PID=$(pgrep -f "nd100x.*port=${PORT}" | head -1)
if [ -z "$PID" ]; then
    echo "no nd100x on port ${PORT}"
    exit 0
fi
echo "SIGINT -> nd100x pid $PID"
kill -INT "$PID"
for i in $(seq 1 15); do
    sleep 1
    kill -0 "$PID" 2>/dev/null || { echo "nd100x exited after ${i}s"; exit 0; }
done
echo "STILL RUNNING after 15s (not killed further - flush may be pending)"
exit 1
