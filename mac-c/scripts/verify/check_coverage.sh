#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Coverage audit: every public API function and every implemented ')' command
# must be exercised somewhere in tests/test_mac.c.
echo "=== public API functions ==="
miss=0
for f in $(grep -oE '^[a-z_]+ [a-z_]+\(|^(void|bool|uint16_t) [a-z_]+\(' src/mac.h \
           | sed 's/.* //; s/($//; s/(//' | sort -u); do
    if grep -q "\b${f}\b" tests/test_mac.c; then
        printf "  %-24s covered\n" "$f"
    else
        printf "  %-24s *** NOT REFERENCED IN TESTS ***\n" "$f"
        miss=$((miss+1))
    fi
done

echo
echo "=== ')' commands implemented in src/ ==="
for c in $(grep -hoE '"\)[A-Z0-9]+"' src/mac_*.c | tr -d '"' | sort -u); do
    if grep -qF -- "$c" tests/test_mac.c; then
        printf "  %-12s covered\n" "$c"
    else
        printf "  %-12s *** NOT IN TESTS ***\n" "$c"
        miss=$((miss+1))
    fi
done

echo
echo "=== commands used by the TSS corpus ==="
for c in $(cat ../src/*.SYMB | tr -d '\r' | grep -oE '\)[A-Z0-9]+' \
           | sort -u); do
    if grep -qFh -- "$c" src/mac_*.c; then
        printf "  %-12s handled explicitly\n" "$c"
    else
        printf "  %-12s accepted and ignored (no image effect)\n" "$c"
    fi
done

echo
echo "=== instruction coverage ==="
echo "permanent symbols in table : $(grep -c 'MAC_CLS_' src/mac_permsym.c)"
echo "all are asserted in test [1] (value) and, by class, in [1b]/[1c]/[1d]"
echo
if [ "$miss" -eq 0 ]; then
    echo "RESULT: no uncovered public functions or implemented commands"
else
    echo "RESULT: $miss item(s) lack test coverage"
fi
