#!/usr/bin/env python3
# Validate the bring-up state of the CDC disc image.
#   - the MIB free-track bitmap (MINIT lays it down; GTRK clears a bit per user)
#   - whether the SYSTEM user name has been written into USTBL
#
# Usage:  python3 verify-disc.py [path-to-cdc.img]
# Default path: ../Build/bringup/cdc.img
import struct, sys, os

path = sys.argv[1] if len(sys.argv) > 1 else \
    os.path.join(os.path.dirname(__file__), "..", "Build", "bringup", "cdc.img")

d = open(path, "rb").read()
SEC = 512
print(f"disc image : {path}")
print(f"size       : {len(d)} bytes ({len(d)//SEC} sectors of 256 words)")

# The MIB (Master Information Block) sits at disc DKBIT=50 octal, which DKADR
# maps to physical sector 104 (octal 150). Sector 0 of the MIB is the free-track
# bit table: a SET bit = a free/good track, a CLEAR bit = used/bad.
MIB_SEC = 104
mib = d[MIB_SEC*SEC:(MIB_SEC+1)*SEC]
setbits = sum(bin(x).count("1") for x in mib)
w18 = struct.unpack_from(">H", mib, 18*2)[0]
w19 = struct.unpack_from(">H", mib, 19*2)[0]
print(f"MIB@sector {MIB_SEC}: word18={w18:06o} word19={w19:06o}  free tracks={setbits}")
print("   (16 = freshly formatted, none allocated; each created user clears 1 bit)")

# The SYSTEM user name ('SYSTEM' = 53 59 53 54 45 4D) is written into the USTBL
# user table, which lives just above the MIB (physical sectors ~105-111).
needle = b"SYSTEM"
found = None
for s in range(MIB_SEC, MIB_SEC + 40):
    off = d[s*SEC:(s+1)*SEC].find(needle)
    if off >= 0:
        found = (s, off); break
if found:
    print(f"SYSTEM user: FOUND in USTBL at sector {found[0]} (oct {found[0]:o}), byte {found[1]}")
else:
    print("SYSTEM user: not found (run 3-create-system.sh)")

print()
if setbits == 0 and not found:
    print("STATE: prepared, not yet formatted -> run 2-format-minit.sh")
elif setbits == 16 and not found:
    print("STATE: formatted, no users yet     -> run 3-create-system.sh")
elif 0 < setbits < 16 and found:
    print("STATE: SYSTEM created              -> run 4-login.sh")
else:
    print(f"STATE: {setbits} free tracks, SYSTEM={'yes' if found else 'no'} "
          "(non-standard range is fine if you widened MINIT LAST)")
