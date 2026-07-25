#!/usr/bin/env python3
"""Drive nd100x's console over a PTY for unattended bring-up.

Runs the emulator with its console on a pseudo-terminal, watches the output for
expected prompt strings, sends scripted answers, and stops the emulator with
SIGINT (the flushing stop) once a "done" marker appears.

Config is JSON on argv[1]:
  {
    "cmd":     ["nd100x", "--boot=bpun", ...],   # emulator command
    "steps":   [{"expect": "<regex>", "send": "<text>"}, ...],
    "done":    "<regex that means finished>",
    "timeout": 180,          # hard cap, seconds
    "settle":  3             # seconds to keep running after done before SIGINT
  }
Sends use \\r for CR (0x0D). Output is echoed to stdout (parity bit stripped).
Matching is case-sensitive regex against the accumulated parity-stripped output.
"""
import os, pty, sys, subprocess, select, time, json, signal, re, termios, tty


def set_raw(fd):
    try:
        mode = termios.tcgetattr(fd)
        # raw: no echo, no canonical processing, no CR/LF translation
        tty.setraw(fd)
        # but keep it simple; setraw already clears ECHO/ICANON/OPOST etc.
    except Exception:
        pass


def _sigint(p):
    try:
        p.send_signal(signal.SIGINT)   # the flushing stop (writes CDC surface)
    except Exception:
        pass


def main():
    cfg = json.load(open(sys.argv[1]))
    cmd = cfg["cmd"]
    steps = cfg.get("steps", [])
    done_re = cfg.get("done")
    timeout = cfg.get("timeout", 180)
    settle = cfg.get("settle", 3)

    master, slave = pty.openpty()
    set_raw(slave)
    set_raw(master)
    p = subprocess.Popen(cmd, stdin=slave, stdout=slave, stderr=slave,
                         close_fds=True)
    os.close(slave)

    acc = []            # accumulated parity-stripped chars (as list for speed)
    si = 0              # current step index
    done = False
    start = time.time()
    done_time = None
    stopping = False    # SIGINT sent, now draining nd100x's shutdown output
    stop_deadline = None
    try:
        while True:
            if not stopping and time.time() - start > timeout:
                sys.stderr.write("\n[pty_drive] TIMEOUT\n")
                _sigint(p)
                stopping = True
                stop_deadline = time.time() + 25
            r, _, _ = select.select([master], [], [], 0.3)
            if r:
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                text = "".join(chr(b & 0x7F) for b in chunk)
                sys.stdout.write(text)
                sys.stdout.flush()
                if not stopping:
                    acc.append(text)
                    whole = "".join(acc)
                    # fire the next step whose prompt has appeared
                    if si < len(steps):
                        exp = steps[si]["expect"]
                        if re.search(exp, whole):
                            send = steps[si]["send"].replace("\\r", "\r")
                            os.write(master, send.encode("latin1"))
                            sys.stderr.write("\n[pty_drive] sent step %d: %r\n"
                                             % (si, steps[si]["send"]))
                            si += 1
                            acc = []    # avoid re-matching the same prompt
                            time.sleep(0.3)
                            continue
                    if done_re and not done and re.search(done_re, whole):
                        done = True
                        done_time = time.time()
                        sys.stderr.write("\n[pty_drive] done marker matched\n")
            # once done and all steps fired, settle then SIGINT — but KEEP
            # reading (drain) so nd100x can flush the CDC surface and exit
            # cleanly instead of blocking on a full PTY.
            if not stopping and done and si >= len(steps) and done_time \
               and time.time() - done_time > settle:
                sys.stderr.write("\n[pty_drive] stopping (SIGINT, draining)\n")
                _sigint(p)
                stopping = True
                stop_deadline = time.time() + 25
            if p.poll() is not None:
                break
            if stopping and stop_deadline and time.time() > stop_deadline:
                sys.stderr.write("\n[pty_drive] shutdown timed out; killing\n")
                p.kill()
                break
    finally:
        if p.poll() is None:
            p.kill()
        try:
            os.close(master)
        except Exception:
            pass
    ok = done or (si >= len(steps) and steps)
    sys.stderr.write("\n[pty_drive] finished: steps %d/%d, done=%s\n"
                     % (si, len(steps), done))
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
