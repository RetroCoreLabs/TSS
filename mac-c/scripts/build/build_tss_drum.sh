#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Build the NORD-10 + swapping-DRUM variant of TSS and emit a bootable BPUN
# paper tape, for loading into an ND-100 emulator (nd100x).
#
# This drives the derived DRUM+N10 command stream
# (derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB) through mac-c exactly as ASSYSA
# is driven in build_tss_assysa.sh: )9ASSM assembles each TSS part into the
# in-memory image with time-ordered ()KILL) scope. Unlike the ASSYSA build,
# which only wants the symbol dump, here we also ask mac-c for:
#   -b  the bootable BPUN tape  (what the emulator loads)
#   -o  the flat MACIMG image   (for inspection / range checking)
#
# The DRUM variant is EXPERIMENTAL (see docs/TSS-BRINGUP.md drum notes and
# docs/TSS-ARCHITECTURE.md (drum chapter)): XDRUM talks to a drum controller at IOX 540.
# Output goes under Build/drum/ so it never collides with the golden ASSYSA
# build in Build/.
set -u

HERE=$(cd "$(dirname "$0")/../.." && pwd)
ROOT=$(cd "$HERE/.." && pwd)
BUILD="$ROOT/Build/drum"
INPUT="$ROOT/derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB"
MAC="$HERE/build/mac"

mkdir -p "$BUILD"
# Clean previous drum outputs (this whole subfolder is disposable).
find "$BUILD" -maxdepth 1 -type f -delete

# )9ASSM resolves "TSS1" as TSS1:SYMB, so present the sources under those names.
for n in 1 2 3 4 5; do
    cp "$ROOT/src/TSS$n.SYMB" "$BUILD/TSS$n.SYMB"
done

# The DRUM+N10 command stream, CRLF stripped. It carries its own marks and
# symbol definitions; the only thing rewritten here is the teletype count.
#
# TEL=<n> (default 10) selects the TELn mark (src/TSS1.SYMB:127-146 maps
# TEL4/6/10/12/.../24 to NTTY) and sets NTY, the number of non-modem
# terminals, to the same value. NTY must equal NTTY on nd100x: the level-12
# and level-10 ident dispatch (TSS1 LEXT) maps ident 44+k to TTY5+k only
# while k < NTY-4, and nd100x's TERMINAL 5-10 answer idents 44-51.
# TEL=4 reproduces the original single-user (console only) build.
TEL=${TEL:-10}
case "$TEL" in
    4|6|10|12|14|16|18|20|22|24) ;;
    *) echo "build_tss_drum.sh: TEL=$TEL is not a TSS mark (4 6 10 12 ... 24)" >&2; exit 1 ;;
esac
NTY_OCT=$(printf '%o' "$TEL")
tr -d '\r' < "$INPUT" \
    | sed -e "s/^CDC MACF DIAB K14 TEL10 DRUM N10 CLKFX\$/CDC MACF DIAB K14 TEL$TEL DRUM N10 CLKFX/" \
          -e "s/^NTY=12;/NTY=$NTY_OCT;/" \
    > "$BUILD/ASSYS-DRUM.SYMB"
echo "=== teletypes: TEL=$TEL (mark TEL$TEL, NTY=$NTY_OCT octal) ==="

echo "=== DRUM+N10 command stream being executed ==="
cat "$BUILD/ASSYS-DRUM.SYMB"
echo "=============================================="
echo

cd "$BUILD"
# -e ISTRT: record TSS's cold-start (ISTRT=025076) as the BPUN autostart, so an
# emulator that reads the start address from the tape enters TSS correctly
# without needing a separate --start override. See docs/TSS-BRINGUP.md.
# -c tss-cdc.img: also emit the CDC-disc overlay image. )9MOVE (inside the
# "MACF OVERX macro) stages each overlay into its VOR window during assembly;
# mac_write_cdc_disc then places each overlay on CDC-disc sectors OVDK+2n so
# the running TSS overlay reader (routine S5) finds them. The overlay->sector
# table it prints (to build.err) lets the boot test verify the mapping.
# See docs/TSS-ARCHITECTURE.md (overlay chapter).
"$MAC" ASSYS-DRUM.SYMB -b tss-drum.bpun -o tss-drum.img -c tss-cdc.img -e ISTRT \
    >build.out 2>build.err
status=$?
echo "  exit status : $status"
echo "  errors      : $(grep -c ERROR build.err || true)"
grep ERROR build.err | head -10

echo
if [ -e tss-drum.bpun ]; then
    echo "  BPUN tape  : $(wc -c < tss-drum.bpun) bytes  ($BUILD/tss-drum.bpun)"
else
    echo "  BPUN tape  : NOT PRODUCED"
fi
if [ -e tss-drum.img ]; then
    echo "  MACIMG     : $(wc -c < tss-drum.img) bytes  ($BUILD/tss-drum.img)"
fi
if [ -e tss-cdc.img ]; then
    echo "  CDC image  : $(wc -c < tss-cdc.img) bytes  ($BUILD/tss-cdc.img)"
    # Show the overlay->sector table the writer emitted (first few rows).
    grep -A6 'CDC overlay disc image' build.err | head -8
else
    echo "  CDC image  : NOT PRODUCED"
fi

# Range + boot vector check (the MACIMG header is 'MACIMG' + base + count).
if [ -e tss-drum.img ]; then
    "$HERE/scripts/verify/macimg_info.sh" tss-drum.img --check-loc7
fi
