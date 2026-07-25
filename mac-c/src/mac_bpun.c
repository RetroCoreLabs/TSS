/* MAC-C - image / CDC disc / BPUN tape readers and writers
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* helper: write one big-endian 16-bit word */
static void put_be16(FILE *f, uint16_t w)
{
    fputc((w >> 8) & 0xFF, f);
    fputc(w & 0xFF, f);
}

bool mac_write_image(mac_state *st, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        return false;
    }
    uint16_t base = (st->hi_used >= st->lo_used) ? st->lo_used : 0;
    uint16_t count = (st->hi_used >= st->lo_used)
                         ? (uint16_t)(st->hi_used - st->lo_used + 1)
                         : 0;
    fwrite("MACIMG", 1, 6, f);
    put_be16(f, base);
    put_be16(f, count);
    for (uint16_t i = 0; i < count; i++)
    {
        put_be16(f, st->mem[base + i]);
    }
    fclose(f);
    return true;
}

/* -------------------------------------------------------------------------
 * CDC-disc overlay image writer  (docs/TSS-ARCHITECTURE.md (overlay chapter))
 * -------------------------------------------------------------------------
 * Writes a raw disc image that the RUNNING TSS overlay reader (routine S5)
 * loads overlays from. We do NOT re-run the SOVER machine code (that needs
 * ND-100 execution); we reproduce its run-time disc CONTRACT directly, which
 * TSS-ARCHITECTURE.md (overlay chapter) sec 4 proves is identical either way.
 *
 * Disc contract [VERIFIED TSS-ARCHITECTURE.md (overlay chapter) sec 2 + sec 4]:
 *   sector(n) = OVDK + 2*n                (256-word sectors; SHA 1 = *2)
 *   overlay n -> two consecutive sectors  sec0 = OVDK+2n, sec1 = sec0+1
 *   sec0 <- overlay words [0 .. 255], sec1 <- overlay words [256 .. 511]
 *   op READ into core ROVER (sec0) and ROV4=ROVER+400 (sec1) at run time.
 *
 * Where the overlay words come from [VERIFIED TSS-ARCHITECTURE.md (overlay chapter) sec 1.3,
 * sec 6]: the "MACF OVERX macro block-copies each overlay from its assembly
 * window at ROVER into a distinct VOR slot via )9MOVE ROVER VOR VORS, so
 * after assembly overlay n lives at image words [VOR_base + n*VORS ..
 * +VORS). We read those windows straight out of st->mem. (cmd_9move must
 * have run - it does in the ASSYSA/DRUM builds via OVERX.)
 *
 * Constants are resolved from the build's OWN symbol table, never hard-coded
 * (TSS-ARCHITECTURE.md (overlay chapter) sec 7 step 1):
 *   OVDK  = base disc sector           (0160 for CDC non-DEBUG; 0360 DEBUG)
 *   VORS  = overlay/window size words  (01000 = 512 = two 256-word sectors)
 *   RQR   = overlay count              (037 = 31; RQR bumps once per OVERL)
 *   VOR   = final staging pointer      (077000); VOR_base = VOR - RQR*VORS.
 *
 * [INFERRED - TSS-ARCHITECTURE.md (overlay chapter) sec 8 item 1] The window-index ->
 * overlay-number mapping is not fully pinned in the TSS source. We implement
 * the straightforward PARALLEL mapping (window n -> overlay n -> sectors
 * OVDK+2n), which is consistent with RQR incrementing in lockstep with the
 * VOR advance (sec 5 cross-check: (077000-040000)/01000 = 037 = RQR). To let
 * the boot test verify it, we emit to stderr a table of
 * {window addr, sector, first 4 words} for every overlay.
 *
 * Image byte layout [VERIFIED task spec / nd100x disc device]: sector S
 * starts at byte offset S*256*2; each word is big-endian (ND word order).
 *
 * PHYSICAL PLACEMENT: the running disc driver does NOT use the logical sector
 * directly - it runs it through DKADR (a logical->physical address conversion)
 * before loading the CDC controller's block-address register. So each overlay
 * must be written at the PHYSICAL sector cdc_dkadr(logical), not at the linear
 * logical sector. See cdc_dkadr() below and docs/TSS-ARCHITECTURE.md (overlay chapter) sec
 * "DKADR physical addressing".                                               */

