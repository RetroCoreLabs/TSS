#!/usr/bin/env python3
"""Analyse the contents of the SINTRAN VSX boot floppy VSXL1.IMG.

Two files were extracted with ndtool:
  MACM-1718L:BPUN   the mass-storage assembler
  SINTRAN-L-1:DATA  the SINTRAN III VSX system

Compares the floppy's MACM against the standalone copy (as loaded programs,
not as tapes), and characterises the DATA file.
"""
import sys


def parse_bpun(path):
    """Return (base, words) of the binary block, skipping leader+bootstrap."""
    d = open(path, 'rb').read()
    i = 0
    while i < len(d) and d[i] == 0:
        i += 1
    while i < len(d) and d[i] != ord('!'):
        i += 1
    i += 1
    if i + 4 > len(d):
        return None, [], 0
    base = (d[i] << 8) | d[i + 1]
    cnt = (d[i + 2] << 8) | d[i + 3]
    i += 4
    w = []
    for _ in range(cnt):
        if i + 1 >= len(d):
            break
        w.append((d[i] << 8) | d[i + 1])
        i += 2
    chk = (d[i] << 8) | d[i + 1] if i + 1 < len(d) else None
    return base, w, chk


A = '/mnt/d/ND/extract/VSXL1/MACM-1718L.BPUN'
B = '/mnt/d/ND/BPUN/MACM-1718L.BPUN'

print("=== MACM: floppy copy vs standalone copy, as LOADED PROGRAMS ===")
for tag, p in (("floppy    ", A), ("standalone", B)):
    base, w, chk = parse_bpun(p)
    s = sum(w) & 0xFFFF
    print(f"  {tag}: base {base:06o}  {len(w)} words  "
          f"({base:06o}-{base + len(w) - 1:06o})  "
          f"checksum {chk:06o} {'OK' if s == chk else 'MISMATCH'}")

ba, wa, _ = parse_bpun(A)
bb, wb, _ = parse_bpun(B)
if ba == bb and len(wa) == len(wb):
    diff = [i for i, (x, y) in enumerate(zip(wa, wb)) if x != y]
    print(f"  same load address and length; {len(diff)} differing words")
    for i in diff[:12]:
        print(f"    {ba + i:06o}: floppy {wa[i]:06o}  standalone {wb[i]:06o}")
    if not diff:
        print("  -> the loaded programs are IDENTICAL "
              "(only the tape wrapper differs)")
else:
    print(f"  DIFFERENT programs: base {ba:06o}/{bb:06o}  "
          f"len {len(wa)}/{len(wb)}")

print()
print("=== SINTRAN-L-1:DATA ===")
d = open('/mnt/d/ND/extract/VSXL1/SINTRAN-L-1.DATA', 'rb').read()
print(f"  {len(d)} bytes = {len(d)/1024:.0f} KB = {len(d)//2} words "
      f"= {len(d)/2048:.1f} pages of 1K words")
print(f"  first 32 bytes: {' '.join(f'{b:02X}' for b in d[:32])}")

# is it a BPUN-style tape, or a raw image?
head = d[:600]
print(f"  contains '!' start marker in first 600 bytes: "
      f"{b'!' in head}")
nul = 0
while nul < len(d) and d[nul] == 0:
    nul += 1
print(f"  leading NUL bytes: {nul}")

# printable runs -> is there readable text (symbol names, banners)?
runs, cur = [], b''
for b in d[:200000]:
    c = b & 0x7F
    if 32 <= c < 127:
        cur += bytes([c])
    else:
        if len(cur) >= 6:
            runs.append(cur.decode('ascii', 'replace'))
        cur = b''
print(f"  readable strings in first 200 KB: {len(runs)}")
for s in runs[:25]:
    print(f"    {s}")
