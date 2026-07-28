#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Compare the symbol table produced by mac-c against the archived
# ASYMB:SYMB golden dump from the original 1978 MAC build.
# Usage: scripts/verify/compare_asymb.sh <produced.list> <golden.txt>
set -u
PROD="$1"
GOLD="$2"

# normalise: keep '=' lines, strip all whitespace and CR, sort
tr -d ' \t\r' < "$GOLD" | grep '=' | sort > /tmp/gold.norm
tr -d ' \t\r' < "$PROD" | grep '=' | sort > /tmp/mine.norm

echo "golden symbols : $(wc -l < /tmp/gold.norm)"
echo "produced       : $(wc -l < /tmp/mine.norm)"
echo "exact matches  : $(comm -12 /tmp/gold.norm /tmp/mine.norm | wc -l)"
echo "missing (golden only) : $(comm -23 /tmp/gold.norm /tmp/mine.norm | wc -l)"
echo "extra   (mine only)   : $(comm -13 /tmp/gold.norm /tmp/mine.norm | wc -l)"
echo
echo "--- first 20 missing (in golden, not produced) ---"
comm -23 /tmp/gold.norm /tmp/mine.norm | head -20
echo
echo "--- first 20 extra (produced, not in golden) ---"
comm -13 /tmp/gold.norm /tmp/mine.norm | head -20
echo
echo "--- name-only comparison (value mismatches) ---"
cut -d= -f1 /tmp/gold.norm | sort -u > /tmp/gold.names
cut -d= -f1 /tmp/mine.norm | sort -u > /tmp/mine.names
echo "names in both : $(comm -12 /tmp/gold.names /tmp/mine.names | wc -l)"
echo "names missing : $(comm -23 /tmp/gold.names /tmp/mine.names | wc -l)"
comm -12 /tmp/gold.names /tmp/mine.names > /tmp/both.names
WRONG=0
while read -r n; do
    gv=$(grep -m1 "^${n}=" /tmp/gold.norm | cut -d= -f2)
    mv=$(grep -m1 "^${n}=" /tmp/mine.norm | cut -d= -f2)
    if [ "$gv" != "$mv" ]; then
        WRONG=$((WRONG+1))
        if [ "$WRONG" -le 20 ]; then
            echo "  $n golden=$gv produced=$mv"
        fi
    fi
done < /tmp/both.names
echo "value mismatches among shared names: $WRONG"
