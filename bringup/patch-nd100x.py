#!/usr/bin/env python3
"""Make nd100x's CDC and drum devices WRITE-THROUGH (persist each written sector
to the image file immediately + fflush) instead of buffering the surface in RAM
and writing it back only on a clean shutdown. This makes disc writes survive any
stop (SIGINT/crash/kill), matching how SMD/floppy already behave."""
import sys

import os
# nd100x lives outside this repo: point ND100X_SRC at its source tree.
ND100X_SRC = os.environ["ND100X_SRC"]
CDC = os.path.join(ND100X_SRC, "src/devices/cdc/deviceCDC.c")
DRUM = os.path.join(ND100X_SRC, "src/devices/drum/deviceDrum.c")

cdc_old = """    case CDC_OP_WRITE: /* 01: core -> disc (persisted via the backing file) */
        for (uint32_t i = 0; i < count; i++)
        {
            int32_t w = Device_DMARead(core + i);
            d->surface[wordOff + i] = (uint16_t)(w & 0xFFFF);
        }
        break;"""

cdc_new = """    case CDC_OP_WRITE: /* 01: core -> disc */
        for (uint32_t i = 0; i < count; i++)
        {
            int32_t w = Device_DMARead(core + i);
            d->surface[wordOff + i] = (uint16_t)(w & 0xFFFF);
        }
        /* WRITE-THROUGH: persist the just-written sector(s) to the backing
         * file immediately (big-endian / ND word order) and flush, so a
         * non-clean stop (SIGINT, crash, kill) can NEVER lose disc writes.
         * Previously the surface was written back only in Cdc_Destroy, which
         * the SIGINT handler's exit(0) skips -> lost format/cold-start writes. */
        if (d->backingFile)
        {
            fseek(d->backingFile, (long)wordOff * 2L, SEEK_SET);
            for (uint32_t i = 0; i < count; i++)
            {
                putc((d->surface[wordOff + i] >> 8) & 0xFF, d->backingFile);
                putc(d->surface[wordOff + i] & 0xFF, d->backingFile);
            }
            fflush(d->backingFile);
        }
        break;"""

drum_old = """        default:
            break;
        }
    }

    /* Queue the delayed completion, exactly like SMD."""

drum_new = """        default:
            break;
        }
    }

    /* WRITE-THROUGH: for a WRITE, persist the transferred range to the backing
     * file immediately (big-endian) and flush, so no non-clean stop can lose
     * drum writes (previously written back only in Drum_Destroy). */
    if (func == DRUM_FUNC_WRITE && d->backingFile)
    {
        fseek(d->backingFile, (long)wordOffset * 2L, SEEK_SET);
        for (uint32_t i = 0; i < count; i++)
        {
            putc((d->surface[wordOffset + i] >> 8) & 0xFF, d->backingFile);
            putc(d->surface[wordOffset + i] & 0xFF, d->backingFile);
        }
        fflush(d->backingFile);
    }

    /* Queue the delayed completion, exactly like SMD."""


def patch(path, old, new):
    txt = open(path).read()
    n = txt.count(old)
    if n != 1:
        print("FAIL: %s — expected 1 match, found %d" % (path, n))
        sys.exit(1)
    open(path, "w").write(txt.replace(old, new))
    print("patched: %s" % path)


patch(CDC, cdc_old, cdc_new)
patch(DRUM, drum_old, drum_new)
print("done")