/* -------------------------------------------------------------------------
 * cdc_dkadr - forward transcription of the TSS DKADR routine's LOGICAL ->
 * PHYSICAL CDC disc-address mapping for the CDC N10 (DRUM) build, unit 0.
 *
 * DKADR (src/TSS1.SYMB:3589-3646, "CONVERT NCR DISK ADDRESS TO CDC DISK
 * ADDRESS") is what the running driver applies to a logical disc sector before
 * it loads the CDC controller block-address register (IOX 503). The overlay
 * reader path is  S5 (TSS1.SYMB:3069-3073) -> SDISK (9SDK) -> DKOP -> JPL I
 * (DKADR -> IOX LBA. To be found by that read, each overlay must be placed at
 * the SAME physical sector DKADR produces, not at the raw logical sector.
 *
 * ---- HISTORY / two superseded formulas ----
 * (1) An early version returned 72*L. It was traced while DKADR's divide-by-12
 *     ("RGDIV ST") was UNDEFINED, so the divide never ran and DKADR degenerated
 *     to a plain multiply. Fixed by RGDIV=RDIV (TSS1.SYMB:3616).
 * (2) A second version returned 8*floor(L*65537/12) + 64*L (giving 077300 for
 *     L=0244). It was traced while the mac-c assembler MISCOMPILED the shift
 *     modifier "SHR": MAC's SHR permsym is 0200 (MAC.BPUN permsym table at
 *     0xE9B3), which is bit-identical to SHD's register-select bit, so a plain
 *     ADDITIVE SHR turned every "... SHR n" right shift into a LEFT shift of a
 *     different register. In DKADR that broke three shifts:
 *        "SAD ZIN SHR 20" (010016), "SHT ZIN SHR 5" (010026), "SHA SHR 6"
 *     making the SAD divide-setup a no-op and mis-scaling the result.
 *     With SHR fixed (eval_expr: SHR subtracts the following count, per
 *     ND-60.096.01 sec 2.3.8 "shift right, gives negative shift counter") the
 *     three shifts assemble to their correct RIGHT-shift encodings
 *     (0156760 / 0156173 / 0154572) and DKADR runs its TRUE algorithm.
 *
 * ---- The TRUE as-built algorithm (SHR fixed) ----
 * [VERIFIED by single-stepping the running DKADR under nd100x --mms1 for
 * logical 0244, PC 010006..010052; EVERY register value below was read off the
 * live --trace, A/D/T columns. See mac-c/dkadr_trace.sh + dkadr_pairs.awk.]
 *
 * Registers are 16-bit; AD is the 32-bit double accumulator (A high, D low).
 * The driver passes the logical sector L in A/T at entry.
 *
 *   010012 AND (17777    A := L & 017777    (13 significant NCR sector bits)
 *   010013 COPY DD SA    D := A = L
 *   010016 SAD ZIN SHR 20  AD >>= 16 (0o20=16, RIGHT shift, zero-in): with
 *                        A=D=L before, AD=(L<<16)|L; after: A=0, D=L. So the
 *                        DIVIDEND becomes exactly L (not (L<<16)|L). This is the
 *                        step the SHR bug had neutralised. [VERIFIED: 010017
 *                        A=000000 D=000244 for L=0244.]  (TSS1.SYMB:3617)
 *   010017 SAT 14        T := 12 decimal   (0o14 = 12, the divisor)
 *   010020 RDIV ST       AD / T -> quotient A, remainder D:
 *                        Q := L / 12 ,  R := L mod 12
 *                        [VERIFIED: L=0244(164.) -> A=0o15(Q=13) D=0o10(R=8)]
 *   010021 COPY DT SA    T := A = Q
 *   010026 SHT ZIN SHR 5 T := T >> 5 (T=0 here after LDT DKTT, so no-op)
 *   010027 RORA DT SA    T := A = Q                             (TSS1:3624)
 *   010030 SHT 5         T := (Q << 5) & 0xFFFF                 (TSS1:3624)
 *   010031 MPY (14       A := (Q * 12) & 0xFFFF                 (TSS1:3628)
 *   010032 RSUB DD SA    D := (L - 12*Q) & 0xFFFF = R           (TSS1:3628)
 *   010033 RADD DD SD    D := (D + D) & 0xFFFF = 2*R            (TSS1:3628)
 *   010034 RADD DT SD    T := (T + D) & 0xFFFF = 32*Q + 2*R
 *                                                  <-- RESULT   (TSS1:3628)
 *   010035..010052       unit-1 displacement (0o52600, TSS1:3632) is NEVER added
 *                        for the overlay disc (unit 0: the SSK skip at 010037 is
 *                        taken); the returned value is ALWAYS T ("COPY DA ST;
 *                        EXIT AD1", TSS1:3642). The DKLIM=626 legality clamp
 *                        (TSS1:3636-3638) extracts the cylinder via the now-
 *                        correct "SHA SHR 6" (A := T>>6 = 6 for L=0244) and only
 *                        decides success vs failure; it does not change T.
 *                        [VERIFIED - live trace returns T=0660 and takes the
 *                        legal path; IOX 503 loads A=000660.]
 *
 * Closed form, all arithmetic mod 2^16:
 *     DKADR(L) = 32*floor(L/12) + 2*(L mod 12)
 * This is a base-12 -> "track*32 + 2*sector" repack (12 = sectors per track).
 * Live anchor: DKADR(0244) = 32*13 + 2*8 = 432 = 0o660.
 * [VERIFIED - IOX 503 A=000660 in --trace, and matched writer==reader by the
 * reboot test for every overlay sector the boot touches.]
 *
 * The mapping is monotonic and DENSE (unlike the buggy 077300 scatter): over the
 * overlay logical range 0160..0257 the physical sector runs 0o560..0o716, so the
 * image is small (< 512 sectors). mac_write_cdc_disc still SCANS all overlay
 * sectors to size the image, which is correct for any monotonic or scattered
 * mapping.                                                                     */
