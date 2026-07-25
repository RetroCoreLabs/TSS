#!/usr/bin/env python3
"""cold_boot_init.py  -- READ-ONLY cold-boot init reporter for NORD TSS.

Reads a MAC ")LIST" symbol dump (default: ../../Build/drum/DSYMB.SYMB, the DRUM/N10
build) and prints the addresses of the cells involved in TSS's cold-boot /
interrupt / overlay-dispatch bring-up, together with the recommended emulator
init derived in docs/TSS-ARCHITECTURE.md (cold-start chapter).

It NEVER writes to mac.c/main.c or to any TSS source; it only parses a symbol
dump and prints. Purely diagnostic.

Background (all VERIFIED from src/TSS*.SYMB -- see docs/TSS-ARCHITECTURE.md (cold-start chapter)):
  * GOVX (the overlay dispatcher) skips the disc read when the requested overlay
    number equals the contents of the OVLAY cell (GOVX: LDA 0,X; SUB I 9OVL;
    JAZ *+7, with 9OVL -> OVLAY, TSS2:182,187).
  * The OVERX macro presets OVLAY/OVLAX/SYSOV to RQR-1 at assembly time
    (TSS3:88-90); the last overlay is OV19=36, so on the cold image all three
    hold 36 -- the first GOVER OV19 then falsely looks "already loaded".
  * The real interrupt bring-up is INIT (address 0301): it arms the level
    vectors (incl. level 5 = the overlay reader LEV5/S5), sets PIE=ENABL(77157),
    programs the RTC, and does ION (TSS1:237-290). ISTRT/START/XSTAR do NONE of
    this -- that is why entering at ISTRT leaves interrupts off.
  * Cold entry is INIT (0301), reached by the disc bootstrap DBOOT
    (TSS3:265-268, "JMP I (301") or the restart vector at location 20
    (TSS1:197). ISTRT is only the per-user restart address.

Usage:
    python3 cold_boot_init.py [path-to-DSYMB.SYMB]
"""

import os
import re
import sys

# Symbols we care about for cold boot, with a one-line role note.
WANTED = [
    ("INIT",  "cold entry: arms vectors, PIE, RTC, ION (TSS1:237)"),
    ("DBOOT", "disc bootstrap: coreload -> core, then JMP INIT (TSS3:265)"),
    ("LEV0",  "idle/background level INIT hands to (TSS1:2566)"),
    ("LEV5",  "level-5 scheduler/swapper; reaches S5 overlay reader (TSS1:2705)"),
    ("GOVX",  "overlay dispatcher; 'already loaded?' test (TSS2:181)"),
    ("OVLAY", "user overlay cell GOVX compares against (TSS2:78)"),
    ("OVLAX", "previous-overlay cell (TSS2:80)"),
    ("SYSOV", "system overlay cell S5 compares against (TSS1:4539)"),
    ("ROVER", "overlay core base (sector 0 load target)"),
    ("ROV4",  "overlay core base + 400 (sector 1 load target)"),
    ("ISTRT", "per-user restart addr (NOT cold entry) (TSS2:1846)"),
    ("START", "user start after ISTRT (TSS2:1847)"),
    ("XSTAR", "GOVER OV19 -> command processor (TSS5:1808)"),
    ("OVDK",  "overlay disc base sector (=160, non-DEBUG)"),
    ("OV19",  "command-processor overlay number (=36)"),
    ("CORLD", "coreload disc sector DBOOT reads from (=60)"),
    ("MSTRT", "top of resident coreload (=40000)"),
    ("ENABL", "cell holding the PIE mask 77157"),
]

# A dump line looks like e.g. " OVLAY=015205" or "OVLAY=015205" (octal value).
LINE_RE = re.compile(r"^\s*([A-Z0-9#]+)\s*=\s*([0-7]+)\s*$")


def load_symbols(path):
    """Parse a )LIST symbol dump into {NAME: octal_string}. Read-only."""
    syms = {}
    with open(path, "r", encoding="latin-1") as fh:
        for line in fh:
            m = LINE_RE.match(line.rstrip("\n"))
            if m:
                # First definition wins (MAC keeps the FIRST value on redefine).
                syms.setdefault(m.group(1), m.group(2))
    return syms


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default = os.path.join(here, "..", "..", "Build", "drum", "DSYMB.SYMB")
    path = sys.argv[1] if len(sys.argv) > 1 else default
    if not os.path.isfile(path):
        sys.exit("symbol dump not found: %s" % path)

    syms = load_symbols(path)
    print("cold-boot init cells from: %s\n" % os.path.normpath(path))
    print("  %-7s %-8s  %s" % ("SYMBOL", "ADDR(oct)", "ROLE"))
    print("  %-7s %-8s  %s" % ("-" * 7, "-" * 8, "-" * 40))
    for name, role in WANTED:
        val = syms.get(name, "??????")
        print("  %-7s %-8s  %s" % (name, val, role))

    ov = syms.get("OVLAY", "??????")
    sv = syms.get("SYSOV", "??????")
    init = syms.get("INIT", "??????")
    print("\nRecommended Rank-1 emulator init (see docs/TSS-ARCHITECTURE.md (cold-start chapter)):")
    print("  start PC          = %s   (INIT, NOT ISTRT)" % init)
    print("  patch OVLAY @%s <- 177777   (force GOVX to trigger a load)" % ov)
    print("  patch SYSOV @%s <- 177777   (force S5 to actually read)" % sv)
    print("  mount CDC overlay disc at IOX 500 (OV19 at sectors 254/255)")
    print("  RTC running (INIT programs it via IOX 13, then ION)")
    print("\nNOTE: addresses are build-specific; always read them from the")
    print("      matching build's own symbol dump, as done here.")


if __name__ == "__main__":
    main()
