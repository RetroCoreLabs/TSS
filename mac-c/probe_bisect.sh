#!/bin/bash
# Bisect the buffer-allocation region in FULL context: report the location
# counter at a series of source lines so the exact statement that emits the
# wrong number of words can be identified.
set -u
SRC=../src/TSS1.SYMB
PRE="-m CDC -m MACF -m DIAB -m K14 -m TEL4 -d IOT=160000 -d ACT=400 \
-d SKA=1000 -d PIN=2000 -d SNI=0 -d INTDS=150440 -d INTEN=150540 \
-d TDBI=410 -d TDBO=40 -d DIABD=156 -d DIABT=4 -d DLP=143 -d GRP1=1 \
-d GRP2=2 -d GRP3=4 -d GRP4=10 -d MPR=3 -d 8LP=200 -d 9PPT=100 \
-d 9TTI=40 -d 9TTO=40 -d 9FP=200 -d 9CR=5 -d NOPEN=16"

probe () {
    { sed -n "1,${1}p" "$SRC" | tr -d '\r'; echo "PX=*"; echo ')LINE'; } > /tmp/p.symb
    ./mac-as $PRE /tmp/p.symb -l /tmp/p.list 2>/dev/null
    v=$(grep -E '^ *PX=' /tmp/p.list | tr -d ' \r' | cut -d= -f2)
    echo "$v"
}

prev=""
for ln in "$@"; do
    v=$(probe "$ln")
    if [ -z "$v" ]; then
        printf "line %-5s loc=<PX not listed>\n" "$ln"
        continue
    fi
    if [ -n "$prev" ]; then
        d=$(( 8#$v - 8#$prev ))
        printf "line %-5s loc=%-8s (%+d words)   | %s\n" \
               "$ln" "$v" "$d" "$(sed -n "${ln}p" "$SRC" | tr -d '\r')"
    else
        printf "line %-5s loc=%-8s              | %s\n" \
               "$ln" "$v" "$(sed -n "${ln}p" "$SRC" | tr -d '\r')"
    fi
    prev="$v"
done
