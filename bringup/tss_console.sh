#!/bin/sh
# Drive nd100x + NORD TSS 3.0 through a pseudo-terminal - POSIX sh + awk only.
#
# The interactive bring-up stages (MINIT format, cold start, login) talk to the
# TSS console, which nd100x maps to the terminal nd100x itself runs on.  This
# script starts nd100x under util-linux `script` (which gives it a pty), feeds
# the answers through that pty as each prompt shows up in the transcript, and
# stops the emulator the documented way where the procedure says so: Ctrl-C
# (the byte 0x03 written to the pty; nd100x leaves ISIG on, so the line
# discipline turns it into SIGINT, which flushes the CDC image).  Every stage
# also carries a --max-instr bound, so the emulator always ends by itself.
#
# Usage:   ./bringup/tss_console.sh format     MINIT: 4470, 4670, I; Ctrl-C at FINISHED
#          ./bringup/tss_console.sh coldstart  OPR=131313 cold start; Ctrl-C at @ENTER
#          ./bringup/tss_console.sh login      disc boot, log in as SYSTEM, run a few commands
# Options: --nd PATH        nd100x binary (default: $ND100X, then ~/repos/nd100x/build/bin/nd100x)
#          --work DIR       disc set directory (default: Build/bringup)
#          --log FILE       raw transcript (default: <work>/<stage>.log)
#          --max-instr N    instruction bound that ends the run
#                           (login: 400000000; format/coldstart: 2000000000, a safety net
#                           behind the Ctrl-C)
#          --exit-wait S    seconds to wait for the emulator to end (default 120)
#          --echo           mirror the console to stdout live
#
# Exit status 0 = the stage completed, 1 = it did not.  Prints RESULT : OK/FAILED.
#
# Library use: test_multiuser.sh sources this file with TSS_CONSOLE_LIB=1 set,
# which skips main and only defines the functions.
#
# Every path inside this file is relative to the repository root.

# ---------------------------------------------------------------------------
# constants - the same answers docs/TSS-BRINGUP.md documents
# ---------------------------------------------------------------------------
TSS_USER=SYSTEM
TSS_PROJECT=1
TSS_DATE=$(date +%d,%m,%Y,%H,%M,%S)
TSS_MMS1=--mms=1     # TSS programs the MMU the Paging-System-I way (bringup/tss.cfg)

# nd100x's own messages, used as markers in the transcript.
TSS_EXIT_MARK='Number of instructions run'
TSS_SIGINT_MARK='Caught signal'

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

# Status lines go to fd 3, which main() points at the original stdout; inside
# an input producer (whose stdout is the pty) that is the only way out.
say() {
    printf '%s\n' "$*" >&3
}

seen() {
    printf '%-40s %s\n' "$1" "$2" >&3
}

# find_nd100x [PATH] - locate the emulator binary without a hard-coded path.
find_nd100x() {
    for c in "$1" "$ND100X" "$HOME/repos/nd100x/build/bin/nd100x" \
             "$HOME/repos/nd100x/build-linux/bin/nd100x"; do
        if [ -n "$c" ] && [ -f "$c" ] && [ -x "$c" ]; then
            printf '%s\n' "$c"
            return 0
        fi
    done
    echo 'nd100x not found: give --nd PATH or set ND100X' >&2
    return 1
}

need() {
    if [ ! -f "$1" ]; then
        echo "missing $1 - run \`make prepare\` (and the earlier stages) first" >&2
        exit 1
    fi
}

# count FILE PATTERN - number of transcript lines matching the grep BRE.
count() {
    n=$(grep -a -c -e "$2" -- "$1" 2>/dev/null)
    printf '%s\n' "${n:-0}"
}

# wait_for FILE PATTERN COUNT TIMEOUT - poll once a second until at least COUNT
# lines of FILE match PATTERN; 0 when they do, 1 after TIMEOUT seconds.
wait_for() {
    t=0
    while :; do
        if [ "$(count "$1" "$2")" -ge "$3" ]; then
            return 0
        fi
        if [ "$t" -ge "$4" ]; then
            return 1
        fi
        sleep 1
        t=$((t + 1))
    done
}

# wait_for2 FILE PAT1 CNT1 PAT2 CNT2 TIMEOUT - like wait_for, for whichever of
# two conditions comes first.  Prints 1 or 2; returns 1 on timeout.
wait_for2() {
    t=0
    while :; do
        if [ "$(count "$1" "$2")" -ge "$3" ]; then
            echo 1
            return 0
        fi
        if [ "$(count "$1" "$4")" -ge "$5" ]; then
            echo 2
            return 0
        fi
        if [ "$t" -ge "$6" ]; then
            return 1
        fi
        sleep 1
        t=$((t + 1))
    done
}

