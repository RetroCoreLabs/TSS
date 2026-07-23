#!/bin/bash
# Which assembler variant matches the TSS golden dumps?
# Appendix E of ND-60.096.01: the 32-bit float build has 2OR3=2, LDR=LDD
# (024000), STR=STD (020000); the 48-bit build has 2OR3=3, LDR=LDF (034000),
# STR=STF (030000). My reconciliation of the archived ASYMB dump concluded
# the '[' constants there occupy TWO words, i.e. a 32-bit-float assembler.
echo "variant marker values: 32-bit => 2OR3=2 LDR=024000 STR=020000"
echo "                       48-bit => 2OR3=3 LDR=034000 STR=030000"
echo
for f in /mnt/d/ND/BPUN/MAC.BPUN \
         /mnt/d/ND/BPUN/FMAC-1408D.BPUN \
         /mnt/d/ND/BPUN/MACM-1718L.BPUN \
         /mnt/d/ND/c3/2024/x2/system/f48mac-1408d.prog \
         /mnt/d/ND/c3/2024/x2/system/fmac-1920c.prog \
         /mnt/d/ND/S/K03/F32-FMAC-1920CPROG \
         /mnt/d/ND/c3/2024/x2/system/mac-1628c.prog \
         /mnt/d/ND/c3/2024/x2/system/dmac-1915g.bpun; do
    [ -e "$f" ] || continue
    python3 scan_raw_table.py "$f" 2OR3 LDR STR 9MOVE 9TSS SOVER 8DUMP \
        | sed 's/^/  /'
    echo
done
