#!/usr/bin/env python3
"""Scan an ND BPUN image for MAC's permanent/command symbol table.

MAC stores symbols as 3-word entries [name_hi, name_lo, value]; the name is
up to 5 characters at 6 bits each, right-justified across the two name
words (verified against MAC.BPUN's table at 0xE9B3). This finds any such
table in an arbitrary BPUN and prints the entries, so we can tell which
assembler build actually implements a given command.

Usage: scan_bpun_commands.py <file.bpun> [name ...]
"""
import sys


def load_bpun(path):
    """Return (base, [words]) from a BPUN tape."""
    d = open(path, 'rb').read()
    i = 0
    while i < len(d) and d[i] == 0:            # null leader
        i += 1
    while i < len(d) and d[i] != ord('!'):     # octal-ASCII bootstrap
        i += 1
    i += 1                                     # skip the '!' start marker
    if i + 4 > len(d):
        return None, []
    base = (d[i] << 8) | d[i + 1]
    count = (d[i + 2] << 8) | d[i + 3]
    i += 4
    words = []
    for k in range(count):
        if i + 1 >= len(d):
            break
        words.append((d[i] << 8) | d[i + 1])
        i += 2
    return base, words


def decode(hi, lo):
    """Unpack 5 characters of 6 bits from the two name words."""
    v = (hi << 16) | lo
    s = ''
    for k in (4, 3, 2, 1, 0):
        c = (v >> (k * 6)) & 0x3F
        if c == 0:
            continue
        s += chr(c + 0x40) if c < 0x20 else chr(c)
    return s


def plausible(name):
    if not (1 <= len(name) <= 5):
        return False
    ok = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$,.-*/()"
    return all(ch in ok for ch in name)


def main():
    path = sys.argv[1]
    wanted = [w.upper() for w in sys.argv[2:]]
    base, words = load_bpun(path)
    if not words:
        print(f"{path}: could not parse a binary block")
        return
    print(f"{path}: base {base:06o}  {len(words)} words "
          f"({base:06o}-{base + len(words) - 1:06o})")

    # Walk every 3-word alignment and keep runs of decodable entries; a real
    # table shows up as a long uninterrupted run.
    best = []
    for off in range(0, 3):
        run, runs = [], []
        for i in range(off, len(words) - 2, 3):
            nm = decode(words[i], words[i + 1])
            if plausible(nm):
                run.append((base + i, nm, words[i + 2]))
            else:
                if len(run) > len(runs):
                    runs = run
                run = []
        if len(run) > len(runs):
            runs = run
        if len(runs) > len(best):
            best = runs
    print(f"  longest plausible table run: {len(best)} entries")
    if best:
        print(f"  at {best[0][0]:06o} .. {best[-1][0]:06o}")

    names = {n: (a, v) for a, n, v in best}
    if wanted:
        print("  --- looked-for commands ---")
        for w in wanted:
            if w in names:
                a, v = names[w]
                print(f"    {w:<6} FOUND at {a:06o}  value {v:06o}")
            else:
                print(f"    {w:<6} not present")
    else:
        for a, n, v in best:
            print(f"    {a:06o}  {n:<6} {v:06o}")


if __name__ == '__main__':
    main()
