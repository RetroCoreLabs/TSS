#!/bin/bash
# Binary-search TSS3 for the first line at which a bare '##' undefined
# symbol appears.
SRC=../src/TSS3.SYMB
TOTAL=$(wc -l < "$SRC")
lo=1
hi=$TOTAL
has_hash () {   # $1 = number of lines to include
    { sed -n "1,${1}p" "$SRC" | tr -d '\r'; echo ')LINE'; } > /tmp/b.symb
    ./mac-as /tmp/b.symb -u >/dev/null 2>/tmp/b.txt
    grep -q '^UNDEFINED: ##$' /tmp/b.txt
}
if ! has_hash "$TOTAL"; then
    echo "no bare '##' in the whole file"
    exit 0
fi
while [ $lo -lt $hi ]; do
    mid=$(( (lo + hi) / 2 ))
    if has_hash "$mid"; then hi=$mid; else lo=$((mid+1)); fi
done
echo "first appears when including line $lo:"
sed -n "${lo}p" "$SRC" | tr -d '\r' | cat -A | head -1
