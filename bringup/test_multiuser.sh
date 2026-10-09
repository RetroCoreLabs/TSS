#!/bin/sh
# Multi-user proof for NORD TSS 3.0 on nd100x - POSIX sh + awk only.
#
# Boots a SCRATCH COPY of a bootable TSS disc (default: dist/cdc-jumpstart.img.gz)
# with --mms=1 and nd100x's telnet server, logs in as SYSTEM on the console
# (TSS TTY1, a pty from util-linux `script`) and on nd100x TERMINAL 8 (TSS
# TTY8, over telnet with `nc`), runs WHO-IS-ON on both terminals and checks
# that both list TTY 1 and TTY 8, then LIST-USERS and LOGOUT on the telnet
# side.  The emulator ends on its own at the --max-instr bound; nothing is
# killed.  Exit status 0 = proof holds, 1 = it does not.
#
#     ./bringup/test_multiuser.sh [--image IMG[.gz]] [--port N] [--terminal N]
#                                 [--nd PATH] [--work DIR] [--max-instr N]
#                                 [--exit-wait S] [--echo]
#
# The transcripts land in Build/bringup/multiuser/ (console.log, telnet.log).
# All paths are relative to the repository root.  The console primitives
# (wait_for, tss_login, tss_command, who_lines, run_console, ...) come from
# bringup/tss_console.sh, sourced in library mode.

here=$(dirname "$0")
TSS_CONSOLE_LIB=1
. "$here/tss_console.sh"
cd "$here/.." || exit 1

IMAGE=dist/cdc-jumpstart.img.gz
PORT=${TELNET_PORT:-9077}
TERMINAL=8
ND_ARG=
WORK=Build/bringup/multiuser
MAX_INSTR=600000000
EXIT_WAIT=400
TSS_ECHO=
while [ $# -gt 0 ]; do
    case $1 in
        --image)      IMAGE=$2; shift ;;
        --port)       PORT=$2; shift ;;
        --terminal)   TERMINAL=$2; shift ;;
        --nd)         ND_ARG=$2; shift ;;
        --work)       WORK=$2; shift ;;
        --max-instr)  MAX_INSTR=$2; shift ;;
        --exit-wait)  EXIT_WAIT=$2; shift ;;
        --echo)       TSS_ECHO=1 ;;
        *) echo "usage: $0 [--image IMG[.gz]] [--port N] [--terminal N] [--nd PATH]" \
                "[--work DIR] [--max-instr N] [--exit-wait S] [--echo]" >&2; exit 2 ;;
    esac
    shift
done
ND=$(find_nd100x "$ND_ARG") || exit 1

# ---------------------------------------------------------------------------
# scratch disc set - the image is copied, never written
# ---------------------------------------------------------------------------
mkdir -p "$WORK"
CDC=$WORK/cdc.img
DRUM=$WORK/drum.img
CLOG=$WORK/console.log
TLOG=$WORK/telnet.log
case $IMAGE in
    *.gz) gzip -dc "$IMAGE" > "$CDC" || exit 1 ;;
    *)    cp "$IMAGE" "$CDC" || exit 1 ;;
esac
head -c 1048576 /dev/zero > "$DRUM"
: > "$TLOG"

exec 3>&1
say "nd100x  : $ND"
say "image   : $IMAGE -> scratch $CDC ($(wc -c < "$CDC") bytes)"
say "telnet  : port $PORT, TERMINAL $TERMINAL"

# ---------------------------------------------------------------------------
# console side (TTY1): log in, then WHO-IS-ON once the telnet user is on
# ---------------------------------------------------------------------------
input_console() {
    if tss_login "$CLOG"; then
        say "console : logged in as $TSS_USER (at the @ prompt)"
        # Wait for the telnet terminal to have run its LIST-USERS (so TTY8 is
        # logged in and has done its WHO-IS-ON), then look from this side.
        wait_for "$TLOG" '^@LIST-USERS' 1 150
        tss_command "$CLOG" WHO-IS-ON
        say "console : WHO-IS-ON -> $(who_lines "$CLOG" | tr '\n' ';')"
    else
        say "console : login FAILED"
    fi
    wait_exit "$CLOG"
}

