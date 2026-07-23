# ASK to the TSS team — should the swap DRUM (540) and CDC (500–507) be *disabled by default* and activated only for TSS?

**Status:** **RESOLVED** — QC reviewed against the TSS source + nd100x. **Option A (gate the drum behind a flag) is approved: safe, zero TSS risk.**
**Context files:** `docs/CDC-DISC-DEVICE.md`, `docs/OVERLAY-DISC-SPEC.md`.
**Emulator:** nd100x (`E:\Dev\Emulators\ND\nd100x\src\devices\drum\`, `...\cdc\`).

---

## RESOLUTION — QC verdict (researched against TSS source + nd100x)

**Net: Option A removes the false 540 error with zero risk to TSS.** The CDC is *already* gated (installed only with `--cdc`, `nd100x.c:377`); **only the drum is installed unconditionally** (`devicemanager.c:126`) — that is exactly what trips the 540 SMD probe error. Gate the drum the same way CDC/SCSI already are.

Answers to the five questions:

1. **Presence — gating is fine.** TSS never probes for the drum/CDC by ident. Gating the drum behind a flag is the established nd100x pattern (CDC/SCSI already do it).
2. **Activation timing — OK to be absent/not-ready until poked,** *provided that once activated the device asserts the status bits TSS polls.* TSS activates-then-polls (`DWAIT`, and `XDRUM`'s BUSY re-poll); it never assumes instant readiness. The only thing that hangs the boot is a device that activates but never asserts ready/complete.
3. **Ident dependence — NONE.** The only `IDENT PL11` in all of TSS (`TSS1.SYMB:1816`) is the level-11 dispatcher, and it recognizes *only ident 4 (Versatec)*. CDC's ident 1 and the drum's ident route to the spurious `NER11` counter — harmless. So changing the drum ident to 2, **or** making it absent-until-activated, breaks nothing in TSS.
4. **Real hardware — UNKNOWN from the source.** The source shows drum/CDC as build-time `"N10`/`"DRUM`/`"CDC` options (the drum is compiled out of *both* golden builds), consistent with "optionally configured," but the source does not state field fitment. **Do not assert this** — it needs a hardware/manual source, not a guess.
5. **CDC interrupt-enable — confirmed poll-only.** TSS's CDC path writes only Activate (bit 2): `AAA 4; IOX LMR` (`TSS1.SYMB:3473`), never the enable bits, and polls `DWAIT`.

**Two issues flagged for follow-up (not blockers for Option A):**
- **Drum ident is a PROVISIONAL `024`** — should be `2` per the manuals (`deviceDrum.h`). Harmless in TSS (routes to NER11), but wrong vs the ND-100 SMD-slot ident.
- **CDC raises its interrupt unconditionally** (`deviceCDC.c:429`), *not* gated on the enable bits — unlike the drum/SMD/floppy which decode them. Currently harmless (lands on NER11), but if interrupt-enable is ever made a real decoded gate, the CDC is where the change is needed.

---

## TL;DR — the one question

The swap **DRUM** (IOX 540–547) and **CDC cartridge disc** (IOX 500–507) were added to nd100x purely so **NORD TSS** can boot. They are **NORD‑1 / NORD‑10‑era peripherals** and are *not* standard on an ND‑100. Because they are always installed, a **normal ND‑100 SINTRAN** device‑configuration probe now throws a **false error** at 540 (details below).

**Proposal:** make the drum and CDC **absent (disabled) by default**, and require an **explicit activation** (config flag / thumbwheel) to install + ready them — which TSS would set. Normal SINTRAN then never sees them → no false error, and we do **not** have to change any ident code.

**We need the TSS team to confirm this is acceptable for TSS**, and to answer the ident question below (we suspect we must *not* silently change the ident).

---

## The observed false error (normal ND‑100 SINTRAN, TPE device‑config diagnostic)

```
SMD 15 MHZ DISC CONTR.   3    540    547                                    565

*** ERROR ***  Device number : 000540B
               Device name   : SMD 15 MHZ DISC CONTR. 3
               No identcode found on level 11D
               Expected identcode : 2B
```

The probe expects an **SMD 15 MHz disc controller #3** at 540 (ident **2**, level 11); nd100x has a **drum** there instead, answering with a different (provisional) ident — so the probe reports "No identcode found."

## What the documentation actually says (NDInsight `Reference-Manuals\`, no assumptions)

- **IOX 540–547 is a *shared / overlapping* device slot.** A machine has **either** a swap drum **or** an SMD‑15 MHz controller #3 there — **never both** — and **both carry ident `2` (octal) on level 11**, because the ident is bound to the *address range*, not the peripheral:
  - ND‑06.016.01 *NORD‑100 I/O System*, App. A p.165: `540–547 | Level 11 | Ident 2 | "Drum 1"`.
  - ND‑30.053.01 *SINTRAN III – How to Order It*, §8 p.37: `SMD 15 MHZ DISC CONTR. #3 | 540–547 | Ident 2` (overlaps the drum slot).
- **CDC cartridge disc**: IOX 500–507, **ident `1`, level 11** (ND‑11.008.01 p.20). Control word bit 0 = enable‑interrupt‑on‑ready, bit 1 = enable‑interrupt‑on‑error, bit 2 = Activate.

So the *diagnostic is correct*. The problem is that nd100x presents a legacy peripheral, with a **provisional/incorrect ident**, in a slot a standard ND‑100 SINTRAN expects to probe.

## Current emulator state (for reference)

- `deviceDrum.h` — `DRUM_IDENT_CODE = 024`, explicitly commented **"PROVISIONAL – confirm against TSS level‑11 handler"**. Documentation says the slot ident is **2**, not 024.
- `deviceSMD.c` — the SMD‑at‑540 entry uses ident `023` (also not the documented `2`).
- Both drum and CDC are installed **unconditionally** at machine init.

---

## The two options

### Option A (proposed) — disable drum + CDC by default; activate explicitly for TSS
- Do **not** install the drum (540) or CDC (500–507) on a normal machine.
- Add an explicit **activation** (e.g. a config flag / thumbwheel the TSS config sets) that installs and readies them.
- Result: normal SINTRAN device probes see *nothing* at 540 / 500–507 → **no false error**, and **no ident change is needed**. TSS gets its drum + CDC when it asks for them.
- Historically faithful: these are NORD‑1/NORD‑10 peripherals; an ND‑100 wouldn't have them unless deliberately configured.

### Option B (alternative we are *hesitant* about) — correct the ident to the documented `2`
- Set the drum (and the SMD‑540 slot) to ident `2` per the manuals, keeping the device always present.
- **Concern:** we suspect it may **not** be safe to change what TSS's own level‑11 / device‑detection code expects. This is exactly what we need you to confirm.

---

## Questions for the TSS team (please answer before we change anything)

1. **Presence:** Does TSS require the **drum (540)** and **CDC (500–507)** to be present + ready *unconditionally at boot*, or is it acceptable to gate them behind an **explicit activation** (Option A) so a normal ND‑100 SINTRAN config has neither installed?
2. **Activation timing:** If gated — is it OK for them to become "ready" only *after* TSS activates them (control‑word / activate), i.e. they report *not present / not ready* until TSS pokes them? Or does TSS assume they are ready the instant it first touches the registers?
3. **Ident dependence:** Does any TSS code path depend on the **drum's ident code** on level 11 (does its swap/overlay driver run `IDENT PL 11` and check the value)? The docs say the slot ident is `2`; nd100x currently answers `024` (provisional). Would correcting it to `2` — or making the device absent until activated — break any TSS assumption?
4. **Real hardware:** On the real NORD‑1/NORD‑10 that ran TSS, were the drum + CDC **always fitted**, or **optionally installed**? (i.e., is "disabled until activated" faithful to the machines TSS targeted?)
5. **CDC interrupt‑enable:** TSS reportedly **polls DWAIT and never enables the CDC interrupt**. Can we confirm TSS does **not** rely on the CDC (or drum) raising a level‑11 interrupt *unless* it has explicitly set the control‑word enable bit (bit 0 / bit 1)? (This affects whether we make interrupt‑enable a real decoded control bit, as the SMD/floppy drivers do.)

---

## Why we're asking rather than just fixing

Changing an ident code, or the presence/readiness of a device TSS boots on, can silently change TSS's device‑detection path. TSS is recovered 1973 source validated only against symbol‑table dumps; a wrong guess here can move the boot without any compile‑time signal. We'd rather confirm the intended hardware model with you first.
