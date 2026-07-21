# NORD TSS boot process — as executed on nd100x

Traced instruction-by-instruction from the DRUM-N10 build booting on nd100x
(2026-07-21). Facts marked **[VERIFIED]** are read directly from the source or
the live CPU trace; **[INFERRED]** is derived.

## The cold-start sequence

```
BPUN autostart
   │  (mac-as -e ISTRT records ISTRT=025076 as the tape's start address)
   ▼
ISTRT  025076   SAA 1 ; STA I (IDEV ; STA I (ODEV      set console in/out device = 1
   │  falls through
   ▼
START  025101   JPL I (ISTK                            initialise the stack
                LDA I (RMODE ; JAZ LEV2A               RMODE = 0 ("NORMAL MODE") -> take LEV2A
   │
   ├─ RMODE != 0 ─▶ LEV2 (025104): INTDS ; init buffers ; INTEN ; SXBRK ;
   │                 core-size check ; JPL I (LOGON     ← the login path (NOT taken)
   │
   └─ RMODE == 0 ─▶ LEV2A (025103): JMP I (XSTAR        ← the normal path (taken)
                    ▼
XSTAR  033601   GOVER OV19,S1,XSTAR                     enter the command processor (overlay 19)
```

**[VERIFIED]** `RMODE, 0  %0 => NORMAL MODE` (TSS2:73). The trace shows
`LDA I (RMODE` loading `A=0`, so `JAZ` branches to `LEV2A -> XSTAR`. This is the
**correct** normal cold path; `LEV2`/`LOGON` is a special reload mode
(RMODE is only set non-zero in TSS1:3161 and TSS4:547).

## The overlay mechanism — this is where it stops

`XSTAR` is not ordinary code: it is `GOVER OV19,S1,XSTAR`, an **overlay call**.

**[VERIFIED]** The `GOVER` macro (TSS3:15) expands to:
```
   STX I *+5 ; SAX 0 ; SWAP DP SX ; <overlay#> ; <seg> ; SAVX
```
The trace at XSTAR matches exactly:
```
033601  STX I 5        save caller context into the overlay-number slot
033602  SAX 0
033603  SWAP SX DP     P := X  -> transfer to the overlay dispatcher
000000  JMP I 3        location 0 vector ...
016456  GOVX ...       ... = the overlay dispatcher ("go overlay execute")
```
So location 0 (`JMP I 3` -> `GOVX=016456`) is the **overlay dispatcher**, not a
plain monitor return. `GOVER` hands it the overlay number; the dispatcher is
meant to ensure overlay OV19 is resident (loading it from the swap disc if not)
and jump to it.

**Where the overlays come from [VERIFIED].** They are written to the swap disc
by **`SOVER`** (TSS3): `... JPL I (8DUMP` / `JPL I (XDISK` writes each assembled
overlay to the `ROVER` area on disc, at assembly time, on a machine where MAC
is resident with mass storage. `SOVER` is invoked through MAC's `)SYMBOL`
user-routine hook — i.e. it is the `)SOVER` / `)8DUMP` that **mac-c accepts and
ignores** (it cannot execute ND-100 code). The `)9MOVE ROVER VOR VORS` overlay
relocation is likewise a no-op in mac-c.

**Consequence [VERIFIED by trace].** The overlays are **not on the drum**, and
the overlay currently sitting in the `ROVER` area of the assembled image is
whichever overlay mac-c laid down last — not OV19. The dispatcher therefore runs
the wrong overlay body; the trace wanders (031474, ...) and finally executes
`JPL I` through a pointer holding `000013`, landing on:
```
000013  JMP 0          jump-to-self, PIE=0000 (interrupts disabled)
```
a dead spin with interrupts off — reached **without a single IOX** (no device
I/O ever happened; 0 IOX in 30 000 instructions).

## Summary: what works, what blocks LOGON

