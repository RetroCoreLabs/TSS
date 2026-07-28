#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Locate which source file (and roughly which line) still yields a bare '##'
# undefined symbol, by assembling each part on its own.
PRE="-m CDC -m MACF -m DIAB -m K14 -m TEL4 -d IOT=160000 -d ACT=400 \
-d SKA=1000 -d PIN=2000 -d SNI=0 -d INTDS=150440 -d INTEN=150540 \
-d TDBI=410 -d TDBO=40 -d DIABD=156 -d DIABT=4 -d DLP=143 -d GRP1=1 \
-d GRP2=2 -d GRP3=4 -d GRP4=10 -d MPR=3 -d 8LP=200 -d 9PPT=100 \
-d 9TTI=40 -d 9TTO=40 -d 9FP=200 -d 9CR=5 -d NOPEN=16"
for f in ../src/TSS1.SYMB ../src/TSS2.SYMB ../src/TSS3.SYMB \
         ../src/TSS4.SYMB ../src/TSS5.SYMB; do
    build/mac $PRE "$f" -u >/dev/null 2>/tmp/one.txt
    if grep -q '^UNDEFINED: ##$' /tmp/one.txt; then
        echo "bare '##' produced by $f"
    fi
done
echo "--- lines whose '#' has fewer than two characters after it ---"
for f in ../src/TSS*.SYMB; do
    awk -v F="$f" '{ line=$0; sub(/\r$/,"",line);
        n=length(line);
        for (i=1; i<=n; i++) {
            if (substr(line,i,1)=="#" && n-i < 2) {
                printf "%s:%d: [%s]\n", F, NR, line; break } } }' "$f"
done
