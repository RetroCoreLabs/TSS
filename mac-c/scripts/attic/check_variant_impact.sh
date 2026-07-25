#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# My mac_permsym.c was extracted from MAC.BPUN, which is the 48-bit float
# build. TSS was assembled with FMAC-1920C, the 32-bit build, where three
# entries differ. Does the corpus actually use them?
cd /mnt/e/Dev/Ronny/TSS || exit 1
echo "=== values embedded in mac-c (taken from MAC.BPUN, 48-bit) ==="
grep -E '"(LDR|STR|2OR3|LDF|STF|LDD|STD)"' mac-c/mac_permsym.c | sed 's/^/  /'
echo
echo "=== correct values for FMAC-1920C (32-bit), which built TSS ==="
echo "  LDR = 024000 (= LDD)   STR = 020000 (= STD)   2OR3 = 2"
echo
echo "=== uses in the TSS corpus (whole-word) ==="
for s in LDR STR 2OR3 LDF STF LDD STD; do
    n=$(grep -how "$s" src/*.SYMB 2>/dev/null | wc -l)
    printf "  %-5s %s uses\n" "$s" "$n"
done
echo
echo "=== and in the 1973 originals (before the CVS STR->XTR rename) ==="
for s in LDR STR 2OR3; do
    n=$(cat archive/original-text/*.ORG 2>/dev/null | grep -how "$s" | wc -l)
    printf "  %-5s %s uses\n" "$s" "$n"
done
