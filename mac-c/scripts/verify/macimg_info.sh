#!/bin/sh
# Report on a MACIMG memory image (header = 'MACIMG' + base word + count word,
# both big-endian, then <count> big-endian words starting at address <base>).
#
# Usage: macimg_info.sh IMAGE [--check-loc7]
#
# --check-loc7 additionally verifies TSS's boot vector: location 7 must hold
# LDT *+3 = 050003 (the vector is LDT *+3; JMP I *+1; SYSSV; CORLD).
IMG="$1"
CHECK7="$2"
[ -f "$IMG" ] || { echo "  (no image: $IMG)"; exit 0; }

od -An -v -tu1 "$IMG" | awk -v check7="$CHECK7" '
    { for (i = 1; i <= NF; i++) b[n++] = $i }
    END {
        magic = sprintf("%c%c%c%c%c%c", b[0], b[1], b[2], b[3], b[4], b[5])
        if (magic != "MACIMG") {
            print "  (image has no MACIMG header - cannot range-check)"
            exit
        }
        base  = b[6] * 256 + b[7]
        count = b[8] * 256 + b[9]
        nz = 0
        for (i = 0; i < count; i++) {
            w[i] = b[10 + 2 * i] * 256 + b[11 + 2 * i]
            if (w[i]) nz++
        }
        printf "  image range: %06o-%06o  (%d words, %d non-zero, %d%%)\n", \
            base, base + count - 1, count, nz, count ? 100 * nz / count : 0
        if (check7 == "--check-loc7") {
            i = 7 - base
            if (i >= 0 && i < count)
                printf "  loc 7 boot vector: %06o  (%s)\n", w[i], \
                    w[i] == 20483 ? "OK = LDT *+3" : "UNEXPECTED"
            else
                print "  loc 7 not present in image"
        }
    }'
