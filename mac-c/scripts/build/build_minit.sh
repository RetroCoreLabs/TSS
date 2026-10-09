#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Build the standalone MINIT disc formatter (MASS STORAGE INITIALIZATION
# PROGRAM, src/MINIT.SYMB) to a bootable BPUN paper tape for nd100x.
#
# MINIT is NOT part of the TSS OS image - it is the operator tool that lays
# down the MIB free-track bitmap on a bare CDC disc *before* the first TSS
# cold-boot. Without it, SINIT's GTRK finds no free tracks and cannot create
# user SYSTEM (see docs/TSS-BRINGUP.md).
#
# It is a self-contained program: entry is the symbol MINIT, which prompts
# for FIRST/LAST NCR disc addresses and I/U/R, then writes the bitmap.
#
# The source carries the CDC mark itself (MINIT.SYMB line 8) and selects
# NORD-1 vs NORD-10 I/O from the N10 library mark:
#   MINIT.SYMB top:  NN10 ; "N10 )KILL NN10 " -> NN10 = NOT N10
#   "NN10 block (line 798) = NORD-1 IOT/SKA I/O
#   default block      (line 783) = NORD-10 IOX I/O
# We set N10 (bare-symbol line, exactly as ASSYS-DRUM sets "... DRUM N10")
# so the NORD-10 IOX path is compiled in and the NORD-1 path is killed.
#
# Output goes under Build/minit/ (tracked binaries, like Build/drum/).
set -u

HERE=$(cd "$(dirname "$0")/../.." && pwd)
ROOT=$(cd "$HERE/.." && pwd)
BUILD="$ROOT/Build/minit"
MAC="$HERE/build/mac"

mkdir -p "$BUILD"
# Clean previous MINIT outputs (this whole subfolder is disposable).
find "$BUILD" -maxdepth 1 -type f -delete

# )9ASSM resolves "MINIT" as MINIT:SYMB, so present the source under that name,
# CR stripped (the archive/src files are CRLF).
tr -d '\r' < "$ROOT/src/MINIT.SYMB" > "$BUILD/MINIT.SYMB"

# The MINIT command stream: set the N10 mark (NORD-10 IOX I/O), assemble MINIT.
# List goes to LISTM:SYMB; no object symbol-dump is needed (0 = dummy device).
cat > "$BUILD/ASSYS-MINIT.SYMB" <<'STREAM'
% MINIT NORD-10 (ND-100) build - project-made command stream, not an original.
% N10 selects the IOX I/O path; MINIT.SYMB already carries the CDC mark.
% The object device is MSYMB:SYMB and )LIST dumps the symbol table there, so
% MINIT's entry address (needed to boot the formatter) is recorded.
N10
)9ASSM MINIT,"LISTM:SYMB","MSYMB:SYMB"
)LIST
)9EXIT
STREAM

echo "=== MINIT command stream being executed ==="
cat "$BUILD/ASSYS-MINIT.SYMB"
echo "==========================================="
echo

cd "$BUILD"
# -e MINIT: record MINIT's address as the BPUN autostart, so the emulator
# enters the formatter directly.
# -b : bootable BPUN tape (what the emulator loads)
# -o : flat MACIMG image (for range/vector inspection)
"$MAC" ASSYS-MINIT.SYMB -b minit.bpun -o minit.img -e MINIT \
    >build.out 2>build.err
status=$?
echo "  exit status : $status"
echo "  errors      : $(grep -c ERROR build.err || true)"
grep ERROR build.err | head -20

echo
if [ -e minit.bpun ]; then
    echo "  BPUN tape  : $(wc -c < minit.bpun) bytes  ($BUILD/minit.bpun)"
else
    echo "  BPUN tape  : NOT PRODUCED"
fi
if [ -e minit.img ]; then
    echo "  MACIMG     : $(wc -c < minit.img) bytes  ($BUILD/minit.img)"
fi

# Range + start-vector check (MACIMG header = 'MACIMG' + base + count).
if [ -e minit.img ]; then
    "$HERE/scripts/verify/macimg_info.sh" minit.img
fi
