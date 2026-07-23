#!/usr/bin/env python3
"""Is SINTRAN-L-1:DATA source code, or compiled system + patch script?

Decide by looking at what the text regions actually contain and what the
binary regions are (BPUN blocks loaded by )9READ carry machine code, not
source).
"""
import re

P = '/mnt/d/ND/extract/VSXL1/SINTRAN-L-1.DATA'
d = open(P, 'rb').read()
txt = bytes(b & 0x7F for b in d).decode('latin-1')

print("=== 1. what a patch macro body looks like ===")
i = txt.find(')MCDEF PCCST')
print(txt[i:i + 700] if i > 0 else "  PCCST not found")

print("\n=== 2. what surrounds a )9READ ===")
for m in list(re.finditer(r'\)9READ', txt))[:2]:
    a = max(0, m.start() - 200)
    seg = txt[a:m.start() + 260]
    print("  ---")
    print("  " + seg.replace('\n', '\n  ')[:460])

print("\n=== 3. binary vs text distribution (32 KB buckets) ===")
BUCK = 32768
for b in range(0, len(d), BUCK):
    chunk = bytes(x & 0x7F for x in d[b:b + BUCK])
    pr = sum(1 for c in chunk if 32 <= c < 127 or c in (9, 10, 13))
    pct = 100 * pr // max(1, len(chunk))
    bar = '#' * (pct // 5)
    print(f"  {b//1024:5d}K {pct:3d}% {bar}")

print("\n=== 4. does it contain SINTRAN SOURCE-style code? ===")
# real MAC source has labels 'NAME,' and instruction mnemonics in column form
labels = re.findall(r'(?m)^([A-Z][A-Z0-9]{1,4}),\s', txt)
mnem = re.findall(r'(?m)\b(LDA|STA|JMP|JPL|COPY|RADD|SKP|IOX|MON)\b', txt)
print(f"  label-like 'NAME,' lines : {len(labels)}")
print(f"  instruction mnemonics    : {len(mnem)}")
print(f"  '=' symbol definitions   : {len(re.findall(r'(?m)^[A-Z][A-Z0-9]*=', txt))}")

print("\n=== 5. sample of the plain-text command lines ===")
n = 0
for line in txt.splitlines():
    t = line.strip()
    if not t or t.startswith('%'):
        continue
    if re.fullmatch(r'[\x20-\x7e]+', t) and len(t) < 80:
        print(f"  {t}")
        n += 1
        if n >= 30:
            break
