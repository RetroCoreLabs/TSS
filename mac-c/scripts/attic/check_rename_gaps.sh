#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# The CVS rename looks incomplete. For each old/new name, show where it is
# DEFINED (label "NAME," or "NAME=" or a DATA declaration) versus merely
# REFERENCED, in the patched sources.
cd /mnt/e/Dev/Ronny/TSS || exit 1
for n in STR XTR STRX XTRX STR0 XTR0 STR1 XTR1 STR2 XTR2 LSS XSS LSSX \
         STR1X STR2X; do
    d=$(grep -h -c -E "(^|[ \t;])$n[,=]|DATA $n," src/TSS*.SYMB 2>/dev/null \
        | paste -sd+ | bc)
    r=$(grep -h -o -w "$n" src/TSS*.SYMB 2>/dev/null | wc -l)
    printf "  %-6s defined:%-3s totalrefs:%-4s" "$n" "${d:-0}" "$r"
    if [ "${d:-0}" -eq 0 ] && [ "$r" -gt 0 ]; then
        echo "  <-- REFERENCED BUT NEVER DEFINED"
    else
        echo
    fi
done
echo
echo "=== the same names in the 1973 originals (parity-stripped) ==="
for n in STR STRX STR1 STR2 LSS; do
    c=$(perl -pe 's/(.)/chr(ord($1)&0x7f)/ge' archive/TSS2.ORG archive/TSS3.ORG \
        archive/TSS4.ORG archive/TSS5.ORG | grep -o -w "$n" | wc -l)
    printf "  %-6s %s references\n" "$n" "$c"
done