| stage | state |
|---|---|
| BPUN autostart -> ISTRT | **works** (mac-as `-e ISTRT`) |
| ISTRT / START / ISTK | **works** — console device set, stack initialised |
| RMODE branch -> XSTAR | **works** — correct normal path |
| overlay dispatch (GOVER/GOVX -> OV19) | **fails** — OV19 is not resident and not on disc |
| command processor / LOGON | **not reached** |

**The single blocker is the overlay system.** TSS's command processor (`XSTAR`,
overlay 19) and `LOGON` are disc overlays. Reaching a login prompt needs the
overlays present on the swap drum and the `GOVX` dispatcher able to load them —
i.e. the `SOVER`/`ROVER` machinery that mac-c does not reproduce.

## Paths to a login prompt (next phase)

1. **Implement `SOVER` in mac-c** — write each assembled overlay to a drum image
   at the `ROVER` disc address the way `SOVER` does (`OVDK` mapping, TSS1:2593).
   Then boot with that drum. Requires decoding the overlay-to-disc layout.
2. **Run real MAC/FMAC on the emulator** to build TSS with real `SOVER` (it
   executes, so overlays and self-save happen naturally) — the long-standing
   "Route B". Heavier, but faithful.
3. **Force the LEV2/LOGON path** (set RMODE != 0 or enter at LEV2=025104) to see
   how far the *login* code gets before it, too, needs an overlay — a cheap
   experiment that would confirm whether LOGON itself is resident or overlaid.

Open, still [INFERRED]: whether the dispatcher tried a disc read at all (no IOX
was seen, so it likely took a "believed-resident" fast path against the wrong
in-memory overlay), and the exact `OVDK` disc layout each overlay maps to.

## Experiment result: the LEV2/LOGON path also needs overlays

**[VERIFIED]** Entering directly at `LEV2=025104` (forcing the login path,
`--start=025104`) behaves the same: 40 000 instructions, **0 IOX**, never
reaches `LOGON` (032735), and settles into the identical `000013 JMP 0` hang.
So `LOGON` is not reachable while the overlay system is inert — the blocker is
the same on both paths. The overlay work (path 1 or 2 above) is required before
a login prompt is possible.

## Convergence test: overlays on disc are necessary but NOT sufficient (2026-07-21)

Both halves are built and validated: mac-c emits the CDC overlay image
(`mac-as -c`, OV19/XSTAR -> sector 0254), and nd100x has a CDC disc device at
IOX 500 serving it. Booting with `--drum` + `--cdc`:

**[VERIFIED] TSS still hangs at location 13 with 0 IOX — it never reads the
disc.** The overlay dispatcher `GOVX` (016456) does an "already loaded?" check:
```
016462 LDA ,X 0      overlay number from the GOVER call site
016463 SUB I 17      minus the "currently loaded overlay" variable
016464 JAZ 7         equal -> believe it is resident -> SKIP the disc read
```
On the cold image this check FALSELY passes, so GOVX skips the read and jumps
into the ROVER area (031474), which holds the wrong (last-assembled) overlay ->
runs off -> location 13. Separately, interrupts are essentially never enabled
(1 ION/IOF in 300 000 instructions), so the interrupt-driven overlay reader
(S5, level 5) could not service a read even if one were requested.

**Root cause [INFERRED, strong]:** entering the resident image at ISTRT does
not replicate TSS's full cold-boot initialisation. Two things are missing:
(1) the "currently loaded overlay" tracking cell is not reset to an invalid
value (so the first GOVER triggers a real load), and (2) the interrupt system
(levels, vectors, enable) and the level-5 overlay reader are not brought up.
The original boot established this machine state before entering; the BPUN load
+ jump-to-ISTRT does not.

**Next frontier:** reverse-engineer TSS's complete cold-boot init sequence — how
the overlay-tracking state is initialised and how/where interrupts and the
level-5 reader are enabled — and either enter at the right point or set up that
state. This is distinct from (and downstream of) the now-complete overlay-image
and CDC-device work.