uint32_t cdc_dkadr(uint16_t logical_sector)
{
    /* Keep only the 13 significant NCR sector bits (TSS1.SYMB:3593 "AND (17777");
     * the unit bits are 0 for the overlay disc (unit 0).                      */
    uint32_t L = (uint32_t)(logical_sector & 017777u);

    /* "SAD ZIN SHR 20" (010016) shifts AD right 16 so the dividend is L itself,
     * then "RDIV ST" with T=12 (SAT 14) divides: Q = L/12, R = L mod 12.
     * [VERIFIED live: L=0244 -> Q=0o15, R=0o10.] (TSS1.SYMB:3617-3618)         */
    uint32_t Q = L / 12u;
    uint32_t R = L % 12u;

    /* Result T = (Q<<5) + 2*R = 32*Q + 2*R, masked to 16 bits exactly as the
     * ND-100 T register does. Built by SHT 5 (T=Q<<5, 010030), then
     * RSUB/RADD/RADD adding 2*R (010032-010034). (TSS1.SYMB:3624-3628)         */
    uint32_t T = ((Q << 5) + (R << 1)) & 0xFFFFu;
    return T;
}

bool mac_write_cdc_disc(mac_state *st, const char *path)
{
    /* one 256-word sector, the CDC transfer unit (TSS-ARCHITECTURE.md overlay chapter sec 4) */
    enum { SECTOR_WORDS = 256 };

    uint16_t ovdk  = sym_lookup_value(st, "OVDK");
    uint16_t vors  = sym_lookup_value(st, "VORS");
    uint16_t count = sym_lookup_value(st, "RQR");
    uint16_t vend  = sym_lookup_value(st, "VOR");

    if (!sym_is_defined(st, "OVDK") || !sym_is_defined(st, "VORS") ||
        !sym_is_defined(st, "RQR")  || !sym_is_defined(st, "VOR"))
    {
        fprintf(stderr, "mac_write_cdc_disc: need OVDK, VORS, RQR and VOR "
                        "in the symbol table (this must be an overlay/OVERX "
                        "build)\n");
        return false;
    }
    if (vors == 0 || count == 0)
    {
        fprintf(stderr, "mac_write_cdc_disc: VORS=%o RQR=%o - nothing to "
                        "write\n", vors, count);
        return false;
    }

    /* sectors occupied by one overlay window (2 for the standard 512-word
     * window); the disc formula sector = OVDK + spo*n generalises OVDK+2n.  */
    uint16_t spo = (uint16_t)(vors / SECTOR_WORDS);
    if (spo == 0)
    {
        spo = 1;
    }
    uint16_t vbase = (uint16_t)(vend - (uint16_t)(count * vors));

    /* Image size: every overlay's PHYSICAL sector = cdc_dkadr(logical). The
     * CORRECTED DKADR mapping (32*floor(L/12)+2*(L mod 12)) is dense and small
     * over the overlay range, but to stay robust for ANY mapping we still scan
     * EVERY overlay sector and take the true maximum physical sector, so the
     * image (and every write below) stays in bounds.                          */
    uint32_t max_phys = 0u;
    for (uint16_t mn = 0; mn < count; mn++)
    {
        for (uint16_t ms = 0; ms < spo; ms++)
        {
            uint16_t mlog = (uint16_t)((uint32_t)ovdk + (uint32_t)spo * mn + ms);
            uint32_t mp = cdc_dkadr(mlog);
            if (mp > max_phys)
            {
                max_phys = mp;
            }
        }
    }
    uint32_t total_sectors = max_phys + 1u;
    uint32_t total_words = total_sectors * SECTOR_WORDS;

    uint16_t *img = (uint16_t *)calloc(total_words, sizeof(uint16_t));
    if (img == NULL)
    {
        fprintf(stderr, "mac_write_cdc_disc: out of memory\n");
        return false;
    }

    fprintf(stderr, "=== CDC overlay disc image (OVDK=%o VORS=%o RQR=%o "
                    "VOR=%o -> VOR_base=%o) DKADR physical placement ===\n",
            ovdk, vors, count, vend, vbase);
    fprintf(stderr, "  ovl  window  logsec  physsec  first 4 words\n");

    for (uint16_t n = 0; n < count; n++)
    {
        uint16_t window = (uint16_t)(vbase + (uint16_t)(n * vors));
        for (uint16_t sidx = 0; sidx < spo; sidx++)
        {
            /* logical sector the running reader computes (OVDK + 2n [+1]);
             * physical sector = DKADR(logical) - where the read lands.       */
            uint16_t logical = (uint16_t)((uint32_t)ovdk + (uint32_t)spo * n + sidx);
            uint32_t sector = cdc_dkadr(logical);
            uint32_t dst = sector * SECTOR_WORDS;
            uint16_t srcbase = (uint16_t)(window + (uint16_t)(sidx * SECTOR_WORDS));
            for (uint16_t w = 0; w < SECTOR_WORDS; w++)
            {
                img[dst + w] = st->mem[(uint16_t)(srcbase + w)];
            }
        }
        /* diagnostic row: overlay #, window addr, logical sec0, PHYSICAL sec0,
         * first 4 words of the window - the boot test diffs against this.     */
        uint16_t log0  = (uint16_t)((uint32_t)ovdk + (uint32_t)spo * n);
        uint32_t phys0 = cdc_dkadr(log0);
        fprintf(stderr, "  %03o  %06o  %06o  %06o  %06o %06o %06o %06o\n",
                n, window, (unsigned)log0, (unsigned)phys0,
                st->mem[window], st->mem[(uint16_t)(window + 1)],
                st->mem[(uint16_t)(window + 2)], st->mem[(uint16_t)(window + 3)]);
    }
    /* Surface summary: the CORRECTED DKADR (32*floor(L/12)+2*(L mod 12)) packs
     * overlays into a small, dense physical range. Report the true max physical
     * sector and the resulting image size so the boot test and the nd100x CDC
     * device (CDC_DEFAULT_SECTORS) can be sized to cover it.                   */
    fprintf(stderr, "  max phys sector = %06o (%u dec) -> image %u sectors, "
                    "%u bytes\n",
            (unsigned)max_phys, (unsigned)max_phys,
            (unsigned)total_sectors, (unsigned)(total_words * 2u));

    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        free(img);
        fprintf(stderr, "mac_write_cdc_disc: cannot open %s\n", path);
        return false;
    }
    for (uint32_t i = 0; i < total_words; i++)
    {
        put_be16(f, img[i]);   /* big-endian: matches the nd100x disc device */
    }
    fclose(f);
    free(img);

    fprintf(stderr, "  wrote %u sectors (%u words, %u bytes) to %s\n",
            (unsigned)total_sectors, (unsigned)total_words,
            (unsigned)(total_words * 2), path);
    return true;
}

