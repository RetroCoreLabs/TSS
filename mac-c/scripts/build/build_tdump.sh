#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Build the standalone TSS DUMPER (src/TDUMP.SYMB).
#
# TDUMP is NOT part of the TSS OS image and it is NOT a bootable program.
# It is a *user program that runs under TSS*: every I/O it does is a monitor
# call (the bare 1610xx words), so it needs a live TSS to execute. What it
# produces is the DISTRIBUTION TAPE - the self-loading paper tape that carries
# TSS to a bare machine. The hardware bootstrap it punches (HLOAD/TBOOT) is
# assembled here as data and copied to the tape by 8DUMP, never executed here.
#
# Library marks (declared in the source header, TDUMP.SYMB:6-18):
#   NCR (default) | CDC   - which disc controller
#   NN10 (default) | N10  - NORD-1 IOT/SKA I/O vs NORD-10 IOX I/O
#   A (default) | B       - which CORLD (core-load disc address) constant
#   DCHN                  - derived: 100 if NN10, 500 if N10
#
# We build CDC + N10 (the same combination as the TSS builds under
# scripts/build/build_tss_*.sh) so the DKRST/RDKOP/DBOOT disc-restart block
# at TDUMP.SYMB:264-327 is compiled in - that block is "CDC-only and is what
# writes the bootable disc pages.
#
# Output goes under Build/tdump/ (disposable, like Build/minit/).
set -u

HERE=$(cd "$(dirname "$0")/../.." && pwd)
ROOT=$(cd "$HERE/.." && pwd)
BUILD="$ROOT/Build/tdump"
MAC="$HERE/build/mac-as"

mkdir -p "$BUILD"
find "$BUILD" -maxdepth 1 -type f -delete

# )9ASSM resolves "TDUMP" as TDUMP:SYMB, so present the source under that name,
# CR stripped (the archive/src files are CRLF).
tr -d '\r' < "$ROOT/src/TDUMP.SYMB" > "$BUILD/TDUMP.SYMB"

cat > "$BUILD/ASSYS-TDUMP.SYMB" <<'STREAM'
% TDUMP CDC / NORD-10 build - project-made command stream, not an original.
% CDC selects the cartridge-disc controller and enables the DKRST/DBOOT block;
% N10 selects the IOX I/O path (and makes DCHN=500).
% The symbol table is dumped to TSYMB:SYMB so TDUMP's entry address is recorded.
CDC
N10
)9ASSM TDUMP,"LISTT:SYMB","TSYMB:SYMB"
)LIST
)9EXIT
STREAM

echo "=== TDUMP command stream being executed ==="
cat "$BUILD/ASSYS-TDUMP.SYMB"
echo "==========================================="
echo

cd "$BUILD"
"$MAC" ASSYS-TDUMP.SYMB -b tdump.bpun -o tdump.img -e TDUMP \
    >build.out 2>build.err
status=$?
echo "  exit status : $status"
echo "  errors      : $(grep -c ERROR build.err || true)"
grep ERROR build.err | head -20

echo
if [ -e tdump.bpun ]; then
    echo "  BPUN tape  : $(wc -c < tdump.bpun) bytes  ($BUILD/tdump.bpun)"
else
    echo "  BPUN tape  : NOT PRODUCED"
fi
if [ -e tdump.img ]; then
    echo "  MACIMG     : $(wc -c < tdump.img) bytes  ($BUILD/tdump.img)"
fi

if [ -e tdump.img ]; then
python3 - <<'PY'
import struct
d = open('tdump.img', 'rb').read()
if d[:6] != b'MACIMG':
    print("  (image has no MACIMG header - cannot range-check)")
else:
    base, count = struct.unpack('>HH', d[6:10])
    words = [struct.unpack('>H', d[10+2*i:12+2*i])[0] for i in range(count)]
    top = base + count - 1
    nz = sum(1 for x in words if x)
    print(f"  image range: {base:06o}-{top:06o}  ({count} words, {nz} non-zero, {100*nz//count if count else 0}%)")
PY
fi
