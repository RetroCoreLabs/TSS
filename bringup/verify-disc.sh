#!/bin/sh
# Validate the bring-up state of the CDC disc image.
#   - the MIB free-track bitmap (MINIT lays it down; GTRK clears a bit per user)
#   - whether the SYSTEM user name has been written into USTBL
#
# Usage:  ./verify-disc.sh [path-to-cdc.img]
# Default path: ../Build/bringup/cdc.img
#
# The MIB (Master Information Block) sits at disc DKBIT=50 octal, which DKADR
# maps to physical sector 104 decimal (octal 150). Sector 0 of the MIB is the
# free-track bit table: a SET bit = a free/good track, a CLEAR bit = used/bad.
cd "$(dirname "$0")/.."

IMG="${1:-Build/bringup/cdc.img}"
SEC=512
MIB_SEC=104

[ -f "$IMG" ] || { echo "no disc image: $IMG (run: make prepare)"; exit 1; }

SIZE=$(wc -c < "$IMG")
echo "disc image : $IMG"
echo "size       : $SIZE bytes ($((SIZE / SEC)) sectors of 256 words)"

# Free tracks = number of set bits in the MIB sector; words 18/19 for display.
eval "$(od -An -tu1 -j $((MIB_SEC * SEC)) -N $SEC "$IMG" | awk '
    { for (i = 1; i <= NF; i++) { b[n++] = $i
          for (v = $i; v; v = int(v / 2)) bits += v % 2 } }
    END { printf "SETBITS=%d W18=%d W19=%d\n", bits,
          b[36] * 256 + b[37], b[38] * 256 + b[39] }')"
printf "MIB@sector %d: word18=%06o word19=%06o  free tracks=%d\n" \
    "$MIB_SEC" "$W18" "$W19" "$SETBITS"
echo "   (16 = freshly formatted, none allocated; each created user clears 1 bit)"

# The SYSTEM user name ('SYSTEM' = 53 59 53 54 45 4D) is written into the
# USTBL user table, which lives just above the MIB (physical sectors ~105-111).
FOUND=$(dd if="$IMG" bs=$SEC skip=$MIB_SEC count=40 2>/dev/null \
        | grep -abo SYSTEM 2>/dev/null | head -1 | cut -d: -f1)
if [ -n "$FOUND" ]; then
    S=$((MIB_SEC + FOUND / SEC))
    printf "SYSTEM user: FOUND in USTBL at sector %d (oct %o), byte %d\n" \
        "$S" "$S" "$((FOUND % SEC))"
else
    echo "SYSTEM user: not found (run: make coldstart)"
fi

echo ""
if [ "$SETBITS" -eq 0 ] && [ -z "$FOUND" ]; then
    echo "STATE: prepared, not yet formatted -> run: make format"
elif [ "$SETBITS" -eq 16 ] && [ -z "$FOUND" ]; then
    echo "STATE: formatted, no users yet     -> run: make coldstart"
elif [ "$SETBITS" -gt 0 ] && [ "$SETBITS" -lt 16 ] && [ -n "$FOUND" ]; then
    echo "STATE: SYSTEM created              -> run: make login"
else
    SYS=no; [ -n "$FOUND" ] && SYS=yes
    echo "STATE: $SETBITS free tracks, SYSTEM=$SYS (non-standard range is fine if you widened MINIT LAST)"
fi