bool mac_read_image(mac_state *st, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return false;
    }
    char magic[6];
    if (fread(magic, 1, 6, f) != 6 || memcmp(magic, "MACIMG", 6) != 0)
    {
        fclose(f);
        return false;
    }
    int bhi = fgetc(f), blo = fgetc(f), chi = fgetc(f), clo = fgetc(f);
    if (clo < 0)
    {
        fclose(f);
        return false;
    }
    uint16_t base = (uint16_t)((bhi << 8) | blo);
    uint16_t count = (uint16_t)((chi << 8) | clo);
    for (uint16_t i = 0; i < count; i++)
    {
        int hi = fgetc(f), lo = fgetc(f);
        if (lo < 0)
        {
            fclose(f);
            return false;
        }
        uint16_t a = (uint16_t)(base + i);
        st->mem[a] = (uint16_t)((hi << 8) | lo);
        st->used[a] = 1;
        if (a < st->lo_used) st->lo_used = a;
        if (a > st->hi_used) st->hi_used = a;
    }
    fclose(f);
    return true;
}

/* Verbatim octal-ASCII loader section recovered from the real MAC.BPUN
 * (file offsets 204..507). It deposits a 36-word paper-tape block loader at
 * octal 164316 and starts it; the loader then reads ONE binary block
 * (load-address, word-count, data..., 16-bit additive checksum) and jumps
 * indirect through the start cell. We reproduce it byte-for-byte so the
 * output tape boots on real ND / nd100x exactly like the archived MAC.     */
