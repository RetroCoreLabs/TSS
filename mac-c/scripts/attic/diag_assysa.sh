#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Diagnose why the ASSYSA stream stops before its final )LIST.
set -u
B=/tmp/tssbuild
cd "$B" || exit 1
MAC=build/mac

rm -f LIST*.SYMB ASYMB.SYMB w.txt e.txt
"$MAC" A2 > w.txt 2> e.txt
echo "exit=$?"
echo
echo "--- files present afterwards ---"
for f in LIST1.SYMB LIST2.SYMB LIST3.SYMB LIST4.SYMB LIST5.SYMB \
         ASYMB.SYMB w.txt e.txt; do
    if [ -e "$f" ]; then
        printf "  %-14s %s bytes\n" "$f" "$(wc -c < "$f")"
    else
        printf "  %-14s MISSING\n" "$f"
    fi
done
echo
echo "--- where did REACHED-LIST go? ---"
grep -l 'REACHED-LIST' LIST*.SYMB ASYMB.SYMB w.txt e.txt 2>/dev/null \
    || echo "  nowhere: the )9MSG line was never executed"
echo
echo "--- last 3 lines of LIST5.SYMB ---"
tail -3 LIST5.SYMB 2>/dev/null
echo
echo "--- stderr ---"
cat e.txt
