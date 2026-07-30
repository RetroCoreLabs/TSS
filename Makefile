# NORD TSS 3.0 — top-level driver: build everything and run the bring-up
# step by step, from a bare disc to an interactive login on nd100x.
#
#   Run from WSL (the nd100x emulator and the POSIX tools live there):
#       make help          (run from the repo root)
#
# This Makefile replaces the former bringup/*.sh wrappers; the concrete
# emulator command lines live in the recipes below.  The two real tools
# (bringup/verify-disc.py, bringup/check-robj-encoding.py) remain scripts.
#
# The interactive stages (format / coldstart / login) run the emulator in
# the FOREGROUND on your terminal: you type into TSS's console and you stop
# the emulator with Ctrl-C.  Ctrl-C is the correct, flushing stop — the CDC
# surface is written back to cdc.img on SIGINT.  Killing it any other way
# loses the disc writes.
#
# The dap-* variants start the same stages under the DAP debugger (port
# $(DAP_PORT)) for scripted control; stop those with `make stop` (SIGINT).
#
# Reference: docs/TSS-BRINGUP.md (the comprehensive bring-up document).

# ---------------------------------------------------------------------------
# configuration
# ---------------------------------------------------------------------------

# The built artifacts (produced by `make build` via mac-c/Makefile).
TSS_BPUN   := Build/drum/tss-drum.bpun
TSS_CDC    := Build/drum/tss-cdc.img
MINIT_BPUN := Build/minit/minit.bpun
CDBIN_TAPE := Build/tdump/cdbin-boot.bpun

# Disposable working area for the bring-up disc images (created by `prepare`).
WORK := Build/bringup
BPUN := $(WORK)/tss.bpun
CDC  := $(WORK)/cdc.img
DRUM := $(WORK)/drum.img

# DAP debugger port for TSS.  1777 is this project's reserved port (other
# emulator instances on this machine use the 4711 default) — override with
# `make dap-login DAP_PORT=1780` if needed.
DAP_PORT ?= 1777

# Locate the nd100x emulator binary. nd100x lives OUTSIDE this repo, so it is
# found via $HOME or the ND100X environment variable - never a hard-coded path.
# Override with `make login ND=/path/to/nd100x`.
ND ?= $(firstword $(wildcard \
        $(HOME)/repos/nd100x/build/bin/nd100x \
        $(HOME)/repos/nd100x/build-linux/bin/nd100x \
        $(ND100X)))

.DEFAULT_GOAL := help

# ---------------------------------------------------------------------------
##@ Overview
# ---------------------------------------------------------------------------

help: ## list all targets (this text)
	@echo ""
	@echo "NORD TSS 3.0 — build + bring-up driver.  Run from WSL."
	@echo ""
	@echo "The full bring-up, in order (interactive stages marked *):"
	@echo "    make build          make prepare        make format   *"
	@echo "    make verify         make coldstart *    make verify"
	@echo "    make login     *    make discboot  *"
	@echo ""
	@echo "After any completed coldstart or login, the disc alone is bootable:"
	@echo "    make discboot   boots from the CDC disc with NO paper tape."
	@echo ""
	@awk 'BEGIN {FS = ":.*##"} \
	     /^##@/      { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } \
	     /^[a-zA-Z0-9_-]+:.*##/ { printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2 }' \
	     $(MAKEFILE_LIST)
	@echo ""
	@echo "Interactive stages run the emulator on YOUR terminal: type into the"
	@echo "TSS console, stop with Ctrl-C (that flushes the CDC disc image)."
	@echo "Full documentation: docs/TSS-BRINGUP.md"
	@echo ""

# ---------------------------------------------------------------------------
##@ Build and test (delegates to mac-c/Makefile)
# ---------------------------------------------------------------------------

build: ## build mac-c + run tests + build all TSS artifacts (tss/drum/minit/tdump)
	$(MAKE) -C mac-c all

test: ## run the mac-c unit-test suite (732 assertions, must be 0 failures)
	$(MAKE) -C mac-c test

golden: ## rebuild the golden ASSYSA/ASSYSB streams and score vs the 1978 oracle
	$(MAKE) -C mac-c tss
	cd mac-c && ./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
	cd mac-c && ./scripts/verify/compare_asymb.sh ../Build/BSYMB.SYMB ../reference/BSYMB.SYMB

verify-repo: ## the whole verification gate: layout, build, tests, coverage, oracle, links
	cd mac-c && ./scripts/verify/verify_repo.sh

# ---------------------------------------------------------------------------
##@ Bring-up: bare disc -> interactive login   (docs/TSS-BRINGUP.md)
# ---------------------------------------------------------------------------