static const unsigned char BPUN_OCTAL_LOADER[304] = {
    0x8D,0x0A,0xB1,0x36,0xB4,0x33,0xB1,0x36,0xAF,0xB1,0xB4,0x36,
    0xB1,0x30,0xB1,0x8D,0x0A,0xB1,0x33,0xB4,0x30,0xB2,0xB1,0x8D,
    0x0A,0xB1,0xB4,0x36,0xB1,0x35,0xB7,0x8D,0x0A,0xB1,0x33,0xB4,
    0x30,0xB1,0xB7,0x8D,0x0A,0xB1,0xB4,0x36,0xB1,0x35,0x36,0x8D,
    0x0A,0xB1,0x33,0xB4,0x30,0xB1,0x35,0x8D,0x0A,0x30,0x30,0x36,
    0x30,0x30,0x30,0x8D,0x0A,0xB1,0xB4,0x36,0x30,0x35,0xB1,0x8D,
    0x0A,0xB1,0xB4,0x36,0xB2,0x30,0x36,0x8D,0x0A,0xB1,0xB4,0xB2,
    0x30,0x30,0x36,0x8D,0x0A,0xB1,0xB2,0xB4,0x30,0x30,0x33,0x8D,
    0x0A,0xB1,0xB7,0x33,0xB4,0x30,0xB1,0x8D,0x0A,0xB1,0xB2,0xB4,
    0x33,0xB7,0xB1,0x8D,0x0A,0xB1,0x33,0xB4,0x30,0x30,0x35,0x8D,
    0x0A,0xB1,0xB4,0x36,0x36,0xB1,0x35,0x8D,0x0A,0xB1,0x33,0xB1,
    0x30,0xB2,0x33,0x8D,0x0A,0xB1,0x35,0xB1,0x30,0xB7,0xB7,0x8D,
    0x0A,0xB1,0x36,0xB4,0xB1,0xB7,0x33,0x8D,0x0A,0xB1,0xB4,0x36,
    0xB1,0xB4,0x35,0x8D,0x0A,0x30,0x30,0xB4,0x30,0xB1,0x36,0x8D,
    0x0A,0xB1,0x33,0xB4,0x30,0x30,0x36,0x8D,0x0A,0xB1,0x35,0x35,
    0x35,0xB7,0x30,0x8D,0x0A,0x30,0x30,0xB4,0x33,0xB7,0x33,0x8D,
    0x0A,0xB1,0x33,0xB4,0x30,0x30,0x33,0x8D,0x0A,0x30,0x36,0x30,
    0x33,0xB7,0xB1,0x8D,0x0A,0xB1,0xB2,0x35,0x30,0xB1,0x30,0x8D,
    0x0A,0xB1,0xB7,0x30,0xB4,0x30,0xB4,0x8D,0x0A,0xB1,0x36,0xB4,
    0xB4,0x30,0x33,0x8D,0x0A,0xB1,0x36,0xB4,0xB4,0x30,0xB2,0x8D,
    0x0A,0xB1,0xB7,0x35,0xB2,0x33,0x35,0x8D,0x0A,0xB1,0xB2,0xB4,
    0x33,0xB7,0x36,0x8D,0x0A,0xB1,0x36,0xB4,0xB4,0x30,0x30,0x8D,
    0x0A,0xB1,0xB4,0x36,0xB1,0xB4,0xB2,0x8D,0x0A,0x30,0x30,0x30,
    0x30,0x30,0x30,0x8D,0x0A,0xB1,0xB2,0x35,0x30,0x30,0xB1,0x8D,
    0x0A,0x30,0x30,0x30,0x30,0x30,0x30,0x8D,0x0A,0xB1,0x36,0xB4,
    0x33,0xB1,0x36,0x21,
};

