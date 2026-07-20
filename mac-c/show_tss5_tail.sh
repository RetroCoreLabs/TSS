#!/bin/bash
# Show how TSS5 ends and which library-mark regions are still open there.
S=../src/TSS5.SYMB
echo "=== last 20 lines ==="
tail -20 "$S" | tr -d '\r' | cat -n
echo
echo "=== last 8 mark lines (lines beginning with a double quote) ==="
grep -n '^"' "$S" | tr -d '\r' | tail -8
