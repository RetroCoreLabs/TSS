#!/bin/bash
# Which assembler build implements the overlay commands TSS needs?
# Scans each candidate BPUN's symbol/command table.
B=/mnt/d/ND/BPUN
for f in "$B/MAC.BPUN" "$B/MACM-1718L.BPUN" "$B/MACM-1718K.BPUN" \
         "$B/FMAC-1408D.BPUN"; do
    echo "############################################################"
    python3 scan_bpun_commands.py "$f" 9MOVE SOVER 8DUMP 9TSS 9ASSM \
        CLOAD GJEM HENT BPOUND 9CTOM
    echo
done
