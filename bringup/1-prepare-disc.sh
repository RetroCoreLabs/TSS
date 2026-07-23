#!/bin/bash
# STEP 1 — prepare a fresh, correctly-sized CDC disc for a TSS bring-up.
#
# The emulated CDC device does NOT grow on writes: it bounds every transfer
# against the backing-file size. The TSS user area (FSYS = NCR 4470 octal) maps
# to CDC sector ~6288, so the raw tss-cdc.img (~459 sectors) is far too small and
# MINIT would get DISK ERROR on every user track. We pad the backing file to
# 8192 sectors (4 MB); padding only appends zeros, so the overlays are untouched.
#
# Usage:  ./1-prepare-disc.sh          (from WSL)
source "$(dirname "$0")/_common.sh"

for f in "$TSS_BPUN" "$TSS_CDC" "$MINIT_BPUN"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: missing $f — build first:" >&2
        echo "  cd $ROOT/mac-c && make && ./build_tss_drum.sh && ./build_minit.sh" >&2
        exit 1
    fi
done

mkdir -p "$WORK"
cp "$TSS_BPUN" "$BPUN"
cp "$TSS_CDC"  "$CDC"
truncate -s 4M "$CDC"                       # 8192 sectors, covers the user area
head -c 1048576 /dev/zero > "$DRUM"         # blank 1 MB swap drum

echo
echo "Prepared a fresh disc set in $WORK:"
ls -la "$WORK"
echo
echo "Next:  ./2-format-minit.sh    (lay down the free-track bitmap)"
