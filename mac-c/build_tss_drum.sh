#!/bin/bash
# Build the NORD-10 + swapping-DRUM variant of TSS and emit a bootable BPUN
# paper tape, for loading into an ND-100 emulator (nd100x).
#
# This drives the derived DRUM+N10 command stream
# (derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB) through mac-as exactly as ASSYSA
# is driven in build_tss_assysa.sh: )9ASSM assembles each TSS part into the
# in-memory image with time-ordered ()KILL) scope. Unlike the ASSYSA build,
# which only wants the symbol dump, here we also ask mac-as for:
#   -b  the bootable BPUN tape  (what the emulator loads)
#   -o  the flat MACIMG image   (for inspection / range checking)
#
# The DRUM variant is EXPERIMENTAL (see docs/BUILD-TSS.md drum notes and
# docs/DRUM-DEVICE-SPEC.md): XDRUM talks to a drum controller at IOX 540.
# Output goes under Build/drum/ so it never collides with the golden ASSYSA
# build in Build/.
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
BUILD="$ROOT/Build/drum"
INPUT="$ROOT/derived/ASSYS-DRUM-N10-MAC-INPUT.SYMB"
MAC="$HERE/mac-as"

mkdir -p "$BUILD"
# Clean previous drum outputs (this whole subfolder is disposable).
find "$BUILD" -maxdepth 1 -type f -delete

# )9ASSM resolves "TSS1" as TSS1:SYMB, so present the sources under those names.
for n in 1 2 3 4 5; do
    cp "$ROOT/src/TSS$n.SYMB" "$BUILD/TSS$n.SYMB"
done

# The DRUM+N10 command stream, CRLF stripped, minus nothing else - it already
# carries its own marks and symbol definitions.
tr -d '\r' < "$INPUT" > "$BUILD/ASSYS-DRUM.SYMB"

echo "=== DRUM+N10 command stream being executed ==="
cat "$BUILD/ASSYS-DRUM.SYMB"
echo "=============================================="
echo

cd "$BUILD"
# -e ISTRT: record TSS's cold-start (ISTRT=025076) as the BPUN autostart, so an
# emulator that reads the start address from the tape enters TSS correctly
# without needing a separate --start override. See docs/RUNNING-TSS-ON-EMULATOR.md.
# -c tss-cdc.img: also emit the CDC-disc overlay image. )9MOVE (inside the
# "MACF OVERX macro) stages each overlay into its VOR window during assembly;
# mac_write_cdc_disc then places each overlay on CDC-disc sectors OVDK+2n so
# the running TSS overlay reader (routine S5) finds them. The overlay->sector
# table it prints (to build.err) lets the boot test verify the mapping.
# See docs/OVERLAY-DISC-SPEC.md.
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
python3 - <<'PY'
import struct
d = open('tss-drum.img', 'rb').read()
if d[:6] != b'MACIMG':
    print("  (image has no MACIMG header - cannot range-check)")
else:
    base, count = struct.unpack('>HH', d[6:10])
    words = [struct.unpack('>H', d[10+2*i:12+2*i])[0] for i in range(count)]
    def at(a):
        i = a - base
        return words[i] if 0 <= i < count else None
    top = base + count - 1
    nz = sum(1 for x in words if x)
    print(f"  image range: {base:06o}-{top:06o}  ({count} words, {nz} non-zero, {100*nz//count}%)")
    v = at(7)
    # location 7 is TSS's boot vector: LDT *+3; JMP I *+1; SYSSV; CORLD.
    # LDT *+3 assembles to 050003; a correct image must have it at word 7.
    if v is not None:
        print(f"  loc 7 boot vector: {v:06o}  ({'OK = LDT *+3' if v==0o50003 else 'UNEXPECTED'})")
    else:
        print("  loc 7 not present in image")
PY
fi