# Guard: fail with a clear message if the emulator binary was not found.
need-nd:
	@test -n "$(ND)" -a -x "$(ND)" || { \
	    echo "ERROR: nd100x binary not found. Set ND=/path/to/nd100x" >&2; exit 1; }

# Guard: fail if the build artifacts are missing.
$(TSS_BPUN) $(TSS_CDC) $(MINIT_BPUN):
	@echo "ERROR: missing $@ — run 'make build' first." >&2; exit 1

# The CDBIN bootstrap-install tape, punched by mac's )8DUMP from the
# unmodified TDUMP.SYMB (CDC+N10 marks) exactly as the original TSBIN
# assembly punched its distribution tape. Committed, regenerable any time.
$(CDBIN_TAPE):
	cd mac-c && ./scripts/build/build_tdump.sh

# The CDC image is padded to 4 MB (8192 sectors) because the emulated CDC
# device does NOT grow on writes, and the TSS user area (FSYS = NCR 4470
# octal) maps to a high physical sector; padding appends zeros only, so the
# overlays written by the build are untouched.  The drum is a blank 1 MB.
#
# The disc bootstrap is installed THE ORIGINAL WAY: mac's )8DUMP punches the
# CDBIN install tape during the TDUMP build (exactly what the TSBIN assembly
# punched in 1973, src/TSS3.SYMB:232-296), and booting that tape runs TSS's
# own HLOAD -> TBOOT -> HDKOP, which writes the boot sectors to the disc
# itself.  TBOOT's trailer (8DKA = -1) ends in a WAIT halt; --max-instr
# bounds the run so the recipe always returns (the writes are write-through).
prepare: $(TSS_BPUN) $(TSS_CDC) $(MINIT_BPUN) $(CDBIN_TAPE) need-nd ## STEP 1: fresh padded disc set in Build/bringup/
	mkdir -p $(WORK)
	cp $(TSS_BPUN) $(BPUN)
	cp $(TSS_CDC)  $(CDC)
	truncate -s 4M $(CDC)
	head -c 1048576 /dev/zero > $(DRUM)
	$(ND) --boot=tape --image=$(CDBIN_TAPE) --cdc=$(CDC) --max-instr=2000000
	@echo ""
	@echo "Fresh disc set in $(WORK) (bootstrap installed from the CDBIN tape)."
	@echo "Next: make format"

bootsector: $(CDBIN_TAPE) need-nd ## (re)install the disc bootstrap by BOOTING the CDBIN tape against Build/bringup/cdc.img
	@test -f $(CDC) || { echo "Run 'make prepare' first." >&2; exit 1; }
	$(ND) --boot=tape --image=$(CDBIN_TAPE) --cdc=$(CDC) --max-instr=2000000

format: need-nd ## STEP 2 (interactive): MINIT format — type 4470, 4670, I; Ctrl-C at FINISHED
	@test -f $(CDC) || { echo "Run 'make prepare' first." >&2; exit 1; }
	@echo ">>> MINIT — type: 4470 <Enter>  4670 <Enter>  I <Enter>; Ctrl-C at FINISHED"
	$(ND) --boot=bpun --image=$(MINIT_BPUN) --cdc=$(CDC)

coldstart: need-nd ## STEP 3 (interactive): cold-start, creates user SYSTEM; Ctrl-C at @ENTER
	@test -f $(CDC) || { echo "Run 'make prepare' and 'make format' first." >&2; exit 1; }
	@echo ">>> Cold-start (OPR=131313 -> SINIT creates SYSTEM). Ctrl-C once @ENTER shows."
	$(ND) --boot=bpun --image=$(BPUN) --cdc=$(CDC) --drum=$(DRUM) --opr=131313 --start=7

login: need-nd ## STEP 4 (interactive): normal boot; type SYSTEM, then 1 — you get the @ prompt
	@test -f $(CDC) || { echo "Run steps 1-3 first (prepare/format/coldstart)." >&2; exit 1; }
	@echo ">>> Normal boot. At @ENTER type: SYSTEM <Enter>, at PROJECT NUMBER: 1 <Enter>."
	@echo ">>> First login also asks TYPE IN DATE (DD,MM,YYYY,HH,MM,SS). Then: @ — try HELP."
	$(ND) --boot=bpun --image=$(BPUN) --cdc=$(CDC) --drum=$(DRUM) --start=7

# --boot=cdc reads physical sector 0 into core 0 and starts at 0, exactly what
# the front-panel LOAD button did.  Needs the bootstrap sectors (installed by
# the CDBIN tape in make prepare / make bootsector) AND a system image on the
# disc — SYSSV writes that image on every --start=7 boot, so any completed
# coldstart or login leaves the disc bootable.  The --image argument is a
# dummy: nd100x's argument validation demands one for --boot=cdc but the CDC
# boot path never reads it.
discboot: need-nd ## STEP 5 (interactive): cold boot from the DISC ALONE — no paper tape
	@test -f $(CDC) || { echo "Run steps 1-3 first (prepare/format/coldstart)." >&2; exit 1; }
	@echo ">>> Cold boot from disc (no tape image). At @ENTER log in as usual."
	$(ND) --boot=cdc --image=$(CDC) --cdc=$(CDC) --drum=$(DRUM)