# Input producers write to stdout, which is the pipe into the pty.
send() {
    printf '%s' "$1"
}

send_line() {
    printf '%s\r' "$1"
}

# tss_command FILE CMD [TIMEOUT] - type CMD at the @ prompt of the terminal
# whose transcript is FILE, wait for the next @ prompt.  The prompt is counted
# as a line starting with "@" (the echoed command completes that line and the
# new prompt opens the next one), so the count rises by one per command.
tss_command() {
    c=$(count "$1" '^@')
    send_line "$2"
    wait_for "$1" '^@' $((c + 1)) "${3:-30}"
}

# tss_login FILE [TIMEOUT] - @ENTER -> SYSTEM -> PROJECT NUMBER -> 1 -> (date)
# -> @.  Returns 0 at the @ prompt.
tss_login() {
    wait_for "$1" '@ENTER' 1 "${2:-180}" || return 1
    send_line "$TSS_USER"
    wait_for "$1" 'PROJECT NUMBER P-' 1 30 || return 1
    c=$(count "$1" '^@')
    send_line "$TSS_PROJECT"
    which=$(wait_for2 "$1" 'TYPE IN DATE' 1 '^@' $((c + 1)) 60) || return 1
    if [ "$which" = 1 ]; then
        send_line "$TSS_DATE"
        wait_for "$1" '^@' $((c + 1)) 60 || return 1
    fi
    return 0
}

# command_output FILE CMD - the transcript lines between "@CMD" and the next
# line starting with "@" (CRs removed).
command_output() {
    awk -v cmd="$2" '
        { gsub(/\r/, "") }
        /^@/ { inblk = ($0 == "@" cmd); next }
        inblk { print }' "$1"
}

# who_lines FILE - the "<tty> <user>" pairs WHO-IS-ON printed in FILE
# (one line per logged-in teletype: " <tty>   <user>").
who_lines() {
    command_output "$1" WHO-IS-ON |
        awk 'NF == 2 && $1 ~ /^[0-9]+$/ && $2 ~ /^[A-Z][A-Z0-9-]*$/ { print $1, $2 }'
}

# run_console LOG CMD... - run the emulator on a pty with CMD's stdin fed from
# the function named in $TSS_INPUT; the transcript is written to LOG live.
# Returns when nd100x has exited.
run_console() {
    log=$1
    shift
    cmd=
    for a in "$@"; do
        cmd="$cmd '$a'"
    done
    : > "$log"
    if [ -n "$TSS_ECHO" ]; then
        "$TSS_INPUT" | script -q -f -c "$cmd" "$log"
    else
        "$TSS_INPUT" | script -q -f -c "$cmd" "$log" > /dev/null
    fi
}

# wait_exit LOG - the emulator's closing message, at most $EXIT_WAIT seconds.
wait_exit() {
    if wait_for "$1" "$TSS_EXIT_MARK" 1 "$EXIT_WAIT"; then
        say "nd100x : exited at the instruction bound"
    else
        say "nd100x : still running after ${EXIT_WAIT}s (instruction bound not reached)"
    fi
}

# ctrl_c LOG - the documented stop: Ctrl-C on the console (SIGINT via the pty).
ctrl_c() {
    printf '\003'
    if wait_for "$1" "$TSS_SIGINT_MARK" 1 20; then
        say "Ctrl-C sent to nd100x: exited"
    else
        say "Ctrl-C sent to nd100x: NOT acknowledged in 20s (the --max-instr bound will end it)"
    fi
}

# ---------------------------------------------------------------------------
# the stages: an input producer each (runs in the pipeline, stdout = the pty)
# and the result check on the transcript afterwards
# ---------------------------------------------------------------------------

# MINIT: FIRST 4470, LAST 4670, I(nitialize); Ctrl-C at FINISHED.
input_format() {
    ok=1
    for pa in 'FIRST DISK ADDRESS=4470' 'LAST DISK ADDRESS=4670' 'INITIALIZE=I'; do
        pat=${pa%%=*}
        if wait_for "$LOG" "$pat" 1 60; then
            seen "$pat" seen
            send_line "${pa#*=}"
        else
            seen "$pat" 'NOT SEEN'
            ok=
            break
        fi
    done
    if [ -n "$ok" ]; then
        if wait_for "$LOG" 'FINISHED' 1 120; then
            seen FINISHED seen
        else
            seen FINISHED 'NOT SEEN'
        fi
        sleep 1
    fi
    # The Ctrl-C goes out in every case, as the procedure's stop (also the
    # stop that flushes the disc when a prompt was missed).
    ctrl_c "$LOG"
}

