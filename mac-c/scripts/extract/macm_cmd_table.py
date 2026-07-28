#!/usr/bin/env python3
"""Decode MACM's command/symbol table and print Ghidra-ready addresses.

MACM inherits MAC's table format: 3-word entries [name_hi, name_lo, value],
name = up to 5 chars at 6 bits each. For command entries the value is the
handler address, which is what we need to drive the Ghidra work.

The BPUN loads at octal 077120 but Ghidra's image base is 0, so the Ghidra
address of an ND address A is simply A (both are the 16-bit word address).
"""
import sys

import os
# External ND binary, not in the repo: set ND_BPUN_DIR.
PATH = os.path.join(os.environ['ND_BPUN_DIR'], 'MACM-1718L.BPUN')


def load(path):
    d = open(path, 'rb').read()
    i = 0
    while i < len(d) and d[i] == 0:
        i += 1
    while i < len(d) and d[i] != ord('!'):
        i += 1
    i += 1
    base = (d[i] << 8) | d[i + 1]
    count = (d[i + 2] << 8) | d[i + 3]
    i += 4
    w = []
    for _ in range(count):
        w.append((d[i] << 8) | d[i + 1])
        i += 2
    return base, w


def name_of(hi, lo):
    v = (hi << 16) | lo
    s = ''
    for k in (4, 3, 2, 1, 0):
        c = (v >> (k * 6)) & 0x3F
        if c:
            s += chr(c + 0x40) if c < 0x20 else chr(c)
    return s


def ok(n):
    return 1 <= len(n) <= 5 and all(
        c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$,.-*/()" for c in n)


base, w = load(PATH)
print(f"# MACM-1718L: base {base:06o}, {len(w)} words, "
      f"{base:06o}-{base + len(w) - 1:06o}")

# find the longest 3-aligned run of decodable entries = the table
best = []
for off in range(3):
    run = []
    for i in range(off, len(w) - 2, 3):
        n = name_of(w[i], w[i + 1])
        if ok(n):
            run.append((base + i, n, w[i + 2]))
        else:
            if len(run) > len(best):
                best = run
            run = []
    if len(run) > len(best):
        best = run

print(f"# table: {len(best)} entries at {best[0][0]:06o}..{best[-1][0]:06o}")
print()

# MACM commands from ND-60.009.02, plus the MAC ones we already know
MACM_CMDS = {'GJEM', 'HENT', 'SAM', 'CMOVE', 'BPUND', 'CLOAD', 'FIX',
             'SYSDF', 'ULIST', 'LINE', 'XPUN', '9READ', '9BYTT', '9CTOM',
             '9SAVE', '9GET', 'RBP'}
MAC_CMDS = {'FILL', 'KILL', 'PCL', 'LIST', 'WRTM', 'NWRT', 'WRITE', 'WRUS',
            'WLOC', 'WMNE', 'BPUN', 'CLEAR', 'PRINT', 'PUNCH', 'ZERO',
            'CORE', 'CHANGE', 'SETSM', 'RESSM', 'MCDEF', 'LSTM',
            '9ASSM', '9EXIT', '9SET', '9LIB', '9MSG', '9MSGE', '9RT',
            '9LC', '9ASF', '9ADS', '9EOF', '9ENT', '9EXT', '9END', '9BEG',
            '9PARI', '9LITR', '9ASCI', '9FABS', '9TSS', '9MOVE', '9TABL'}

print("## MACM-specific commands (ND-60.009.02) -> handler address")
for a, n, v in best:
    if n in MACM_CMDS:
        print(f"  {n:<6} table@{a:06o}  handler {v:06o}  ghidra ram:{v:04x}")

print()
print("## MAC commands also present")
row = []
for a, n, v in best:
    if n in MAC_CMDS:
        row.append(f"{n}={v:06o}")
for i in range(0, len(row), 4):
    print("  " + "  ".join(row[i:i + 4]))

print()
print("## every entry whose value looks like a handler (in the code range)")
lo_code, hi_code = base, base + len(w) - 1
n_h = 0
for a, n, v in best:
    if lo_code <= v <= hi_code and n not in MAC_CMDS and n not in MACM_CMDS:
        n_h += 1
        if n_h <= 40:
            print(f"  {n:<6} -> {v:06o}  ram:{v:04x}")
print(f"  ({n_h} such entries)")
