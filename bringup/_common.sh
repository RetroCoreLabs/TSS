#!/bin/bash
# Shared paths/config for the TSS bring-up scripts. Sourced by the others.
# Run everything from WSL (the nd100x binary and the POSIX tools live there).
set -eu

# Repo root = parent of this bringup/ directory.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Disposable working area for the bring-up disc images.
WORK="$ROOT/Build/bringup"

# The built artefacts (produced by mac-c/build_tss_drum.sh and build_minit.sh).
TSS_BPUN="$ROOT/Build/drum/tss-drum.bpun"
TSS_CDC="$ROOT/Build/drum/tss-cdc.img"
MINIT_BPUN="$ROOT/Build/minit/minit.bpun"

# Locate the nd100x emulator binary (override with: ND=/path/to/nd100x <script>).
ND="${ND:-}"
if [ -z "$ND" ]; then
    for cand in \
        "$HOME/repos/nd100x/build/bin/nd100x" \
        "$HOME/repos/nd100x/build-linux/bin/nd100x" \
        "/mnt/e/Dev/Emulators/ND/nd100x/build-linux/bin/nd100x"; do
        [ -x "$cand" ] && ND="$cand" && break
    done
fi
if [ -z "$ND" ] || [ ! -x "$ND" ]; then
    echo "ERROR: nd100x binary not found. Set ND=/path/to/nd100x and re-run." >&2
    exit 1
fi

# Working disc images (created by 1-prepare-disc.sh).
CDC="$WORK/cdc.img"
DRUM="$WORK/drum.img"
BPUN="$WORK/tss.bpun"

# DAP debugger port for TSS (see memory tss-dap-port).
DAP_PORT="${DAP_PORT:-1777}"

echo "  nd100x : $ND"
echo "  work   : $WORK"
