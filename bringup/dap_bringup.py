#!/usr/bin/env python3
"""Fully automated TSS bring-up driver.

nd100x keeps the CDC disc surface in memory and only writes it back to the image
file during the clean shutdown path (cleanup_machine -> Cdc_Destroy). A Ctrl-C
(SIGINT) sets CPU_STOPPED and calls exit(0), which NEVER runs that path, so disc
writes are lost. The DAP debugger's `disconnect{terminateDebuggee}` sets
CPU_SHUTDOWN, letting the main loop exit and flush the disc. So every phase that
must persist the disc is driven under the DAP debugger and stopped via terminate.

This script runs the bring-up phases (format, cold-start) unattended: it launches
nd100x with the debugger, drives the console over DAP, and terminates cleanly.

Usage:  python3 dap_bringup.py <repo-root> [--nd PATH] [--port N] [--login]
"""
import json, os, re, socket, subprocess, sys, time


# ── minimal synchronous DAP client (Content-Length framed JSON) ──────────────
class DAP:
    def __init__(self, sock):
        self.s = sock
        self.seq = 1
        self.buf = b""

    def _send(self, command, arguments=None):
        seq = self.seq
        self.seq += 1
        msg = {"seq": seq, "type": "request", "command": command}
        if arguments is not None:
            msg["arguments"] = arguments
        body = json.dumps(msg).encode()
        self.s.sendall(b"Content-Length: %d\r\n\r\n%s" % (len(body), body))
        return seq

    def _read_msg(self, timeout):
        """Read one framed DAP message, or None on timeout."""
        self.s.settimeout(timeout)
        try:
            while b"\r\n\r\n" not in self.buf:
                chunk = self.s.recv(4096)
                if not chunk:
                    return None
                self.buf += chunk
            head, rest = self.buf.split(b"\r\n\r\n", 1)
            length = 0
            for line in head.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1])
            while len(rest) < length:
                chunk = self.s.recv(4096)
                if not chunk:
                    return None
                rest += chunk
            self.buf = rest[length:]
            return json.loads(rest[:length].decode("utf-8", "replace"))
        except socket.timeout:
            return None

    def request(self, command, arguments=None, timeout=15):
        """Send a request and wait for its response (buffering events meanwhile)."""
        seq = self._send(command, arguments)
        deadline = time.time() + timeout
        while time.time() < deadline:
            m = self._read_msg(0.5)
            if m is None:
                continue
            if m.get("type") == "response" and m.get("request_seq") == seq:
                return m
            if m.get("type") == "event":
                self._on_event(m)
        raise TimeoutError("no response to %s" % command)

    # console output arrives as DAP 'output' events; collect their text
    console = []

    def _on_event(self, m):
        if m.get("event") == "output":
            body = m.get("body", {})
            if body.get("category") in ("stdout", "console"):
                txt = body.get("output", "")
                # parity bit is already stripped in nd100x's output text
                self.console.append(txt)
                sys.stdout.write(txt)
                sys.stdout.flush()

    def drain(self, seconds):
        """Read events for `seconds`, feeding console output to the buffer."""
        end = time.time() + seconds
        while time.time() < end:
            m = self._read_msg(0.3)
            if m and m.get("type") == "event":
                self._on_event(m)

    def console_text(self):
        return "".join(self.console)

    def reset_console(self):
        self.console = []

    def console_write(self, text, terminal=192):
        # send raw bytes; CR must be 0x0D. We pass hex to be unambiguous.
        hexstr = text.encode("latin1").hex()
        self.request("consoleWrite",
                     {"terminal": terminal, "input": hexstr, "hex": True})


def connect_dap(port, secs=20):
    """Connect to the DAP server once (retrying), returning the live socket.

    Do NOT probe with a throwaway connect/close first — nd100x's DAP accepts a
    single client and a probe would consume it, resetting the real connection.
    """
    deadline = time.time() + secs
    while time.time() < deadline:
        try:
            return socket.create_connection(("127.0.0.1", port), timeout=1.0)
        except OSError:
            time.sleep(0.25)
    return None


