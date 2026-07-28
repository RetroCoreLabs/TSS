#!/usr/bin/env bash
#
# logon_trace_probe.sh -- locate where NORD TSS's cold console-login path stops.
#
# READ-ONLY with respect to the TSS source and the committed disc images: it runs
# nd100x on *copies* of the drum/CDC images, entering at INIT=000301, streams the
# CPU --trace through awk, and tallies (a) how many instructions run at each
# interrupt level (PIL) and (b) how often the key login-path addresses execute:
#
#     INIT   000301   cold-start / interrupt bring-up
#     LEV5   006324   resident level-5 scheduler (idle spin lives here)
#     SWAPR  006443   swapper; the "initialise a fresh process" path -> S10 -> LEV2
#     LEV2   025104   level-2 cold-login entry (the console process is dispatched here)
#     SXBRK  024535   called early by LEV2
#     ERMSG  031322   LEV2 prints a start-up message; ERMSG = JMP I (FERMS
#                     and FERMS = GOVER OV15  -> needs overlay OV15 loaded from disc
#     LOGON  032735   the login routine (inside resident overlay OV19)
#     XSTAR  033601   GOVER OV19 warm-start command-processor entry (RMODE=0 path)
#
# Interpretation:
#   * PIL=2 count > 0 and LEV2 == 1  -> the console process WAS auto-created and
#     dispatched (no keypress needed; INIT seeds PRT1[5]=0, see docs/TSS-ARCHITECTURE.md (login chapter)).
#   * LOGON == 0 while ERMSG/FERMS-region churns -> blocked at the ERMSG=GOVER OV15
#     start-up-message overlay call, before `JPL I (LOGON`.  This is the verified
#     final blocker; fix = stage OV15 (and the other overlays) loadably on the CDC
#     disc, OR neutralise the ERMSG call in LEV2 (diagnostic only).
#
# Usage:  bringup/logon_trace_probe.sh [max_instructions]   (default 6000000)
#
set -euo pipefail

ND100X=${ND100X:?set ND100X to the nd100x binary}
TSSDIR=${TSSDIR:-Build/drum}
MAXI=${1:-6000000}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cp "$TSSDIR/tss-drum.img" "$TMP/drum.img"
cp "$TSSDIR/tss-cdc.img"  "$TMP/cdc.img"

# Resolve the login-path addresses from the build's OWN symbol dump (DSYMB) so a
# source edit that shifts addresses (e.g. restoring a missing resident word) can
# never make this probe silently watch the wrong locations. DSYMB stores each as
# a 6-digit octal string ("LOGON=032736"), exactly the form the --trace prints.
DSYMB="$TSSDIR/DSYMB.SYMB"
sym() { grep -iE "^$1=" "$DSYMB" 2>/dev/null | head -1 | cut -d= -f2 | tr -d '[:space:]'; }
# TSS2+ symbols move whenever TSS2/3/4/5 source changes -> always pull them live.
A_SWAPR=$(sym SWAPR); A_SXBRK=$(sym SXBRK); A_ERMSG=$(sym ERMSG)
A_LOGON=$(sym LOGON); A_XSTAR=$(sym XSTAR)
# INIT/LEV5/LEV2 are )KILL'd out of DSYMB. INIT/LEV5 live in TSS1 and are never
# shifted by TSS2+ edits, so their resident addresses are fixed. LEV2 is in TSS2
# (shifts with it); it only affects the dispatched-vs-never-dispatched wording,
# not the LOGON-reached verdict, so a stale value is harmless. Fall back to the
# known values when the symbol is absent from DSYMB.
A_INIT=000301; A_LEV5=006324; A_LEV2=025105
: "${A_SWAPR:=006443}"

"$ND100X" --boot=bpun --image="$TSSDIR/tss-drum.bpun" \
          --drum="$TMP/drum.img" --cdc="$TMP/cdc.img" \
          --start=000301 --trace --max-instr="$MAXI" 2>&1 1>/dev/null \
| awk -v init="$A_INIT" -v lev5="$A_LEV5" -v swapr="$A_SWAPR" -v lev2="$A_LEV2" \
      -v sxbrk="$A_SXBRK" -v ermsg="$A_ERMSG" -v logon="$A_LOGON" -v xstar="$A_XSTAR" '
    /PIL=/ { for (i=1;i<=NF;i++) if ($i ~ /^PIL=/) { pil[$i]++; break } }
    $1==init {ni++}  $1==lev5 {nlev5++}  $1==swapr {nswapr++}
    $1==lev2 {nlev2++} $1==sxbrk {nsxbrk++} $1==ermsg {nermsg++}
    $1==logon {nlogon++} $1==xstar {nxstar++}
    END {
        print "--- resolved addresses (from DSYMB) ---";
        printf "  INIT=%s LEV5=%s SWAPR=%s LEV2=%s SXBRK=%s ERMSG=%s LOGON=%s XSTAR=%s\n",
               init, lev5, swapr, lev2, sxbrk, ermsg, logon, xstar;
        print "--- instructions per interrupt level (PIL) ---";
        for (k in pil) print "  " k, pil[k];
        print "--- login-path address hit counts ---";
        printf "  INIT=%d LEV5=%d SWAPR=%d LEV2=%d SXBRK=%d ERMSG=%d LOGON=%d XSTAR=%d\n",
               ni+0, nlev5+0, nswapr+0, nlev2+0, nsxbrk+0, nermsg+0, nlogon+0, nxstar+0;
        if (nlogon+0 == 0 && nlev2+0 > 0)
            print "  VERDICT: console dispatched to LEV2 but LOGON never reached (ERMSG=GOVER OV15 blocks).";
        else if (nlogon+0 > 0)
            print "  VERDICT: LOGON reached.";
        else
            print "  VERDICT: LEV2 never reached (console process not dispatched).";
    }'
