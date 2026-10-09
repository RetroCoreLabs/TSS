#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Walk the golden ASYMB dump in definition order and report the first symbol
# whose produced value differs. Matching is by NAME (the produced list may
# contain extra symbols), so the report localises the construct that emits
# the wrong number of words.
set -u
PROD="$1"
GOLD="$2"
tr -d ' \t\r' < "$GOLD" | grep '=' > /tmp/g.ord
tr -d ' \t\r' < "$PROD" | grep '=' > /tmp/m.ord

n=0
while IFS= read -r gline; do
    n=$((n+1))
    name=${gline%%=*}
    gval=${gline#*=}
    mval=$(grep -m1 "^${name}=" /tmp/m.ord | cut -d= -f2)
    if [ -z "$mval" ]; then
        echo "entry $n: $name MISSING from produced (golden=$gval)"
        continue
    fi
    if [ "$gval" != "$mval" ]; then
        echo "FIRST VALUE MISMATCH at golden entry $n"
        echo "  $name  golden=$gval  produced=$mval"
        echo "--- preceding 6 golden entries (all matched) ---"
        sed -n "$((n-6)),$((n-1))p" /tmp/g.ord
        exit 0
    fi
done < /tmp/g.ord
echo "no value mismatches across $n golden entries"