# ---------------------------------------------------------------------------
# telnet side (TERMINAL n): menu -> ESC -> login -> WHO-IS-ON -> LIST-USERS -> LOGOUT
# ---------------------------------------------------------------------------
input_telnet() {
    if ! wait_for "$TLOG" 'Select terminal' 1 20; then
        say "telnet  : no terminal menu received"
        return 1
    fi
    # The menu names a terminal by its nd100x logical device number
    # ("Terminal 39"); nd100x's own start-up log line
    # "Terminal 39 created (TERMINAL 8/ TET9, ident 47, address 370)"
    # gives the mapping to the TERMINAL number (= TSS TTY number).
    logical=$(awk -v t="$TERMINAL" \
        '$0 ~ ("created \\(TERMINAL " t "[/,]") { print $4; exit }' "$CLOG")
    choice=$(awk -v l="$logical" '
        { gsub(/\r/, "") }
        $1 ~ /^[0-9]+\)$/ && $2 == "Terminal" && $3 == l { sub(/\)/, "", $1); print $1; exit }' "$TLOG")
    say "telnet  : menu = $(awk '
        { gsub(/\r/, "") }
        $1 ~ /^[0-9]+\)$/ && $2 == "Terminal" { printf "%s%s %s %s", (n++ ? ", " : ""), $1, $2, $3 }
        END { print "" }' "$TLOG")"
    say "telnet  : TERMINAL $TERMINAL is nd100x logical device ${logical:-?}"
    if [ -z "$choice" ]; then
        say "telnet  : TERMINAL $TERMINAL (Terminal ${logical:-?}) is not in the menu"
        return 1
    fi
    # A bare digit selects; a CR would select "first available" instead.
    send "$choice"
    if ! wait_for "$TLOG" 'Connected to ' 1 20; then
        say "telnet  : NOT connected"
        return 1
    fi
    say "telnet  : $(grep -a 'Connected to ' "$TLOG" | head -1 | tr -d '\r')"
    # Wake the dead teletype: the level-6 scanner (src/TSS1.SYMB LEV6, N10
    # branch) polls every dead teletype's status register each 80 ms, reads
    # the character, and wakes the process only for ESC (033 octal):
    # `AND (177; SUB (33; JAF L2`.  Any other character is read and dropped.
    printf '\033'
    if wait_for "$TLOG" '@ENTER' 1 30; then
        say "telnet  : ESC sent, @ENTER seen"
    else
        say "telnet  : ESC sent, @ENTER NOT seen"
        return 1
    fi
    if tss_login "$TLOG" 5; then
        say "telnet  : login at the @ prompt"
    else
        say "telnet  : login FAILED"
        return 1
    fi
    # The proof: WHO-IS-ON on the telnet terminal, then LIST-USERS.
    tss_command "$TLOG" WHO-IS-ON
    say "telnet  : WHO-IS-ON -> $(who_lines "$TLOG" | tr '\n' ';')"
    tss_command "$TLOG" LIST-USERS
    # Stay logged in until the console has run its own WHO-IS-ON (its third
    # "@" line: @ENTER, the prompt, the prompt after WHO-IS-ON).
    wait_for "$CLOG" '^@' 3 60
    send_line LOGOUT
    wait_for "$TLOG" 'TIME USED' 1 30
    # Keep the connection until the emulator has ended at its bound.
    wait_for "$CLOG" "$TSS_EXIT_MARK" 1 "$EXIT_WAIT"
}

# ---------------------------------------------------------------------------
# run: console in the background, telnet once the console is logged in
# ---------------------------------------------------------------------------
TSS_INPUT=input_console
run_console "$CLOG" "$ND" --boot=cdc "--image=$CDC" "--cdc=$CDC" "--drum=$DRUM" \
            "$TSS_MMS1" "--telnet=$PORT" "--max-instr=$MAX_INSTR" &

# The console login is the precondition for the telnet step (two "@" lines:
# @ENTER and the prompt after it).  The telnet server listens from start-up.
if wait_for "$CLOG" '^@' 2 240; then
    input_telnet | nc -q 1 127.0.0.1 "$PORT" > "$TLOG"
fi
wait

# ---------------------------------------------------------------------------
# the checks: both WHO-IS-ON listings show TTY 1 and TTY n, all SYSTEM
# ---------------------------------------------------------------------------
# who_ok FILE - 0 when WHO-IS-ON in FILE lists TTY 1 and TTY $TERMINAL, every
# user is $TSS_USER and there are at least two lines.
who_ok() {
    who_lines "$1" | awk -v t="$TERMINAL" -v u="$TSS_USER" '
        $2 != u { bad = 1 }
        $1 == 1 { one = 1 }
        $1 == t { term = 1 }
        END { exit !(one && term && !bad && NR >= 2) }'
}

ok=1
if who_ok "$TLOG"; then
    :
else
    say "telnet  : expected TTY 1 and TTY $TERMINAL both logged in as $TSS_USER"
    ok=
fi
if who_ok "$CLOG"; then
    :
else
    say "console : expected TTY 1 and TTY $TERMINAL both logged in as $TSS_USER"
    ok=
fi

say ''
for k in 'telnet WHO-IS-ON' 'telnet LIST-USERS' 'console WHO-IS-ON' 'telnet LOGOUT'; do
    say "----- $k -----"
    case $k in
        telnet*)  command_output "$TLOG" "${k#telnet }" >&3 ;;
        console*) command_output "$CLOG" "${k#console }" >&3 ;;
    esac
done
say ''
if [ -n "$ok" ]; then
    say "RESULT  : OK - two users, console TTY1 + TERMINAL $TERMINAL"
    exit 0
fi
say 'RESULT  : FAILED'
exit 1
