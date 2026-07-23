# MAC-C - host-side MAC assembler for the NORD TSS sources, plus the TSS
# artifact builds it drives. One Makefile owns the whole pipeline:
#
#   make            -> mac-as (driver) + test_mac (unit tests)   [default]
#   make test       -> build and run the unit test suite
#   make tss        -> the golden ASSYSA/ASSYSB build (Build/)
#   make drum       -> the NORD-10 + swapping-drum bootable image (Build/drum/)
#   make minit      -> the standalone MINIT disc formatter BPUN  (Build/minit/)
#   make artifacts  -> tss + drum + minit
#   make all        -> tools + tests + all artifacts (everything)
#   make clean      -> remove tool build products (artifacts are disposable too)
#
# The artifact targets are a DATA pipeline: each runs mac-as on the original
# .SYMB sources.  mac-as is a real prerequisite of every artifact, so editing
# the assembler (mac.c) and re-running `make <artifact>` rebuilds the tool
# first and then the artifact - no stale-assembler surprises.

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2
OBJS    = mac.o mac_permsym.o

# Default goal: build the tool and the tests, nothing heavy.
.DEFAULT_GOAL := tools
tools: mac-as test_mac

mac.o: mac.c mac.h
	$(CC) $(CFLAGS) -c mac.c -o mac.o

mac_permsym.o: mac_permsym.c mac.h
	$(CC) $(CFLAGS) -c mac_permsym.c -o mac_permsym.o

mac-as: $(OBJS) main.o
	$(CC) $(CFLAGS) -o mac-as $(OBJS) main.o

main.o: main.c mac.h
	$(CC) $(CFLAGS) -c main.c -o main.o

test_mac: $(OBJS) test_mac.o
	$(CC) $(CFLAGS) -o test_mac $(OBJS) test_mac.o

test_mac.o: test_mac.c mac.h
	$(CC) $(CFLAGS) -c test_mac.c -o test_mac.o

test: test_mac
	./test_mac

# ---- TSS artifact builds (run the ORIGINAL 1973 sources through mac-as) ----
# Each artifact depends on mac-as: the tool is rebuilt first if mac.c changed.
# The recipe bodies live in the shell scripts (they carry the exact )9ASSM
# command streams and the range/vector self-checks).

# The golden build: the real ASSYSA/ASSYSB command streams -> Build/*.SYMB,
# scored against reference/ASYMB.SYMB / BSYMB.SYMB.
tss: mac-as
	./build_tss_assysa.sh

# NORD-10 + swapping-drum variant -> Build/drum/tss-drum.bpun (+ CDC image).
drum: mac-as
	./build_tss_drum.sh

# Standalone MINIT disc formatter -> Build/minit/minit.bpun.  Must exist
# before a bare-disc bring-up (it writes the MIB free-track bitmap that
# SINIT's GTRK needs).  See docs/DISK-INIT-USERS-AND-BOOT.md.
minit: mac-as
	./build_minit.sh

# Everything that mac-as produces from the sources.
artifacts: tss drum minit

# The full monty: tool + tests + every artifact.
all: tools test artifacts

clean:
	rm -f *.o mac-as test_mac mac_test_*.bin mac_test_*.bpun mac_test_*.symb

.PHONY: tools all test tss drum minit artifacts clean