stage_format() {
    need "$CDC"; need "$MINIT"
    TSS_INPUT=input_format
    run_console "$LOG" "$ND" --boot=bpun "--image=$MINIT" "--cdc=$CDC" "--max-instr=$MAX_INSTR"
    [ "$(count "$LOG" FINISHED)" -ge 1 ] && [ "$(count "$LOG" "$TSS_SIGINT_MARK")" -ge 1 ]
}

# Cold start with OPR=131313 (SINIT creates user SYSTEM); Ctrl-C at @ENTER.
input_coldstart() {
    if wait_for "$LOG" '@ENTER' 1 180; then
        seen '@ENTER' seen
    else
        seen '@ENTER' 'NOT SEEN'
    fi
    sleep 2
    ctrl_c "$LOG"
}

stage_coldstart() {
    need "$CDC"; need "$DRUM"; need "$BPUN"
    TSS_INPUT=input_coldstart
    run_console "$LOG" "$ND" --boot=bpun "--image=$BPUN" "--cdc=$CDC" "--drum=$DRUM" \
                --opr=131313 --start=7 "$TSS_MMS1" "--max-instr=$MAX_INSTR"
    [ "$(count "$LOG" '@ENTER')" -ge 1 ] && [ "$(count "$LOG" "$TSS_SIGINT_MARK")" -ge 1 ]
}

# Boot from the disc alone, log in on the console, run a few commands, then
# let the instruction bound end the run (no Ctrl-C needed).
input_login() {
    if tss_login "$LOG"; then
        seen 'console login' 'at @ prompt'
        for c in WHO-IS-ON LIST-USERS DATE DISK-SPACE; do
            tss_command "$LOG" "$c" || seen "$c" 'no @ prompt back in 30s'
        done
    else
        seen 'console login' FAILED
    fi
    wait_exit "$LOG"
}

stage_login() {
    need "$CDC"; need "$DRUM"
    TSS_INPUT=input_login
    run_console "$LOG" "$ND" --boot=cdc "--image=$CDC" "--cdc=$CDC" "--drum=$DRUM" \
                "$TSS_MMS1" "--max-instr=$MAX_INSTR"
    for c in WHO-IS-ON LIST-USERS DATE DISK-SPACE; do
        say "----- $c -----"
        command_output "$LOG" "$c" >&3
    done
    # Logged in = the @ prompt came back after @ENTER: at least two "@" lines.
    [ "$(count "$LOG" '@ENTER')" -ge 1 ] && [ "$(count "$LOG" '^@')" -ge 2 ]
}

# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
main() {
    cd "$(dirname "$0")/.." || exit 1
    STAGE=
    ND_ARG=
    WORK=Build/bringup
    LOG=
    MAX_INSTR=
    EXIT_WAIT=120
    TSS_ECHO=
    while [ $# -gt 0 ]; do
        case $1 in
            format|coldstart|login) STAGE=$1 ;;
            --nd)         ND_ARG=$2; shift ;;
            --work)       WORK=$2; shift ;;
            --log)        LOG=$2; shift ;;
            --max-instr)  MAX_INSTR=$2; shift ;;
            --exit-wait)  EXIT_WAIT=$2; shift ;;
            --echo)       TSS_ECHO=1 ;;
            *) echo "usage: $0 format|coldstart|login [--nd PATH] [--work DIR] [--log FILE]" \
                    "[--max-instr N] [--exit-wait S] [--echo]" >&2; exit 2 ;;
        esac
        shift
    done
    if [ -z "$STAGE" ]; then
        echo "usage: $0 format|coldstart|login [options]" >&2
        exit 2
    fi
    ND=$(find_nd100x "$ND_ARG") || exit 1
    CDC=$WORK/cdc.img
    DRUM=$WORK/drum.img
    BPUN=$WORK/tss.bpun
    MINIT=Build/minit/minit.bpun
    [ -n "$LOG" ] || LOG=$WORK/$STAGE.log
    if [ -z "$MAX_INSTR" ]; then
        if [ "$STAGE" = login ]; then MAX_INSTR=400000000; else MAX_INSTR=2000000000; fi
    fi
    exec 3>&1
    say "nd100x : $ND"
    say "log    : $LOG"
    if "stage_$STAGE"; then
        say 'RESULT : OK'
        exit 0
    fi
    say 'RESULT : FAILED'
    exit 1
}

[ -n "$TSS_CONSOLE_LIB" ] || main "$@"
