#!/usr/bin/env python3
"""Check which ROBJ RFN-jump encoding a build artifact contains.

The TSS2 ROBJ error exits end with the byte-adjacent word sequence
    SAA 4 (170404=F104) ; JMP RFN+2 ; SAA 11 (170411=F109) ; JMP RFN+2
FIXED  (addend kept):    ... F104 A805 F109 A803 ...
BROKEN (addend dropped): ... F104 A803 F109 A801 ...
Scans the raw big-endian word stream of each file given.
"""
import sys

FIXED = bytes.fromhex("F104A805F109A803")
BROKEN = bytes.fromhex("F104A803F109A801")

for path in sys.argv[1:]:
    data = open(path, "rb").read()
    tags = []
    if FIXED in data:
        tags.append("FIXED")
    if BROKEN in data:
        tags.append("BROKEN")
    if not tags:
        tags.append("pattern not found")
    print(f"{path}: {' + '.join(tags)}")
