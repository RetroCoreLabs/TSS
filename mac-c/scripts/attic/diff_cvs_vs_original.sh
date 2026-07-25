#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# What did "CVS" change? Compare the 1973 originals (archive/TSSn.ORG) with
# the patched version (archive/TSSn.SYMB), both parity-stripped and with
# whitespace normalised so only real edits show.
cd /mnt/e/Dev/Ronny/TSS || exit 1
strip () { perl -pe 's/(.)/chr(ord($1)&0x7f)/ge' "$1" | tr -d '\r' \
           | sed 's/[ \t]\+/ /g; s/ *$//'; }

total=0
echo "=== per-file edit counts ==="
for n in 1 2 3 4 5; do
    strip "archive/TSS$n.ORG"  > /tmp/o
    strip "archive/TSS$n.SYMB" > /tmp/s
    c=$(diff /tmp/o /tmp/s | grep -c '^<')
    total=$((total+c))
    printf "  TSS%-2s %4s changed lines\n" "$n" "$c"
done
echo "  TOTAL $total changed lines"

echo
echo "=== every distinct word that was replaced ==="
for n in 1 2 3 4 5; do
    strip "archive/TSS$n.ORG"  > /tmp/o
    strip "archive/TSS$n.SYMB" > /tmp/s
    diff /tmp/o /tmp/s
done > /tmp/alldiff
# pair each '<' original line with its '>' replacement and report the tokens
awk '
    /^< / { old = substr($0,3); getline sep;
            if (sep ~ /^---/) { getline nw; new = substr(nw,3);
              no = split(old, A, /[^A-Za-z0-9$]+/);
              nn = split(new, B, /[^A-Za-z0-9$]+/);
              if (no == nn) for (i=1;i<=no;i++) if (A[i] != B[i])
                  printf "%s -> %s\n", A[i], B[i];
            } }
' /tmp/alldiff | sort | uniq -c | sort -rn

echo
echo "=== sample context (first 3 edits in TSS3) ==="
strip archive/TSS3.ORG > /tmp/o; strip archive/TSS3.SYMB > /tmp/s
diff /tmp/o /tmp/s | head -12

echo
echo "=== do the replaced names collide with MAC built-ins? ==="
for sym in STR LSS XTR XSS XTRX LSSX; do
    if grep -q "\"$sym\"" mac-c/mac_permsym.c; then
        echo "  $sym : IS a MAC permanent symbol (would collide)"
    else
        echo "  $sym : not a MAC permanent symbol"
    fi
done
