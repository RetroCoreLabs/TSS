#!/usr/bin/env python3
"""Characterise SINTRAN-L-1:DATA - the SINTRAN III VSX generation stream."""
import re
from collections import Counter

P = '/mnt/d/ND/extract/VSXL1/SINTRAN-L-1.DATA'
d = open(P, 'rb').read()
txt = bytes(b & 0x7F for b in d)          # strip ND parity

print(f"file: {len(d)} bytes, {len(d)//2} words")

# how much is plain text vs binary?
printable = sum(1 for b in txt if 32 <= b < 127 or b in (9, 10, 13))
print(f"printable after parity strip: {printable} "
      f"({100*printable//len(txt)}%)  -> it is a TEXT/command stream")

s = txt.decode('latin-1')

print("\n=== MACM / MAC commands present (count) ===")
cmds = Counter(re.findall(r'\)([A-Z0-9]{2,6})', s))
for c, n in cmds.most_common(30):
    print(f"  ){c:<7} {n}")

print("\n=== macro definitions ()MCDEF) ===")
for m in re.findall(r'\)MCDEF\s+(\S+)', s)[:40]:
    print(f"  {m}")

print("\n=== '!' start commands (address! = start execution) ===")
for m in sorted(set(re.findall(r'(?m)^\s*(\d+)\s*!', s)))[:20]:
    print(f"  {m}!")

print("\n=== version / banner lines ===")
for line in s.splitlines():
    t = line.strip()
    if re.search(r'SINTRAN|VERSION|VSX|ND-100|ND-500|L07|250305', t) \
       and len(t) < 100 and t:
        print(f"  {t}")
        if sum(1 for _ in ()) > 0:
            break

print("\n=== structure: first 40 non-comment, non-blank lines ===")
n = 0
for line in s.splitlines():
    t = line.strip()
    if not t or t.startswith('%'):
        continue
    print(f"  {t[:95]}")
    n += 1
    if n >= 40:
        break
