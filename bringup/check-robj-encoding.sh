#!/bin/sh
# Check which ROBJ RFN-jump encoding a build artifact contains.
#
# The TSS2 ROBJ error exits end with the byte-adjacent word sequence
#     SAA 4 (170404=F104) ; JMP RFN+2 ; SAA 11 (170411=F109) ; JMP RFN+2
# FIXED  (addend kept):    ... F104 A805 F109 A803 ...
# BROKEN (addend dropped): ... F104 A803 F109 A801 ...
# Scans the raw big-endian word stream of each file given.
#
# Usage: ./check-robj-encoding.sh FILE...
for f in "$@"; do
    [ -f "$f" ] || { echo "$f: no such file"; continue; }
    HEX=$(od -An -v -tx1 "$f" | tr -d ' \n')
    TAGS=""
    case "$HEX" in *f104a805f109a803*) TAGS="FIXED";; esac
    case "$HEX" in *f104a803f109a801*) TAGS="$TAGS${TAGS:+ + }BROKEN";; esac
    echo "$f: ${TAGS:-pattern not found}"
done
