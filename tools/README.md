# `tools/`

Analysis instruments that are **not** part of a build and **not** a step in
the bring-up procedure.

| subfolder | what it is |
|---|---|
| [`analysis/`](analysis/README.md) | read a *built* artifact — a symbol dump or the `Build/` tree — and report something about it. Nothing here runs the emulator. |

Where the neighbours sit:

- **`../mac-c/`** — builds TSS from source. Nothing here participates in a
  build or is a prerequisite of `make`.
- **`../bringup/`** — everything that drives the **nd100x emulator**: the
  steps the root `Makefile` calls (`verify-disc.py`, `check-robj-encoding.py`,
  `dap_bringup.py`, `pty_drive.py`, `patch-nd100x.py`) plus the trace probes
  used when a step misbehaves (`logon_trace_probe.sh`, `dkadr_trace.sh`,
  `dkadr_pairs.awk`, `tss.cfg`).

The dividing line is the emulator: if it boots TSS, it lives in `bringup/`;
if it only reads files the build produced, it lives here.
