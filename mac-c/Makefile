# MAC-C - host-side MAC assembler for the NORD TSS sources, plus the TSS
# artifact builds it drives. One Makefile owns the whole pipeline:
#
#   make            -> mac-c (driver) + test_mac (unit tests)   [default]
#   make test       -> build and run the unit test suite
#   make tss        -> the golden ASSYSA/ASSYSB build (../Build/)
#   make drum       -> the NORD-10 + swapping-drum bootable image (../Build/drum/)
#   make minit      -> the standalone MINIT disc formatter BPUN  (../Build/minit/)
#   make artifacts  -> tss + drum + minit
#   make all        -> tools + tests + all artifacts (everything)
#   make clean      -> remove build/ (artifacts are disposable too)
#
# Layout:
#   src/      the assembler itself (split by section; see src/mac_internal.h)
#   tests/    the unit-test binary's source
#   build/    ALL generated output - objects and both executables. Disposable.
#   scripts/  build/ verify/ extract/ attic/ - see README.md
#
# The artifact targets are a DATA pipeline: each runs mac-c on the original
# .SYMB sources.  mac-c is a real prerequisite of every artifact, so editing
# the assembler and re-running `make <artifact>` rebuilds the tool first and
# then the artifact - no stale-assembler surprises.

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Isrc
SRCDIR   = src
TESTDIR  = tests
BUILDDIR = build

# The assembler core, shared by mac-c and test_mac. mac_permsym.c is
# GENERATED from the real MAC.BPUN - do not hand-edit it.
CORE_SRC = $(SRCDIR)/mac_symtab.c $(SRCDIR)/mac_expr.c   $(SRCDIR)/mac_macro.c \
           $(SRCDIR)/mac_cmd.c    $(SRCDIR)/mac_stmt.c   $(SRCDIR)/mac_api.c   \
           $(SRCDIR)/mac_bpun.c   $(SRCDIR)/mac_permsym.c
CORE_OBJ = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(CORE_SRC))

MAC_C   = $(BUILDDIR)/mac-c
TEST_MAC = $(BUILDDIR)/test_mac

# Default goal: build the tool and the tests, nothing heavy.
.DEFAULT_GOAL := tools
tools: $(MAC_C) $(TEST_MAC)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/mac.h $(SRCDIR)/mac_internal.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/main.o: $(SRCDIR)/main.c $(SRCDIR)/mac.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/test_mac.o: $(TESTDIR)/test_mac.c $(SRCDIR)/mac.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(MAC_C): $(CORE_OBJ) $(BUILDDIR)/main.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_MAC): $(CORE_OBJ) $(BUILDDIR)/test_mac.o
	$(CC) $(CFLAGS) -o $@ $^

test: $(TEST_MAC)
	$(TEST_MAC)

# ---- TSS artifact builds (run the ORIGINAL 1973 sources through mac-c) ----
# Each artifact depends on mac-c: the tool is rebuilt first if a source
# changed. The recipe bodies live in scripts/build/ (they carry the exact
# )9ASSM command streams and the range/vector self-checks).

# The golden build: the real ASSYSA/ASSYSB command streams -> ../Build/*.SYMB,
# scored against ../reference/ASYMB.SYMB / BSYMB.SYMB.
tss: $(MAC_C)
	./scripts/build/build_tss_assysa.sh

# NORD-10 + swapping-drum variant -> ../Build/drum/tss-drum.bpun (+ CDC image).
drum: $(MAC_C)
	./scripts/build/build_tss_drum.sh

# Standalone MINIT disc formatter -> ../Build/minit/minit.bpun.  Must exist
# before a bare-disc bring-up (it writes the MIB free-track bitmap that
# SINIT's GTRK needs).  See ../docs/TSS-BRINGUP.md.
minit: $(MAC_C)
	./scripts/build/build_minit.sh

# Everything that mac-c produces from the sources.
artifacts: tss drum minit

# The full monty: tool + tests + every artifact.
all: tools test artifacts

clean:
	rm -rf $(BUILDDIR)
	rm -f mac_test_*.bin mac_test_*.bpun mac_test_*.symb

.PHONY: tools all test tss drum minit artifacts clean
