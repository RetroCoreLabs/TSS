#!/bin/bash
# Can the current build produce a bootable TSS? Check what actually exists
# and what the assembled image looks like.
cd /mnt/e/Dev/Ronny/TSS/mac-c || exit 1

echo "=== 1. does Build/ contain any image or tape today? ==="
ls -1 ../Build | grep -iE '\.(bpun|bin|img|core)$' || \
    echo "  none - Build/ holds only symbol dumps, listings and logs"

echo
echo "=== 2. do the build scripts ever ask for an image or tape? ==="
grep -nE '\-o |\-b ' scripts/build/build_tss_assysa.sh scripts/build/run_tss.sh || \
    echo "  no: neither script passes -o (image) or -b (BPUN) to mac-c"

echo
echo "=== 3. produce one now and see what comes out ==="
PRE="-m CDC -m MACF -m DIAB -m K14 -m TEL4 -d IOT=160000 -d ACT=400 \
-d SKA=1000 -d PIN=2000 -d SNI=0 -d INTDS=150440 -d INTEN=150540 \
-d TDBI=410 -d TDBO=40 -d DIABD=156 -d DIABT=4 -d DLP=143 -d GRP1=1 \
-d GRP2=2 -d GRP3=4 -d GRP4=10 -d MPR=3 -d 8LP=200 -d 9PPT=100 \
-d 9TTI=40 -d 9TTO=40 -d 9FP=200 -d 9CR=5 -d NOPEN=16"
build/mac-c $PRE ../src/TSS1.SYMB ../src/TSS2.SYMB ../src/TSS3.SYMB \
    ../src/TSS4.SYMB ../src/TSS5.SYMB \
    -o /tmp/tss.img -b /tmp/tss.bpun >/dev/null 2>/tmp/img.err
echo "  exit=$?  errors=$(grep -c ERROR /tmp/img.err)"
[ -e /tmp/tss.img ] && echo "  image: $(wc -c < /tmp/tss.img) bytes"
[ -e /tmp/tss.bpun ] && echo "  tape : $(wc -c < /tmp/tss.bpun) bytes"

echo
echo "=== 4. what address range does the image cover? ==="
python3 - <<'PY'
import struct
d=open('/tmp/tss.img','rb').read()
assert d[:6]==b'MACIMG'
base,count=struct.unpack('>HH',d[6:10])
print(f"  base {base:06o}  count {count:06o} ({count} words)  top {base+count-1:06o}")
w=[struct.unpack('>H',d[10+2*i:12+2*i])[0] for i in range(count)]
def at(a):
    i=a-base
    return w[i] if 0<=i<count else None
print(f"  TSS expects to occupy 000000-037777 (16K)")
print(f"  location 7 (boot vector, should be LDT *+3 = 050003): {at(7):06o}" if at(7) is not None else "  location 7 not in image")
for a in (0,1,2,3,6,7,8,9,10):
    v=at(a)
    print(f"    {a:06o}: {v:06o}" if v is not None else f"    {a:06o}: --")
nz=sum(1 for x in w if x)
print(f"  non-zero words: {nz} of {count} ({100*nz//count}%)")
PY

echo
echo "=== 5. the commands that place overlays on disk ==="
for c in ')9MOVE' ')SOVER' ')8DUMP'; do
    if grep -q "\"$c\"" mac.c; then echo "  $c implemented"
    else echo "  $c ACCEPTED AND IGNORED  <-- overlays are not written out"; fi
done
echo "  used in the corpus:"
grep -c ')9MOVE\|)SOVER\|)8DUMP' ../src/TSS*.SYMB | sed 's/^/    /'
