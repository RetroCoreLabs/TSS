#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Assemble the TSS sources exactly as ASSYSA does and compare the resulting
# symbol table against the archived ASYMB:SYMB golden dump.
# Usage: ./run_tss.sh [files...]   (defaults to all five TSS parts)
set -u
PRE="-m CDC -m MACF -m DIAB -m K14 -m TEL4 -d IOT=160000 -d ACT=400 \
-d SKA=1000 -d PIN=2000 -d SNI=0 -d INTDS=150440 -d INTEN=150540 \
-d TDBI=410 -d TDBO=40 -d DIABD=156 -d DIABT=4 -d DLP=143 -d GRP1=1 \
-d GRP2=2 -d GRP3=4 -d GRP4=10 -d MPR=3 -d 8LP=200 -d 9PPT=100 \
-d 9TTI=40 -d 9TTO=40 -d 9FP=200 -d 9CR=5 -d NOPEN=16"

if [ $# -gt 0 ]; then
    FILES="$*"
else
    FILES="../src/TSS1.SYMB ../src/TSS2.SYMB ../src/TSS3.SYMB \
../src/TSS4.SYMB ../src/TSS5.SYMB"
fi

# Everything lands in Build/ - the single output folder.
# stdout carries the )WRITE output produced by TSS's own OVERX macro
# ()WRTM + )WRITE $A1 prints each overlay's size); keep it out of the way.
OUT=../Build
mkdir -p "$OUT"
build/mac-c $PRE $FILES -l "$OUT/asymb.list" -u \
    > "$OUT/write.txt" 2> "$OUT/err.txt"
echo "errors    : $(grep -c ERROR "$OUT/err.txt")"
echo "undefined : $(grep -c UNDEFINED "$OUT/err.txt")"
echo "symbols   : $(grep -c '=' "$OUT/asymb.list")"
echo "output    : $OUT/"
