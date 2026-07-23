#!/usr/bin/env python3
"""Scan ANY ND image (BPUN or raw .PROG) for MAC's packed-name symbol table.

Unlike scan_bpun_commands.py this does not parse a tape wrapper - it slides
over the raw bytes looking for the 3-word [name_hi, name_lo, value] pattern.
Used to read the permanent table out of .PROG memory images.

Usage: scan_raw_table.py <file> [name ...]
"""
import sys


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


def main():
    path = sys.argv[1]
    wanted = [w.upper() for w in sys.argv[2:]]
    d = open(path, 'rb').read()
    w = [(d[i] << 8) | d[i + 1] for i in range(0, len(d) - 1, 2)]

    best, best_off = [], 0
    for off in range(3):
        run, start = [], off
        for i in range(off, len(w) - 2, 3):
            n = name_of(w[i], w[i + 1])
            if ok(n):
                if not run:
                    start = i
                run.append((i, n, w[i + 2]))
            else:
                if len(run) > len(best):
                    best, best_off = run, start
                run = []
        if len(run) > len(best):
            best, best_off = run, start

    print(f"{path}")
    print(f"  {len(w)} words; longest table run {len(best)} entries "
          f"at word offset {best_off}")
    names = {n: v for _, n, v in best}
    if wanted:
        for t in wanted:
            if t in names:
                print(f"    {t:<6} = {names[t]:06o}")
            else:
                print(f"    {t:<6} not found")
    else:
        for _, n, v in best:
            print(f"    {n:<6} {v:06o}")


if __name__ == '__main__':
    main()