/* Core BPUN writer: emits leader + verbatim octal bootstrap + one binary
 * block covering [lo,hi] + additive checksum + trailer to an open stream.
 * 'entry' is recorded in the trailing start word (0 = no autostart, which
 * is what the archived MAC.BPUN itself carries).                          */
bool mac_write_bpun_range(mac_state *st, FILE *f, uint16_t lo, uint16_t hi,
                          uint16_t entry)
{
    if (f == NULL)
    {
        return false;   /* dummy device */
    }
    for (int i = 0; i < 204; i++)
    {
        fputc(0x00, f);
    }
    /* The reproduced bootstrap ends with its own autostart token "164316!"
     * (the last 7 bytes). When an 'entry' is given we replace THAT token with
     * "<entry>!" so a loader that takes its start address from the octal
     * preamble (e.g. nd100x's BPUN loader: boot = the address before '!' when
     * it differs from the last opened '/' location) starts the loaded image at
     * 'entry'. With entry==0 the tape stays byte-identical to the archived
     * MAC.BPUN. TSS's image (0..033636) does not cover the high autostart cell
     * (164361) the original loader jumps through, so this preamble autostart is
     * how TSS is entered at its cold-start (ISTRT). */
    if (entry != 0)
    {
        /* even parity on each octal digit, exactly as the rest of the loader */
        fwrite(BPUN_OCTAL_LOADER, 1, sizeof(BPUN_OCTAL_LOADER) - 7, f);
        char oct[8];
        snprintf(oct, sizeof(oct), "%o", entry);
        for (const char *p = oct; *p; p++)
        {
            unsigned char v = (unsigned char)(*p) & 0x7F;
            int ones = 0;
            for (int b = 0; b < 7; b++)
                if (v & (1 << b))
                    ones++;
            if (ones & 1)
                v |= 0x80;      /* set bit 7 to make the byte even parity */
            fputc(v, f);
        }
        fputc('!', f);          /* 0x21 is already even parity */
    }
    else
    {
        fwrite(BPUN_OCTAL_LOADER, 1, sizeof(BPUN_OCTAL_LOADER), f);
    }

    uint16_t base = lo;
    uint16_t count = (hi >= lo) ? (uint16_t)(hi - lo + 1) : 0;
    put_be16(f, base);
    put_be16(f, count);
    uint16_t sum = 0;
    for (uint16_t i = 0; i < count; i++)
    {
        uint16_t w = st->mem[(uint16_t)(base + i)];
        put_be16(f, w);
        sum = (uint16_t)(sum + w);
    }
    put_be16(f, sum);          /* 16-bit additive checksum */
    /* NOTE: nothing is written after the checksum, so the tape is
     * byte-compatible with the archived MAC.BPUN. The bootstrap takes its
     * start address from a cell INSIDE the loaded image (the loader ends
     * with "JMP I 164361"), so an autostart can only be expressed when the
     * dumped range covers that cell; 'entry' is honoured in that case and
     * is otherwise informational, exactly as on the archived tape which
     * carries 0 there.                                                    */
    if (entry != 0 && lo <= 0xE8F1 && hi >= 0xE8F1)
    {
        /* the start cell is inside the range: it was already written above
         * as part of the block, so warn rather than silently disagree      */
        if (st->mem[0xE8F1] != entry)
        {
            mac_err(st, ")BPUN start cell does not hold the entry address",
                    NULL);
        }
    }
    for (int i = 0; i < 64; i++)
    {
        fputc(0x00, f);
    }
    return true;
}

