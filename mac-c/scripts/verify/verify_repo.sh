#!/bin/bash
# End-to-end check after the reorganisation: build, test, rebuild TSS, score
# against the oracle, and verify every markdown link resolves.
cd /mnt/e/Dev/Ronny/TSS || exit 1

echo "=== layout ==="
for d in archive src reference derived Build mac-c tools bringup docs ppt; do
    printf "  %-10s %3s files  %s\n" "$d" \
        "$(ls -1 "$d" 2>/dev/null | wc -l)" \
        "$([ -e "$d/README.md" ] && echo 'README ok' || echo 'NO README')"
done
printf "  %-10s %s\n" "(root)" \
    "$([ -e README.md ] && echo 'README ok' || echo 'NO README')"

echo
echo "=== no duplicate source copies ==="
# Build/ legitimately holds working copies staged under the ND names MAC
# expects; it is regenerated output, so it is not a duplication problem.
dup=0
for f in src/TSS*.SYMB; do
    b=$(basename "$f")
    for other in reference derived; do
        [ -e "$other/$b" ] && { echo "  duplicate: $other/$b"; dup=1; }
    done
done
[ "$dup" -eq 0 ] && echo "  none (Build/ staging copies are output, ignored)"

echo
echo "=== build + test ==="
cd mac-c || exit 1
make >/dev/null 2>&1 || { echo "  BUILD FAILED"; exit 1; }
build/test_mac 2>/dev/null | tail -1
scripts/verify/check_coverage.sh 2>/dev/null | tail -1

echo
echo "=== rebuild TSS from the 1973 scripts ==="
scripts/build/build_tss_assysa.sh 2>&1 | grep -E 'exit status|errors ' | sed 's/^/  /'
echo
echo "=== score against the oracle ==="
echo "  version A:"
scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB 2>/dev/null \
    | head -5 | sed 's/^/    /'
echo "  version B:"
scripts/verify/compare_asymb.sh ../Build/BSYMB.SYMB ../reference/BSYMB.SYMB 2>/dev/null \
    | head -5 | sed 's/^/    /'

echo
echo "=== markdown links ==="
cd ..
bad=0
while IFS= read -r line; do
    src=${line%%:*}; tgt=${line#*:}
    dir=$(dirname "$src")
    [ -e "$dir/$tgt" ] || { echo "  BROKEN  $src -> $tgt"; bad=$((bad+1)); }
done < <(grep -rhoE '^|\]\([^)#][^)]*\)' --include='*.md' . 2>/dev/null >/dev/null; \
         grep -rnoE '\]\(([^)#][^)]*)\)' --include='*.md' . 2>/dev/null \
         | sed -E 's/^\.\///; s/:[0-9]+:\]\(/:/; s/\)$//' \
         | grep -v '^mac-c/README.md:https' | grep -v ':http')
[ "$bad" -eq 0 ] && echo "  all links resolve"
