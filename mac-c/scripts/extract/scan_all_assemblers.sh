#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Which assembler build implements the overlay commands TSS needs?
# Scans each candidate BPUN's symbol/command table.
# External ND binaries are NOT in the repo. Point ND_BPUN_DIR at them.
B=${ND_BPUN_DIR:?set ND_BPUN_DIR to the directory holding MAC.BPUN etc.}
for f in "$B/MAC.BPUN" "$B/MACM-1718L.BPUN" "$B/MACM-1718K.BPUN" \
         "$B/FMAC-1408D.BPUN"; do
    echo "############################################################"
    python3 scripts/extract/scan_bpun_commands.py "$f" 9MOVE SOVER 8DUMP 9TSS 9ASSM \
        CLOAD GJEM HENT BPOUND 9CTOM
    echo
done