def run_phase(name, nd, boot_args, port, steps, done_re, timeout=180,
              settle=3.0):
    """Launch nd100x under DAP, drive the console, terminate cleanly."""
    print("\n=== phase: %s ===" % name, flush=True)
    p = subprocess.Popen([nd] + boot_args + ["--debugger", "--port=%d" % port],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ok = False
    try:
        s = connect_dap(port)
        if s is None:
            print("[%s] DAP port never opened" % name, file=sys.stderr)
            return False
        dap = DAP(s)
        dap.reset_console()
        dap.request("initialize", {"clientID": "tss-auto", "adapterID": "nd100x",
                                   "linesStartAt1": True, "columnsStartAt1": True,
                                   "pathFormat": "path"})
        dap.request("attach", {})
        dap.request("configurationDone")
        dap.request("consoleEnable", {"terminal": 192, "enable": True})
        dap.request("continue", {"threadId": 1})   # CPU now free-runs

        si = 0
        start = time.time()
        done_since = None
        while time.time() - start < timeout:
            dap.drain(0.5)
            whole = dap.console_text()
            if si < len(steps) and re.search(steps[si]["expect"], whole):
                send = steps[si]["send"].replace("\\r", "\r")
                print("\n[%s] -> step %d: %r" % (name, si, steps[si]["send"]),
                      flush=True)
                dap.console_write(send)
                si += 1
                dap.reset_console()
                time.sleep(0.5)
                continue
            if done_re and si >= len(steps) and re.search(done_re, whole):
                if done_since is None:
                    done_since = time.time()
                    print("\n[%s] done marker seen" % name, flush=True)
                elif time.time() - done_since > settle:
                    ok = True
                    break
        if not done_re and si >= len(steps):
            ok = True
        # clean terminate -> CPU_SHUTDOWN -> cleanup_machine -> CDC flush
        print("\n[%s] terminating (clean shutdown, flushes disc)" % name,
              flush=True)
        try:
            dap.request("disconnect", {"terminateDebuggee": True, "restart": False},
                        timeout=8)
        except Exception:
            pass
        s.close()
    finally:
        try:
            p.wait(timeout=25)     # let it flush the CDC surface and exit
        except Exception:
            p.kill()
    print("[%s] %s" % (name, "OK" if ok else "FAILED (marker not seen)"),
          flush=True)
    return ok


def main():
    root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
    nd = None
    do_login = "--login" in sys.argv
    base_port = 1783
    for i, a in enumerate(sys.argv):
        if a == "--nd":
            nd = sys.argv[i + 1]
        if a == "--port":
            base_port = int(sys.argv[i + 1])
    if not nd:
        for c in (os.path.expanduser("~/repos/nd100x/build/bin/nd100x"),
                  os.path.expanduser("~/repos/nd100x/build-linux/bin/nd100x")):
            if os.path.exists(c):
                nd = c
                break
    if not nd or not os.path.exists(nd):
        print("nd100x not found; pass --nd PATH", file=sys.stderr)
        sys.exit(2)

    work = os.path.join(root, "Build", "bringup")
    for i, a in enumerate(sys.argv):
        if a == "--work":
            work = os.path.abspath(sys.argv[i + 1])
    cdc = os.path.join(work, "cdc.img")
    bpun = os.path.join(work, "tss.bpun")
    drum = os.path.join(work, "drum.img")
    minit = os.path.join(root, "Build", "minit", "minit.bpun")
    for f in (cdc, bpun, drum, minit):
        if not os.path.exists(f):
            print("missing %s — run 'make prepare' first" % f, file=sys.stderr)
            sys.exit(2)

    ok = run_phase(
        "format", nd,
        ["--boot=bpun", "--image=%s" % minit, "--cdc=%s" % cdc],
        base_port,
        [{"expect": "FIRST DISK ADDRESS", "send": "4470\\r"},
         {"expect": "LAST DISK ADDRESS", "send": "4670\\r"},
         {"expect": "INITIALIZE OR UPDATE", "send": "I\\r"}],
        done_re="FINISHED", timeout=120)
    if not ok:
        sys.exit(1)

    ok = run_phase(
        "coldstart", nd,
        ["--boot=bpun", "--image=%s" % bpun, "--cdc=%s" % cdc,
         "--drum=%s" % drum, "--opr=131313", "--start=7"],
        base_port + 1,
        [],                          # no input; SINIT creates SYSTEM by itself
        done_re="@ENTER", timeout=120)
    if not ok:
        sys.exit(1)

    print("\n=== bring-up complete: disc formatted + SYSTEM created ===")
    print("Disc is ready. Run 'make login' for an interactive session,")
    print("or re-run with --login to auto-drive a first login as SYSTEM.")


if __name__ == "__main__":
    main()
