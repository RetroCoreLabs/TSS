#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Compare specific symbol values between the produced list and the golden
# ASYMB dump. Usage: scripts/verify/cmp_syms.sh <produced.list> <golden.txt> SYM [SYM...]
PROD="$1"; shift
GOLD="$1"; shift
tr -d ' \t\r' < "$GOLD" > /tmp/g.flat
tr -d ' \t\r' < "$PROD" > /tmp/m.flat
printf "%-8s %-10s %-10s %s\n" SYMBOL GOLDEN PRODUCED DELTA
for s in "$@"; do
    g=$(grep -m1 "^${s}=" /tmp/g.flat | cut -d= -f2)
    m=$(grep -m1 "^${s}=" /tmp/m.flat | cut -d= -f2)
    if [ -n "$g" ] && [ -n "$m" ]; then
        d=$(( 8#$m - 8#$g ))
    else
        d="-"
    fi
    printf "%-8s %-10s %-10s %s\n" "$s" "${g:-none}" "${m:-none}" "$d"
done
