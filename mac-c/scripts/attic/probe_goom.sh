#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# The idiom at TSS1:583..646 is
#     GOOM=*      save the location counter
#     <bare symbol lines that emit throwaway words>
#     GOOM/       restore it
# so the block must be NET ZERO. Measure the counter before and after.
set -u
SRC=../src/TSS1.SYMB
PRE="-m CDC -m MACF -m DIAB -m K14 -m TEL4 -d IOT=160000 -d ACT=400 \
-d SKA=1000 -d PIN=2000 -d SNI=0 -d INTDS=150440 -d INTEN=150540 \
-d TDBI=410 -d TDBO=40 -d DIABD=156 -d DIABT=4 -d DLP=143 -d GRP1=1 \
-d GRP2=2 -d GRP3=4 -d GRP4=10 -d MPR=3 -d 8LP=200 -d 9PPT=100 \
-d 9TTI=40 -d 9TTO=40 -d 9FP=200 -d 9CR=5 -d NOPEN=16"

probe () {  # $1 = last source line to include, $2 = probe symbol name
    { sed -n "1,${1}p" "$SRC" | tr -d '\r'; echo "$2=*"; echo ')LINE'; } > /tmp/p.symb
    build/mac-c $PRE /tmp/p.symb -l /tmp/p.list 2>/dev/null
    grep -E "^ *$2=" /tmp/p.list | tr -d ' \r'
}

echo "counter just BEFORE the GOOM block (line 582): $(probe 582 PA)"
echo "counter just AFTER  the GOOM block (line 646): $(probe 646 PB)"
echo "counter after )KILL GOOM           (line 647): $(probe 647 PC)"
echo
echo "If the block is net-zero, PA and PB must be equal."