auto: prepare need-nd ## ONE-COMMAND bring-up: prepare + format + cold-start, fully unattended (DAP-driven, disc PERSISTS). Then: make login
	@echo ">>> Automated bring-up: format + cold-start, driven over DAP and stopped"
	@echo ">>> with a CLEAN debugger-terminate so the CDC disc is actually written back."
	python3 bringup/dap_bringup.py $(CURDIR) --nd $(ND)
	@python3 bringup/verify-disc.py

verify: ## check the bring-up disc: MIB free tracks + SYSTEM user present
	python3 bringup/verify-disc.py

check-encoding: ## assert the built/working BPUNs carry the FIXED ROBJ encoding (stale-mac-c trap)
	@python3 bringup/check-robj-encoding.py $(TSS_BPUN) $(wildcard $(BPUN))

clean-bringup: ## delete the disposable bring-up disc set (Build/bringup/)
	rm -rf $(WORK)

# ---------------------------------------------------------------------------
##@ DAP-debugger variants (scripted control; port 1777 unless DAP_PORT=... given)
# ---------------------------------------------------------------------------
# The emulator holds at entry until a DAP client connects and continues.
# Console I/O goes over DAP (terminal 192, raw bytes, CR = 0x0D) instead of
# the local terminal.  Stop with `make stop` (SIGINT = flushing stop).

dap-format: need-nd ## STEP 2 under the DAP debugger
	@test -f $(CDC) || { echo "Run 'make prepare' first." >&2; exit 1; }
	$(ND) --boot=bpun --image=$(MINIT_BPUN) --cdc=$(CDC) --debugger --port=$(DAP_PORT)

dap-coldstart: need-nd ## STEP 3 under the DAP debugger
	@test -f $(CDC) || { echo "Run 'make prepare' and 'make format' first." >&2; exit 1; }
	$(ND) --boot=bpun --image=$(BPUN) --cdc=$(CDC) --drum=$(DRUM) --opr=131313 --start=7 \
	      --debugger --port=$(DAP_PORT)

dap-login: need-nd ## STEP 4 under the DAP debugger
	@test -f $(CDC) || { echo "Run steps 1-3 first (prepare/format/coldstart)." >&2; exit 1; }
	$(ND) --boot=bpun --image=$(BPUN) --cdc=$(CDC) --drum=$(DRUM) --start=7 \
	      --debugger --port=$(DAP_PORT)

dap-discboot: need-nd ## STEP 5 under the DAP debugger
	@test -f $(CDC) || { echo "Run steps 1-3 first (prepare/format/coldstart)." >&2; exit 1; }
	$(ND) --boot=cdc --image=$(CDC) --cdc=$(CDC) --drum=$(DRUM) \
	      --debugger --port=$(DAP_PORT)

# The bringu[p] bracket keeps pgrep from matching its own sh -c command line.
stop: ## SIGINT the bring-up emulator (= Ctrl-C: flushes the CDC image) and wait for exit
	@PID=$$(pgrep -f "nd100x.*Build/bringu[p]" | head -1); \
	if [ -z "$$PID" ]; then echo "no bring-up nd100x running"; exit 0; fi; \
	echo "SIGINT -> nd100x pid $$PID"; kill -INT $$PID; \
	for i in $$(seq 1 15); do \
	    sleep 1; kill -0 $$PID 2>/dev/null || { echo "nd100x exited after $$i s"; exit 0; }; \
	done; \
	echo "STILL RUNNING after 15s (flush may be pending — not killed harder)"; exit 1

status: ## show artifact presence, disc state, and whether an emulator is running
	@echo "nd100x        : $(if $(ND),$(ND),NOT FOUND)"
	@for f in $(TSS_BPUN) $(TSS_CDC) $(MINIT_BPUN) $(BPUN) $(CDC) $(DRUM); do \
	    if [ -f $$f ]; then echo "present       : $$f"; else echo "MISSING       : $$f"; fi; \
	done
	@pgrep -af "nd100x.*Build/bringu[p]" || echo "emulator      : not running"
	@[ -f $(CDC) ] && python3 bringup/verify-disc.py || true

.PHONY: help build test golden verify-repo need-nd prepare bootsector format \
        coldstart login discboot verify check-encoding clean-bringup \
        dap-format dap-coldstart dap-login dap-discboot stop status
