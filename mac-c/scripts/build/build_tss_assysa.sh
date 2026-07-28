#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Build TSS the way the 1978 operator did: run the ASSYSA command stream
# itself through mac-c. ASSYSA drives everything with )9ASSM, so this
# exercises the stream model, nested source includes, ND file naming and
# the ")LIST -> object stream" rule, rather than us feeding files in order.
set -u
# Build/ is the single output folder for everything the assembler produces:
# the copied sources MAC needs under their ND names, the LISTn listing
# streams, and the ASYMB/BSYMB symbol dumps. It is disposable - regenerate
# it by re-running this script.
BUILD=../Build
mkdir -p "$BUILD"
# Clean generated files only - Build/README.md is checked in and explains
# what this folder is, so it must survive a rebuild.
find "$BUILD" -maxdepth 1 -type f ! -name 'README.md' -delete

# MAC resolves "TSS1" as TSS1:SYMB, so present the sources under those names
for n in 1 2 3 4 5; do
    cp "../src/TSS$n.SYMB" "$BUILD/TSS$n.SYMB"
done

# The real ASSYSA, minus the three operator lines that start FMAC itself
# (FMAC / 100 / SYSA) which are not MAC input. Everything else verbatim.
tail -n +4 ../src/ASSYSA.SYMB | tr -d '\r' > "$BUILD/ASSYSA.SYMB"

echo "=== ASSYSA command stream being executed ==="
cat "$BUILD/ASSYSA.SYMB"
echo "============================================"
echo

# ASSYSB is the DEBUG (version B) build; same treatment, and it drops the
# trailing "@cc" operator line as well as the three startup lines.
tail -n +4 ../src/ASSYSB.SYMB | tr -d '\r' | grep -v '^@cc' \
    > "$BUILD/ASSYSB.SYMB"

MAC=$PWD/build/mac-c
cd "$BUILD"

run () {   # $1 = script, $2 = symbol dump it should produce
    echo "=== running $1 ==="
    "$MAC" "$1" >write_$1.txt 2>err_$1.txt
    echo "  exit status : $?"
    echo "  errors      : $(grep -c ERROR err_$1.txt || true)"
    grep ERROR err_$1.txt | head -5
    if [ -s "$2" ]; then
        echo "  $2 : $(wc -c < "$2") bytes, $(grep -c '=' "$2") symbols"
    else
        echo "  $2 : EMPTY OR MISSING"
    fi
}

run ASSYSA ASYMB.SYMB
echo
run ASSYSB BSYMB.SYMB
echo
echo "files produced:"
ls -la *.SYMB *.txt 2>/dev/null | awk '{printf "  %-16s %s\n", $9, $5}'
