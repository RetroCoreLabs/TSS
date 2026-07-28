#!/bin/bash
# --- relocated to scripts/<group>/: restore mac-c/ as the working directory
cd "$(dirname "$0")/../.." || exit 1
# Isolate the TSS1 buffer-allocation region (the code between the last
# matching symbol SECHS and GOOM=*) and measure how many words it consumes,
# so it can be compared against the size implied by the golden ASYMB values.
set -u
SRC=../src/TSS1.SYMB
OUT=/tmp/probe.symb

{
  echo '0/'
  echo 'START=*'
  # the BSS macro exactly as TSS1 defines it
  echo ')MCDEF BSS $A1'
  echo '*+$A1 /'
  echo ']'
  # the region: from just after )KILL GOOM (line 647) to just before GOOM=*
  sed -n '648,757p' "$SRC" | tr -d '\r'
  echo 'SIZE=*-START'
  echo ')LINE'
} > "$OUT"

build/mac -m CDC -m MACF -m DIAB -m K14 -m TEL4 \
  -d TDBI=410 -d TDBO=40 -d 8LP=200 -d 9PPT=100 -d 9TTI=40 -d 9TTO=40 \
  -d 9FP=200 -d 9CR=5 -d NOPEN=16 \
  "$OUT" -l /tmp/probe.list 2>/dev/null

echo "--- region size measured by mac ---"
grep -E '^ *SIZE=' /tmp/probe.list | tr -d ' \r'
echo
echo "--- expected component sizes (octal -> decimal) ---"
echo "9TTI=40o=32  TDBI=410o=264  9TTO=40o=32  TDBO=40o=32"
echo "9PPT=100o=64 9FP=200o=128   8LP=200o=128 9CR=5o=5"
echo "TTI block (BB1,BB2,BB3=TDBI,BB4) = 32+32+264+32 = 360"
echo "TTO block (BB17,BB18,BB19=TDBO,BB20) = 32+32+32+32 = 128"
echo "misc (9PPT+9FP+8LP+9CR+9SP) = 64+128+128+5+9SP = 325+9SP"
echo "total expected = 360+128+325+9SP = 813+9SP decimal"
echo
echo "--- is 9SP given a default in TSS1? ---"
grep -n '9SP' "$SRC" | head
