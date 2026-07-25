#!/usr/bin/env python3
"""overlay_layout.py - derive the NORD TSS overlay-on-disc layout.

READ-ONLY analysis helper for the TSS-ARCHITECTURE.md (overlay chapter) work. It does NOT
touch mac.c/main.c or any TSS source; it only reads a golden symbol dump
(reference/ASYMB.SYMB or BSYMB.SYMB) and prints, for every overlay symbol
OVn, the disc sector pair it occupies using the run-time/SOVER contract:

        disc_sector(n) = OVDK + 2*n         (first 256-word sector)
        disc_sector(n) + 1                  (second 256-word sector)

OVDK, ROVER and the overlay numbers are all taken from the dump itself, so
the output is a re-derivation from the oracle, not a hand-typed table.

All values in the dump are OCTAL (that is how MAC prints them).

Usage:
    python3 overlay_layout.py ../../reference/ASYMB.SYMB
"""
import re
import sys


def load_syms(path):
    """Parse a MAC )LIST symbol dump: lines of 'NAME=octalvalue'."""
    syms = {}
    with open(path, "r", encoding="latin-1") as f:
        for line in f:
            m = re.match(r"\s*([0-9A-Z#]+)\s*=\s*([0-7]+)\s*$", line.strip())
            if m:
                syms[m.group(1)] = int(m.group(2), 8)  # octal
    return syms


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    s = load_syms(sys.argv[1])

    ovdk = s.get("OVDK")
    rover = s.get("ROVER")
    vors = s.get("VORS", 0o1000)
    if ovdk is None or rover is None:
        print("ERROR: OVDK/ROVER not found in dump")
        return 2

    # collect OVn symbols (skip the QOVn overlay-size symbols)
    ovs = []
    for name, val in s.items():
        if re.fullmatch(r"OV[0-9][0-9A-Z]*", name) and not name.startswith("QOV"):
            ovs.append((val, name))
    ovs.sort()

    print(f"OVDK  = {ovdk:06o}  (octal) - base disc sector of overlay 0")
    print(f"ROVER = {rover:06o}  (octal) - core load address")
    print(f"VORS  = {vors:06o}  (octal) - overlay size / sectors = {vors // 0o400}")
    print()
    print("  OVn   num(oct)   sec0    sec1   core_lo  core_hi")
    for val, name in ovs:
        sec0 = ovdk + 2 * val
        sec1 = sec0 + 1
        print(f"{name:>6}  {val:06o}   {sec0:06o}  {sec1:06o}  "
              f"{rover:06o}  {rover + vors - 1:06o}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