bool mac_write_bpun(mac_state *st, const char *path, uint16_t entry)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        return false;
    }
    uint16_t lo = (st->hi_used >= st->lo_used) ? st->lo_used : 0;
    uint16_t hi = (st->hi_used >= st->lo_used) ? st->hi_used : 0;
    bool ok = mac_write_bpun_range(st, f, lo, hi, entry);
    fclose(f);
    return ok;
}

bool mac_read_bpun(mac_state *st, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return false;
    }
    /* Skip the null leader, then the octal-ASCII bootstrap which ends with
     * the '!' start marker; the binary block follows immediately.         */
    int c;
    while ((c = fgetc(f)) == 0x00)
    {
        /* leader */
    }
    if (c == EOF)
    {
        fclose(f);
        return false;
    }
    while (c != '!' && c != EOF)
    {
        c = fgetc(f);
    }
    if (c == EOF)
    {
        fclose(f);
        return false;
    }
    int bh = fgetc(f), bl = fgetc(f), ch = fgetc(f), cl = fgetc(f);
    if (cl == EOF)
    {
        fclose(f);
        return false;
    }
    uint16_t base = (uint16_t)((bh << 8) | bl);
    uint16_t count = (uint16_t)((ch << 8) | cl);
    uint16_t sum = 0;
    for (uint16_t i = 0; i < count; i++)
    {
        int hi = fgetc(f), lo = fgetc(f);
        if (lo == EOF)
        {
            fclose(f);
            return false;
        }
        uint16_t w = (uint16_t)((hi << 8) | lo);
        uint16_t a = (uint16_t)(base + i);
        st->mem[a] = w;
        st->used[a] = 1;
        if (a < st->lo_used) st->lo_used = a;
        if (a > st->hi_used) st->hi_used = a;
        sum = (uint16_t)(sum + w);
    }
    int sh = fgetc(f), sl = fgetc(f);
    if (sl == EOF)
    {
        fclose(f);
        return false;
    }
    uint16_t stored = (uint16_t)((sh << 8) | sl);
    fclose(f);
    return stored == sum;   /* the loader halts in WAIT 77 on a mismatch */
}

