/**************************************************************************
** MAC-C UNIT TESTS - comprehensive validation of the MAC assembler core  **
**                                                                       **
** Oracles used (no invented expectations):                              **
**   - MAC_PERMSYM[]: the 154 permanent symbols byte-verified against    **
**     the real MAC.BPUN binary. Every one is assembled and its emitted  **
**     word compared to the value the binary itself carries.             **
**   - ND-100 addressing modes: the nd100-markdown docs set          **
**     addressing_modes.md (X=bit10=02000, I=bit9=01000, B=bit8=0400).   **
**   - P-relative rule EA=(P)+disp with P = the instruction's OWN        **
**     address: ND-60.096.01 sec 2.3.1, cross-checked against real code  **
**     in MAC.BPUN (JMP *0xdef6 at 0xdf11 encodes disp 0345 = -27).      **
**   - Text strings: ND-60.096.01 sec 3.2.3.7 worked example 'ABCDEF'    **
**     at location 400 occupies 400..403 and leaves the counter at 404.  **
**   - MAC word = opcode + mode + (disp & 0377): ND-60.096.01 sec 2.6    **
**     worked example (043776 + 400 = 044376).                           **
**                                                                       **
** Ronny Hansen                                                          **
***************************************************************************/
#include "mac.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass;
static int g_fail;

/* ---------------------------------------------------------------------- */
/* helpers                                                                 */
/* ---------------------------------------------------------------------- */

static void check_word(const char *what, uint16_t got, uint16_t want)
{
    if (got == want)
    {
        g_pass++;
    }
    else
    {
        g_fail++;
        printf("  FAIL %-44s got %06o want %06o\n", what, got, want);
    }
}

static void check_int(const char *what, long got, long want)
{
    if (got == want)
    {
        g_pass++;
    }
    else
    {
        g_fail++;
        printf("  FAIL %-44s got %ld want %ld\n", what, got, want);
    }
}

static void check_true(const char *what, bool cond)
{
    if (cond)
    {
        g_pass++;
    }
    else
    {
        g_fail++;
        printf("  FAIL %-44s (condition false)\n", what);
    }
}

/* Assemble one statement at a chosen location; return the emitted word.   */
static uint16_t asm1_at(mac_state *st, uint16_t at, const char *text)
{
    st->loc = at;
    mac_line(st, text);
    return st->mem[at];
}

/* Fresh state helper */
static void fresh(mac_state *st)
{
    mac_init(st);
    st->loc = 01000;
}

/* ---------------------------------------------------------------------- */
/* 1. EVERY permanent symbol assembles to the value in the real binary     */
/* ---------------------------------------------------------------------- */
static void test_all_permanent_symbols(void)
{
    printf("[1] all %zu permanent symbols vs MAC.BPUN golden values\n",
           MAC_PERMSYM_COUNT);
    for (size_t i = 0; i < MAC_PERMSYM_COUNT; i++)
    {
        mac_state st;
        fresh(&st);
        const char *name = MAC_PERMSYM[i].name;
        uint16_t want = MAC_PERMSYM[i].value;
        char text[64];

        switch (MAC_PERMSYM[i].cls)
        {
        case MAC_CLS_MRI:
            /* operand 0 is a plain value: word = opcode + 0 + 0 */
            snprintf(text, sizeof(text), "%s 0", name);
            break;
        case MAC_CLS_JUMP8:
            /* '*' = this instruction's address -> displacement 0 */
            snprintf(text, sizeof(text), "%s *", name);
            break;
        case MAC_CLS_ARG8:
            snprintf(text, sizeof(text), "%s 0", name);
            break;
        case MAC_CLS_PLAIN:
        default:
            snprintf(text, sizeof(text), "%s", name);
            break;
        }
        uint16_t got = asm1_at(&st, 01000, text);
        char label[160];
        snprintf(label, sizeof(label), "%s (\"%s\")", name, text);
        check_word(label, got, want);
    }
}

/* ---------------------------------------------------------------------- */
/* 1b. EVERY memory-reference opcode against EVERY addressing mode         */
/* ---------------------------------------------------------------------- */
static void test_every_mri_every_mode(void)
{
    printf("[1b] every MRI opcode x all 8 addressing modes\n");
    /* mode bits from the binary's own table: ,X=02000 I=01000 ,B=0400 */
    static const struct { const char *operand; uint16_t mode; } MODES[] = {
        { "5",          00000 },  /* mode 0, plain displacement          */
        { "5,B",        00400 },  /* mode 1                              */
        { "I 5",        01000 },  /* mode 2                              */
        { "I 5,B",      01400 },  /* mode 3                              */
        { "5,X",        02000 },  /* mode 4                              */
        { "5,B,X",      02400 },  /* mode 5                              */
        { ",X I 5",     03000 },  /* mode 6                              */
        { ",X I 5,B",   03400 },  /* mode 7                              */
    };
    int checked = 0;
    for (size_t i = 0; i < MAC_PERMSYM_COUNT; i++)
    {
        if (MAC_PERMSYM[i].cls != MAC_CLS_MRI)
        {
            continue;
        }
        for (size_t m = 0; m < sizeof(MODES) / sizeof(MODES[0]); m++)
        {
            mac_state st;
            fresh(&st);
            char text[96];
            snprintf(text, sizeof(text), "%s %s",
                     MAC_PERMSYM[i].name, MODES[m].operand);
            uint16_t got = asm1_at(&st, 01000, text);
            uint16_t want = (uint16_t)(MAC_PERMSYM[i].value +
                                       MODES[m].mode + 5);
            char label[160];
            snprintf(label, sizeof(label), "%-4s %-10s",
                     MAC_PERMSYM[i].name, MODES[m].operand);
            check_word(label, got, want);
            checked++;
        }
    }
    check_true("at least 100 MRI/mode combos    ", checked >= 100);
}

/* ---------------------------------------------------------------------- */
/* 1c. every conditional jump and every argument instruction               */
/* ---------------------------------------------------------------------- */
static void test_every_jump_and_arg(void)
{
    printf("[1c] every JUMP8 opcode (fwd/back) and every ARG8 opcode\n");
    for (size_t i = 0; i < MAC_PERMSYM_COUNT; i++)
    {
        char text[96], label[160];
        if (MAC_PERMSYM[i].cls == MAC_CLS_JUMP8)
        {
            /* forward: *+5 -> displacement 5 */
            mac_state st;
            fresh(&st);
            snprintf(text, sizeof(text), "%s *+5", MAC_PERMSYM[i].name);
            snprintf(label, sizeof(label), "%-4s *+5", MAC_PERMSYM[i].name);
            check_word(label, asm1_at(&st, 01000, text),
                       (uint16_t)(MAC_PERMSYM[i].value + 5));
            /* backward: *-3 -> displacement 0375 */
            fresh(&st);
            snprintf(text, sizeof(text), "%s *-3", MAC_PERMSYM[i].name);
            snprintf(label, sizeof(label), "%-4s *-3", MAC_PERMSYM[i].name);
            check_word(label, asm1_at(&st, 01000, text),
                       (uint16_t)(MAC_PERMSYM[i].value + 0375));
        }
        else if (MAC_PERMSYM[i].cls == MAC_CLS_ARG8)
        {
            mac_state st;
            fresh(&st);
            snprintf(text, sizeof(text), "%s 77", MAC_PERMSYM[i].name);
            snprintf(label, sizeof(label), "%-4s 77", MAC_PERMSYM[i].name);
            check_word(label, asm1_at(&st, 01000, text),
                       (uint16_t)(MAC_PERMSYM[i].value + 077));
            /* negative argument is masked to 8 bits */
            fresh(&st);
            snprintf(text, sizeof(text), "%s -1", MAC_PERMSYM[i].name);
            snprintf(label, sizeof(label), "%-4s -1", MAC_PERMSYM[i].name);
            check_word(label, asm1_at(&st, 01000, text),
                       (uint16_t)(MAC_PERMSYM[i].value + 0377));
        }
    }
}

/* ---------------------------------------------------------------------- */
/* 1d. register-class instructions built by summing sub-fields             */
/* ---------------------------------------------------------------------- */
static void test_register_class_forms(void)
{
    printf("[1d] register/IO instructions assembled as sums of sub-fields\n");
    mac_state st;
    fresh(&st);
    /* every combination is the arithmetic sum of the permanent values */
    check_word("COPY SA DT                  ",
               asm1_at(&st, 01000, "COPY SA DT"), 0146100 + 050 + 06);
    check_word("COPY SL DP                  ",
               asm1_at(&st, 01000, "COPY SL DP"), 0146100 + 040 + 02);
    check_word("RADD SX DA                  ",
               asm1_at(&st, 01000, "RADD SX DA"), 0146000 + 070 + 05);
    check_word("RSUB SD DA                  ",
               asm1_at(&st, 01000, "RSUB SD DA"), 0146600 + 010 + 05);
    check_word("SWAP SA DT                  ",
               asm1_at(&st, 01000, "SWAP SA DT"), 0144000 + 050 + 06);
    check_word("RCLR DA                     ",
               asm1_at(&st, 01000, "RCLR DA"), 0146100 + 05);
    check_word("RINC DX                     ",
               asm1_at(&st, 01000, "RINC DX"), 0146400 + 07);
    check_word("RDCR DT                     ",
               asm1_at(&st, 01000, "RDCR DT"), 0146200 + 06);
    check_word("EXIT                        ",
               asm1_at(&st, 01000, "EXIT"), 0146142);
    check_word("SKP IF DA EQL ST            ",
               asm1_at(&st, 01000, "SKP IF DA EQL ST"),
               0140000 + 0 + 05 + 0 + 060);
    check_word("SKP IF DX UEQ 0             ",
               asm1_at(&st, 01000, "SKP IF DX UEQ 0"),
               0140000 + 07 + 02000);
    /* SHR = "shift right, gives negative shift counter" (ND-60.096.01 sec
     * 2.3.8; MAC.BPUN permsym SHR=0200). The count term following SHR is
     * SUBTRACTED, so these are RIGHT shifts. Old (buggy) values summed the
     * count and produced left shifts - corrected below.                    */
    check_word("SHA SHR 6                   ",
               asm1_at(&st, 01000, "SHA SHR 6"),
               0154400 + 0200 - 6);          /* = 0154572 */
    check_word("SHA SHR 3                   ",
               asm1_at(&st, 01000, "SHA SHR 3"),
               0154400 + 0200 - 3);          /* = 0154575 */
    check_word("SHA ZIN SHR 1               ",
               asm1_at(&st, 01000, "SHA ZIN SHR 1"),
               0154400 + 02000 + 0200 - 1);  /* = 0156577 */
    check_word("SHA ROT SHR 10              ",
               asm1_at(&st, 01000, "SHA ROT SHR 10"),
               0154400 + 01000 + 0200 - 010);/* = 0155170 */
    check_word("SAD ZIN SHR 20              ",
               asm1_at(&st, 01000, "SAD ZIN SHR 20"),
               0154600 + 02000 + 0200 - 020);/* = 0156760 (DKADR divide setup) */
    check_word("BSET ZRO 70 DA              ",
               asm1_at(&st, 01000, "BSET ZRO 70 DA"),
               0174000 + 0 + 070 + 05);
    check_word("BSKP ONE 30 DA              ",
               asm1_at(&st, 01000, "BSKP ONE 30 DA"),
               0175000 + 0200 + 030 + 05);
    check_word("IOX 302                     ",
               asm1_at(&st, 01000, "IOX 302"), 0164000 + 0302);
    check_word("MON 2                       ",
               asm1_at(&st, 01000, "MON 2"), 0153000 + 2);
    check_word("TRA PVL                     ",
               asm1_at(&st, 01000, "TRA PVL"), 0150000 + 04);
    check_word("TRR PIE                     ",
               asm1_at(&st, 01000, "TRR PIE"), 0150100 + 07);
    check_word("MST PID                     ",
               asm1_at(&st, 01000, "MST PID"), 0150300 + 06);
    check_word("MCL PIE                     ",
               asm1_at(&st, 01000, "MCL PIE"), 0150200 + 07);
    check_word("WAIT 37                     ",
               asm1_at(&st, 01000, "WAIT 37"), 0151000 + 037);
    check_word("IDENT PL11                  ",
               asm1_at(&st, 01000, "IDENT PL11"), 0143600 + 011);
    check_word("IRW 10 DP                   ",
               asm1_at(&st, 01000, "IRW 10 DP"), 0153400 + 010 + 02);
    check_word("IRR 10 DP                   ",
               asm1_at(&st, 01000, "IRR 10 DP"), 0153600 + 010 + 02);
    check_word("RMPY ST DA                  ",
               asm1_at(&st, 01000, "RMPY ST DA"), 0141200 + 060 + 05);
    check_word("RDIV ST DA                  ",
               asm1_at(&st, 01000, "RDIV ST DA"), 0141600 + 060 + 05);
    check_word("EXR ST                      ",
               asm1_at(&st, 01000, "EXR ST"), 0140600 + 060);
    check_word("SBYT / LBYT                 ",
               asm1_at(&st, 01000, "SBYT"), 0142600);
    check_word("IOF / ION / POF / PON       ",
               asm1_at(&st, 01000, "IOF"), 0150401);
    check_word("NLZ 20                      ",
               asm1_at(&st, 01000, "NLZ 20"), 0151400 + 020);
    /* DNZ's scaling is an 8-bit FIELD, so a negative one is masked into
     * it - it does not subtract from the whole word. This assertion used
     * to read (0152000 - 020) = 0151760, which is NLZ+0360, not DNZ at
     * all; it locked the bug in rather than catching it. See test [18].  */
    check_word("DNZ -20 (masked)            ",
               asm1_at(&st, 01000, "DNZ -20"),
               (uint16_t)(0152000 + ((-020) & 0377)));
}

/* ---------------------------------------------------------------------- */
/* 2. all eight addressing modes                                           */
/* ---------------------------------------------------------------------- */
static void test_addressing_modes(void)
{
    printf("[2] addressing modes (addressing_modes.md bit assignments)\n");
    mac_state st;
    fresh(&st);

    /* LDA = 044000. Mode bits: X=02000 (bit10), I=01000 (bit9), B=0400 (b8) */
    check_word("mode 0 P-rel  'LDA *'      ",
               asm1_at(&st, 01000, "LDA *"), 044000);          /* disp 0    */
    check_word("mode 1 B-rel  'LDA 5,B'    ",
               asm1_at(&st, 01000, "LDA 5,B"), 044405);
    check_word("mode 2 I P    'LDA I *'    ",
               asm1_at(&st, 01000, "LDA I *"), 045000);
    check_word("mode 3 I B    'LDA I 3,B'  ",
               asm1_at(&st, 01000, "LDA I 3,B"), 045403);
    check_word("mode 4 X-rel  'LDA 0,X'    ",
               asm1_at(&st, 01000, "LDA 0,X"), 046000);
    check_word("mode 5 B+X    'LDA 17,B,X' ",
               asm1_at(&st, 01000, "LDA 17,B,X"), 046417);
    check_word("mode 6 ,X I P 'LDA ,X I *' ",
               asm1_at(&st, 01000, "LDA ,X I *"), 047000);
    check_word("mode 7 ,X I B 'LDA ,X I 4,B'",
               asm1_at(&st, 01000, "LDA ,X I 4,B"), 047404);

    /* negative displacement must be masked to 8 bits BEFORE the mode bit is
     * added, else the sign extension eats the B bit (manual sec 2.6).      */
    check_word("negative disp 'LDA -4,B'   ",
               asm1_at(&st, 01000, "LDA -4,B"), 044774);
}

/* ---------------------------------------------------------------------- */
/* 3. P-relative displacement arithmetic                                   */
/* ---------------------------------------------------------------------- */
static void test_p_relative(void)
{
    printf("[3] P-relative displacement (EA = P + disp, P = instr address)\n");
    mac_state st;

    /* forward: JMP to *+2 */
    fresh(&st);
    check_word("JMP *+2                     ",
               asm1_at(&st, 01000, "JMP *+2"), 0124002);

    /* backward: the real binary has JMP *0xdef6 at 0xdf11 -> disp 0345     */
    fresh(&st);
    st.loc = 0xdf11;
    mac_line(&st, "JMP *-33");   /* 0xdf11 - 0xdef6 = 27 dec = 33 octal    */
    check_word("JMP backward (MAC.BPUN case)", st.mem[0xdf11], 0124345);

    /* label-based forward reference, patched by the fixup chain            */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "JMP TGT");
    mac_line(&st, "STZ 0");
    mac_line(&st, "TGT, STZ 0");
    /* TGT is at 01002; instruction at 01000 -> disp = 2 */
    check_word("forward label JMP TGT       ", st.mem[01000], 0124002);

    /* backward label */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "BACK, STZ 0");
    mac_line(&st, "JMP BACK");
    /* instruction at 01001, target 01000 -> disp = -1 = 0377 */
    check_word("backward label JMP BACK     ", st.mem[01001], 0124377);
}

/* ---------------------------------------------------------------------- */
/* 4. statement forms                                                      */
/* ---------------------------------------------------------------------- */
static void test_statement_forms(void)
{
    printf("[4] statement forms: labels, '=', '/', ';', comments, BSS\n");
    mac_state st;
    fresh(&st);

    /* location counter set */
    mac_line(&st, "2000/");
    check_int("location set '2000/'         ", st.loc, 02000);

    /* symbol assignment, octal default */
    mac_line(&st, "FOO=377");
    mac_line(&st, "FOO");
    check_word("assignment FOO=377          ", st.mem[02000], 0377);

    /* decimal with trailing dot */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "DEC=10.");
    mac_line(&st, "DEC");
    check_word("decimal literal 10.         ", st.mem[02000], 10);

    /* label defines current location as an address */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "HERE, STZ 0");
    mac_line(&st, "HERE");
    check_word("label value = its address   ", st.mem[03001], 03000);

    /* multiple statements on one line separated by ';' */
    fresh(&st);
    st.loc = 04000;
    mac_line(&st, "SAA 1; SAA 2; SAA 3");
    check_word("';' stmt 1                  ", st.mem[04000], 0170401);
    check_word("';' stmt 2                  ", st.mem[04001], 0170402);
    check_word("';' stmt 3                  ", st.mem[04002], 0170403);

    /* '%' comment is stripped */
    fresh(&st);
    st.loc = 05000;
    mac_line(&st, "SAA 7 % this is a comment");
    check_word("'%' comment stripped        ", st.mem[05000], 0170407);

    /* BSS is NOT a MAC builtin: TSS defines it as a macro that advances the
     * location counter ()MCDEF BSS $A1 / "*+$A1 /"). Verify that shape.    */
    fresh(&st);
    st.loc = 06000;
    mac_line(&st, ")MCDEF BSS $A1");
    mac_line(&st, "*+$A1 /");
    mac_line(&st, "]");
    mac_line(&st, "BUF, BSS 4");
    check_int("TSS BSS macro advances loc  ", st.loc, 06004);
    check_word("TSS BSS label value         ",
               sym_lookup_value(&st, "BUF"), 06000);

    /* digit-leading symbols (TSS uses 8LP, 9TTI, ...) */
    fresh(&st);
    st.loc = 07000;
    mac_line(&st, "9TTI=40");
    mac_line(&st, "8LP=200");
    mac_line(&st, "9TTI");
    mac_line(&st, "8LP");
    check_word("digit-leading symbol 9TTI   ", st.mem[07000], 040);
    check_word("digit-leading symbol 8LP    ", st.mem[07001], 0200);
}

/* ---------------------------------------------------------------------- */
/* 5. text strings (ND-60.096.01 sec 3.2.3.7 worked example)               */
/* ---------------------------------------------------------------------- */
static void test_strings(void)
{
    printf("[5] text strings, manual worked example 'ABCDEF' at 400\n");
    mac_state st;
    fresh(&st);
    st.loc = 0400;
    mac_line(&st, "'ABCDEF'");
    /* two chars per word, first char in the high byte; the terminating
     * quote is part of the string, so 7 chars -> 4 words, 400..403         */
    check_word("word 400 = 'AB'             ", st.mem[0400], 0x4142);
    check_word("word 401 = 'CD'             ", st.mem[0401], 0x4344);
    check_word("word 402 = 'EF'             ", st.mem[0402], 0x4546);
    check_word("word 403 = closing quote    ", st.mem[0403], 0x2700);
    check_int("location counter -> 404     ", st.loc, 0404);
}

/* ---------------------------------------------------------------------- */
/* 6. literals and )FILL                                                   */
/* ---------------------------------------------------------------------- */
static void test_literals_and_fill(void)
{
    printf("[6] literals '(EXPR' and the )FILL pool dump\n");
    mac_state st;
    fresh(&st);
    st.loc = 01000;

    mac_line(&st, "LDA (400");     /* 01000: addresses a literal cell      */
    mac_line(&st, "STZ 0");        /* 01001: filler                        */
    mac_line(&st, ")FILL");        /* literal cell lands at 01002          */

    check_word("literal cell holds 400      ", st.mem[01002], 0400);
    /* LDA at 01000 addresses 01002 P-relative -> disp 2 */
    check_word("LDA (400 -> P-rel disp 2    ", st.mem[01000], 044002);

    /* identical literals share one cell */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "LDA (7");
    mac_line(&st, "LDT (7");
    mac_line(&st, ")FILL");
    check_word("shared literal cell value   ", st.mem[02002], 7);
    check_word("first ref  disp 2           ", st.mem[02000], 044002);
    check_word("second ref disp 1           ", st.mem[02001], 050001);

    /* indirect through a literal: JPL I (SYM - the TSS calling idiom */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "TGT=12345");
    mac_line(&st, "JPL I (TGT");
    mac_line(&st, ")FILL");
    check_word("literal holds symbol value  ", st.mem[03001], 012345);
    /* JPL=0134000 + I(01000) + disp 1 */
    check_word("JPL I (TGT encoding         ", st.mem[03000], 0135001);
}

/* ---------------------------------------------------------------------- */
/* 7. forward references / fixups                                          */
/* ---------------------------------------------------------------------- */
static void test_forward_references(void)
{
    printf("[7] forward references and fixup patching\n");
    mac_state st;

    /* full-word forward reference in a data word */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "LATER");        /* data word, symbol not yet defined    */
    mac_line(&st, "LATER=7777");
    check_word("data word forward ref       ", st.mem[01000], 07777);

    /* ARG8 forward reference (SAA of a later symbol) */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "SAA VAL");
    mac_line(&st, "VAL=125");
    check_word("SAA forward ref             ", st.mem[02000], 0170525);

    /* MRI forward reference is P-relative */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "LDA FWD");
    mac_line(&st, "STZ 0");
    mac_line(&st, "FWD, STZ 0");
    check_word("LDA forward label P-rel     ", st.mem[03000], 044002);

    /* REGRESSION (mac.c tokenizer): a digit-leading, ALL-digit SYMBOL whose
     * digits are not valid octal (contain 8 or 9) must resolve as a SYMBOL,
     * not a number. TSS defines pointer literals like `9377, 377` (the 377
     * byte-mask used by WBUF/RBUF). A tokenizer that classified `9377` as a
     * number via isdigit() then ran strtol("9377",8), which stops at the
     * non-octal '9' and returns 0 -- silently miscompiling `AND 9377` to
     * `AND 0` (AND with the instruction word itself), zeroing every console
     * character on the way into the type-ahead ring and hanging LOGON. */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "AND 9377");      /* forward ref to a digit-named symbol   */
    mac_line(&st, "9377, 377");     /* 9377 defined at 01001, holds data 377 */
    /* AND (070000) P-relative to 01001 from 01000 -> disp 1 -> 070001, NOT 0 */
    check_word("AND 9377 resolves to symbol ", st.mem[01000], 070001);
    check_word("9377 data word is 377       ", st.mem[01001], 000377);

    /* backward reference to the same digit-named symbol also resolves */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "9377, 377");     /* 9377 at 01000                         */
    mac_line(&st, "AND 9377");      /* at 01001, disp = 01000-01001 = -1     */
    check_word("AND 9377 backward ref       ", st.mem[01001], 070377);

    /* a genuine octal number is STILL a number (not a symbol) */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "377");           /* bare data word = octal 377            */
    check_word("octal 377 still numeric     ", st.mem[01000], 000377);

    /* a decimal literal (trailing '.') is STILL decimal */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "95.");           /* bare data word = decimal 95 = 0137    */
    check_word("decimal 95. still numeric   ", st.mem[01000], 0000137);
}

/* ---------------------------------------------------------------------- */
/* 8. library marks (conditional assembly)                                 */
/* ---------------------------------------------------------------------- */
static void test_library_marks(void)
{
    printf("[8] library marks: \"MARK conditional regions\n");
    mac_state st;

    /* a mark is TRUE when the symbol is referenced but never defined */
    fresh(&st);
    mac_reference_mark(&st, "CDC");
    st.loc = 01000;
    mac_line(&st, "\"CDC");
    mac_line(&st, "SAA 1");
    mac_line(&st, "\"");
    check_word("true mark assembles body    ", st.mem[01000], 0170401);

    /* an undefined-and-unreferenced mark is false -> body skipped */
    fresh(&st);
    st.loc = 02000;
    st.mem[02000] = 0;
    mac_line(&st, "\"NCR");
    mac_line(&st, "SAA 1");
    mac_line(&st, "\"");
    check_word("false mark skips body       ", st.mem[02000], 0);
    check_int("false mark leaves loc alone ", st.loc, 02000);

    /* negated mark: '-MARK' is true when the mark is absent */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "\"-NOTSET");
    mac_line(&st, "SAA 2");
    mac_line(&st, "\"");
    check_word("negated mark '-NOTSET'      ", st.mem[03000], 0170402);

    /* a defined symbol is NOT a library mark */
    fresh(&st);
    st.loc = 04000;
    st.mem[04000] = 0;
    mac_define(&st, "DEFINED", 1);
    mac_line(&st, "\"DEFINED");
    mac_line(&st, "SAA 1");
    mac_line(&st, "\"");
    check_word("defined symbol is not a mark", st.mem[04000], 0);
}

/* ---------------------------------------------------------------------- */
/* 9. commands: )KILL, )PCL, )LINE, )LIST                                  */
/* ---------------------------------------------------------------------- */
static void test_commands(void)
{
    printf("[9] commands )KILL, )PCL, )LINE, )LIST\n");
    mac_state st;

    /* )KILL removes a symbol so it can be redefined */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "A1=5");
    mac_line(&st, ")KILL A1");
    mac_line(&st, "A1=6");
    mac_line(&st, "A1");
    check_word(")KILL then redefine         ", st.mem[01000], 6);

    /* )PCL expunges every symbol defined after the named one */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "S1=1");
    mac_line(&st, "S2=2");
    mac_line(&st, "S3=3");
    mac_line(&st, ")PCL S2");
    check_true(")PCL keeps anchor S2        ",
               sym_lookup_value(&st, "S2") == 2);
    check_true(")PCL keeps earlier S1       ",
               sym_lookup_value(&st, "S1") == 1);
    check_true(")PCL removes later S3       ",
               !sym_is_defined(&st, "S3"));

    /* )LINE stops assembly of the stream */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, ")LINE");
    check_true(")LINE sets end-of-stream    ", st.end_of_file_seen);

    /* )LIST output format matches the archived ASYMB:SYMB layout */
    fresh(&st);
    mac_line(&st, "KLOK=16");
    FILE *tmp = tmpfile();
    check_true(")LIST tmpfile created       ", tmp != NULL);
    if (tmp != NULL)
    {
        mac_list_symbols(&st, tmp);
        rewind(tmp);
        char line[64];
        if (fgets(line, sizeof(line), tmp) != NULL)
        {
            /* archived format: 5-char right-justified name '=' 6-digit octal */
            check_true(")LIST format ' KLOK=000016'",
                       strcmp(line, " KLOK=000016\n") == 0);
            if (strcmp(line, " KLOK=000016\n") != 0)
            {
                printf("       got: [%s]\n", line);
            }
        }
        else
        {
            check_true(")LIST produced a line      ", false);
        }
        fclose(tmp);
    }
}

/* ---------------------------------------------------------------------- */
/* 9b. macros ()MCDEF), manual sec 4.2.9 worked examples                   */
/* ---------------------------------------------------------------------- */
static void test_macros(void)
{
    printf("[9b] macros )MCDEF: definition, body, ']', parameters\n");
    mac_state st;

    /* manual example: parameterless macro MOVEAB */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "A=100");
    mac_line(&st, "B=101");
    mac_line(&st, ")MCDEF MOVEAB");
    mac_line(&st, "LDA A");
    mac_line(&st, "STA B");
    mac_line(&st, "]");
    check_int("body not assembled at defn  ", st.loc, 01000);
    mac_line(&st, "MOVEAB");
    check_int("call expands to two words   ", st.loc, 01002);
    /* A=100 is a plain value (not an address) -> disp is the value itself  */
    check_word("expanded LDA A              ", st.mem[01000], 044100);
    check_word("expanded STA B              ", st.mem[01001], 004101);

    /* manual example: parameterised macro MOVE $X, $Y */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "ABCD=55");
    mac_line(&st, "EF=66");
    mac_line(&st, ")MCDEF MOVE $X, $Y");
    mac_line(&st, "LDA $X ");
    mac_line(&st, "STA $Y ");
    mac_line(&st, "]");
    mac_line(&st, "MOVE ABCD, EF");
    check_word("MOVE $X -> LDA ABCD         ", st.mem[02000], 044055);
    check_word("MOVE $Y -> STA EF           ", st.mem[02001], 004066);

    /* manual example: data-only macro DATA1 */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, ")MCDEF DATA1");
    mac_line(&st, "123456");
    mac_line(&st, "0");
    mac_line(&st, "123456");
    mac_line(&st, "]");
    mac_line(&st, "FOO, 1");
    mac_line(&st, "DATA1");
    mac_line(&st, "2");
    check_word("DATA1 word 1                ", st.mem[03001], 0123456);
    check_word("DATA1 word 2                ", st.mem[03002], 0);
    check_word("DATA1 word 3                ", st.mem[03003], 0123456);
    check_word("word after macro            ", st.mem[03004], 2);

    /* a macro body may itself contain commands: they must run only on call */
    fresh(&st);
    st.loc = 04000;
    mac_line(&st, ")MCDEF SETLOC $A1");
    mac_line(&st, "$A1 =*");
    mac_line(&st, "]");
    check_true("cmd in body inert at defn   ", !sym_is_defined(&st, "MARK1"));
    mac_line(&st, "SETLOC MARK1");
    check_true("cmd in body runs on call    ", sym_is_defined(&st, "MARK1"));
    check_word("macro-defined symbol value  ",
               sym_lookup_value(&st, "MARK1"), 04000);
}

/* ---------------------------------------------------------------------- */
/* 9c. constructs discovered by reconciling against the golden ASYMB dump  */
/* ---------------------------------------------------------------------- */
static void test_corpus_constructs(void)
{
    printf("[9c] corpus constructs: ##c, [float, (data, 5-char rule, marks\n");
    mac_state st;

    /* ---- '##c' character constants (manual sec 4.2: "SAA ##?") ------- */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAA ##0");
    check_word("##0 -> SAA with ASCII '0'   ", st.mem[01000],
               (uint16_t)(0170400 + '0'));
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAT ##/");
    check_word("##/ not read as location set", st.mem[01000],
               (uint16_t)(0171000 + '/'));
    check_int("##/ leaves counter intact   ", st.loc, 01001);

    /* an apostrophe inside '##' must not start a string, or the ';'
     * separators would be swallowed (TSS2 OPIII does exactly this)       */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "AAA -##'; JAZ *; AAA ##'");
    check_int("##' keeps ';' splitting     ", st.loc, 02003);

    /* '#ab' is a character PAIR: the word is (a << 8) | b. TSS2:843 uses
     * it as data ("SINQ, #SY; #ST; #EM"); the familiar "##c" spelling is
     * the same rule with a='#', which is why an ARG8 instruction, keeping
     * only the low byte, yields the character c.                         */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SINQ, #SY; #ST; #EM");
    check_word("#SY packs two characters    ", st.mem[01000],
               (uint16_t)(('S' << 8) | 'Y'));
    check_word("#ST packs two characters    ", st.mem[01001],
               (uint16_t)(('S' << 8) | 'T'));
    check_word("#EM packs two characters    ", st.mem[01002],
               (uint16_t)(('E' << 8) | 'M'));
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "##?");
    check_word("##? is the pair ('#','?')   ", st.mem[01000],
               (uint16_t)(('#' << 8) | '?'));

    /* the second character may be a blank, a ';' or a '<' - none of which
     * may be treated as a separator, terminator or interval operator     */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAT ## ; SAA 1");
    check_word("'## ' (blank) then ';' split", st.mem[01000],
               (uint16_t)(0171000 + ' '));
    check_word("statement after '## ;'      ", st.mem[01001], 0170401);
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAA ##<; SAA 1");
    check_word("'##<' not an interval cmd   ", st.mem[01000],
               (uint16_t)(0170400 + '<'));
    check_int("...and interval untouched   ", st.ihigh, 0);
    /* a trailing '#a' at end of line keeps its blank second character */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAT ## ");
    check_word("trailing '## ' keeps blank  ", st.mem[01000],
               (uint16_t)(0171000 + ' '));

    /* ---- '*N' is shorthand for '*+N' -------------------------------- */
    fresh(&st);
    check_word("JMP *3 -> displacement 3    ",
               asm1_at(&st, 01000, "JMP *3"), 0124003);
    check_word("JMP *2 -> displacement 2    ",
               asm1_at(&st, 01000, "JMP *2"), 0124002);
    check_word("'*' alone is displacement 0 ",
               asm1_at(&st, 01000, "JMP *"), 0124000);

    /* ---- '[' floating point constants -------------------------------- */
    /* default = the optional 32-bit format: TWO words (Appendix E) */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "[50");
    check_int("32-bit float = 2 words      ", st.loc, 03002);
    /* 50 = 0.78125 * 2^6 -> exponent 6, biased to 0406. The mantissa is
     * 0.78125 * 2^23 = 0x640000; dropping the implicit leading 1 leaves
     * 0x240000, whose top six bits (044) sit in word0 bits 5..0, and whose
     * low 16 bits are zero. Decoding 040644/000000 gives back exactly 50.  */
    check_word("32-bit float exponent word  ", st.mem[03000],
               (uint16_t)(((0400 + 6) << 6) | 044));
    check_word("32-bit float mantissa word  ", st.mem[03001], 0);
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "[0");
    check_word("float zero word 0           ", st.mem[03000], 0);
    check_word("float zero word 1           ", st.mem[03001], 0);

    /* 48-bit variant: THREE words (manual sec 2.1.4) */
    fresh(&st);
    st.float48 = true;
    st.loc = 03000;
    mac_line(&st, "[50");
    check_int("48-bit float = 3 words      ", st.loc, 03003);
    check_word("48-bit float exponent word  ", st.mem[03000],
               (uint16_t)(040000 + 6));

    /* ---- literal used as a DATA word: "(0" --------------------------- */
    fresh(&st);
    st.loc = 04000;
    mac_line(&st, "TBL, (0; 7");
    mac_line(&st, ")FILL");
    /* the data word holds the ADDRESS of the literal cell placed by )FILL */
    check_word("data literal cell value     ", st.mem[04002], 0);
    check_word("data literal word = cell addr", st.mem[04000], 04002);
    check_word("word after data literal     ", st.mem[04001], 7);

    /* ---- 5-character significance keeps the LAST five ---------------- */
    fresh(&st);
    mac_line(&st, "TIMOUT=1234");
    check_true("6-char name keeps last five ", sym_is_defined(&st, "IMOUT"));
    check_word("...with the assigned value  ",
               sym_lookup_value(&st, "IMOUT"), 01234);
    check_true("first-five form NOT defined ", !sym_is_defined(&st, "TIMOU"));
    /* consequence: LBYTE / LBYTE1 / LBYTE2 stay distinct */
    fresh(&st);
    mac_line(&st, "LBYTE=1");
    mac_line(&st, "LBYTE1=2");
    mac_line(&st, "LBYTE2=3");
    check_word("LBYTE distinct              ",
               sym_lookup_value(&st, "LBYTE"), 1);
    check_word("BYTE1 distinct              ",
               sym_lookup_value(&st, "BYTE1"), 2);
    check_word("BYTE2 distinct              ",
               sym_lookup_value(&st, "BYTE2"), 3);

    /* ---- a label may not be silently redefined ----------------------- */
    fresh(&st);
    st.loc = 05000;
    mac_line(&st, "DUP, STZ 0");
    st.loc = 06000;
    mac_line(&st, "DUP, STZ 0");
    check_word("ALREADY DEFINED keeps first ",
               sym_lookup_value(&st, "DUP"), 05000);

    /* ---- '-' negates a mark term, with or without a space ------------ */
    fresh(&st);
    mac_reference_mark(&st, "CDC");
    st.loc = 07000;
    mac_line(&st, "\"CDC-DRUM");   /* CDC AND NOT DRUM -> true */
    mac_line(&st, "SAA 1");
    mac_line(&st, "\"");
    check_word("\"CDC-DRUM (unspaced NOT)    ", st.mem[07000], 0170401);

    fresh(&st);
    mac_reference_mark(&st, "CDC");
    mac_reference_mark(&st, "DRUM");
    st.loc = 07000;
    st.mem[07000] = 0;
    mac_line(&st, "\"CDC-DRUM");   /* DRUM now set -> false */
    mac_line(&st, "SAA 1");
    mac_line(&st, "\"");
    check_word("\"CDC-DRUM false when DRUM   ", st.mem[07000], 0);

    /* ---- location set followed by the rest of the statement ---------- */
    /* TSS1: "DEVTB+NTTY+1/OEV,TTY1O;TTY2O" - sets the counter, then a
     * label and data words continue on the same line.                    */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "BASE=2000");
    mac_line(&st, "BASE+3/LAB,7;10");
    check_word("locset then label: LAB addr ",
               sym_lookup_value(&st, "LAB"), 02003);
    check_word("locset then data word 1     ", st.mem[02003], 7);
    check_word("locset then data word 2     ", st.mem[02004], 010);

    /* ---- an unrecognised expression line still emits a word ---------- */
    /* '&' is not a MAC operator, so "&L" assembles as an expression.     */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "&L");
    check_int("'&L' emits one word         ", st.loc, 01001);

    /* ---- a macro name with trailing junk is NOT a macro call --------- */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, ")MCDEF PROGM");
    mac_line(&st, "SAA 1");
    mac_line(&st, "SAA 2");
    mac_line(&st, "]");
    mac_line(&st, "PROGM");
    check_int("PROGM expands (2 words)     ", st.loc, 01002);
    st.loc = 01000;
    mac_line(&st, "PROGM&M");
    check_int("PROGM&M is an expression    ", st.loc, 01001);
}

/* ---------------------------------------------------------------------- */
/* 9d. the remaining commands: '<', )ZERO, )CHANGE, )9SET, )WRITE, )WRTM,  */
/*     )NWRT, )WRUS, )WLOC, )WMNE, )PRINT, )9ASCI, )CORE, )9MSG, )CLEAR,   */
/*     )9LITR, )9PARI, )SETSM, )RESSM, )9EXIT, )9TSS                       */
/* ---------------------------------------------------------------------- */

/* capture whatever a command writes to the list/object stream */
static void capture_begin(mac_state *st, FILE **f)
{
    *f = tmpfile();
    st->listing = *f;
    st->object = *f;
}
static void capture_text(FILE *f, char *buf, size_t cap)
{
    fflush(f);
    rewind(f);
    size_t n = fread(buf, 1, cap - 1, f);
    buf[n] = '\0';
}

static void test_remaining_commands(void)
{
    printf("[9d] remaining commands and their stream routing\n");
    mac_state st;
    char buf[4096];
    FILE *cap;

    /* ---- '<' interval + )ZERO --------------------------------------- */
    fresh(&st);
    st.loc = 0300;
    mac_line(&st, "SAA 1; SAA 2; SAA 3");
    mac_line(&st, "300 < 302");
    check_int("'<' sets interval low       ", st.ilow, 0300);
    check_int("'<' sets interval high      ", st.ihigh, 0302);
    mac_line(&st, ")ZERO");
    check_word(")ZERO clears interval w0    ", st.mem[0300], 0);
    check_word(")ZERO clears interval w2    ", st.mem[0302], 0);

    /* )ZERO with a fill symbol */
    fresh(&st);
    mac_line(&st, "FILLV=252");
    mac_line(&st, "400 < 401");
    mac_line(&st, ")ZERO FILLV");
    check_word(")ZERO SYM fills with value  ", st.mem[0400], 0252);
    check_word(")ZERO SYM fills whole range ", st.mem[0401], 0252);

    /* '<' with an undefined expression is ignored */
    fresh(&st);
    mac_line(&st, "500 < 600");
    mac_line(&st, "700 < NOSUCHSYM");
    check_int("'<' ignored if undefined    ", st.ihigh, 0600);

    /* ---- )CHANGE ---------------------------------------------------- */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "177; 177; 200");
    mac_line(&st, "2000 < 2002");
    st.chg_old = 0177;
    st.chg_new = 0;
    st.chg_mask = 0777;
    mac_line(&st, ")CHANGE");
    check_word(")CHANGE matched word 1      ", st.mem[02000], 0);
    check_word(")CHANGE matched word 2      ", st.mem[02001], 0);
    check_word(")CHANGE leaves non-match    ", st.mem[02002], 0200);

    /* ---- )9SET: the B and X location counters ----------------------- */
    /* With the B counter at G, "LDA A1,B" must encode (A1 - G) as the
     * displacement (ND-60.096.01 sec 3.2.4 worked example).             */
    fresh(&st);
    mac_line(&st, "G=1000");
    mac_line(&st, "A1=1005");
    mac_line(&st, ")9SET ,B G");
    check_int(")9SET ,B sets B counter     ", st.bcounter, 01000);
    /* A1 is a plain value, not an address, so it is still a literal disp */
    check_word("non-address ,B unaffected   ",
               asm1_at(&st, 03000, "LDA 5,B"), 044405);
    /* an ADDRESS operand becomes relative to the B counter */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "G, 0");
    st.loc = 01005;
    mac_line(&st, "A1, 0");
    mac_line(&st, ")9SET ,B G");
    check_word("address ,B relative to Bctr ",
               asm1_at(&st, 03000, "LDA A1,B"), 044405);
    /* ,X counter */
    fresh(&st);
    mac_line(&st, "XB=2000");
    mac_line(&st, ")9SET ,X XB");
    check_int(")9SET ,X sets X counter     ", st.xcounter, 02000);
    /* )9SET ,B ZRO zeroes it (ZRO is a permanent symbol = 0) */
    mac_line(&st, ")9SET ,X ZRO");
    check_int(")9SET ,X ZRO zeroes counter ", st.xcounter, 0);
    /* )CLEAR zeroes both */
    fresh(&st);
    mac_line(&st, "Q=777");
    mac_line(&st, ")9SET ,B Q");
    mac_line(&st, ")CLEAR");
    check_int(")CLEAR zeroes B counter     ", st.bcounter, 0);

    /* ---- )WRTM / )NWRT / )WRITE ------------------------------------- */
    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, "AAA1=17");
    mac_line(&st, ")WRITE AAA1");          /* non-write mode: ignored */
    capture_text(cap, buf, sizeof(buf));
    check_true(")WRITE ignored in non-write ", buf[0] == '\0');
    fclose(cap);

    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, "AAA1=17");
    mac_line(&st, ")WRTM");
    mac_line(&st, ")WRITE AAA1 NOPE");
    capture_text(cap, buf, sizeof(buf));
    check_true(")WRTM enables )WRITE        ", strstr(buf, "017") != NULL);
    check_true(")WRITE marks undefined /NF  ", strstr(buf, "/NF") != NULL);
    fclose(cap);

    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, "AAA1=17");
    mac_line(&st, ")WRTM");
    mac_line(&st, ")NWRT");
    mac_line(&st, ")WRITE AAA1");
    capture_text(cap, buf, sizeof(buf));
    check_true(")NWRT disables )WRITE again ", buf[0] == '\0');
    fclose(cap);

    /* ---- )WRUS ------------------------------------------------------ */
    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, "LDA MISSINGSYM");
    mac_line(&st, ")WRUS");
    capture_text(cap, buf, sizeof(buf));
    check_true(")WRUS lists undefined syms  ",
               strstr(buf, "NGSYM") != NULL);   /* MISSINGSYM -> last five */
    fclose(cap);

    /* ---- )WLOC and )WMNE -------------------------------------------- */
    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, "MYSYM=1");
    mac_line(&st, ")WLOC");
    capture_text(cap, buf, sizeof(buf));
    check_true(")WLOC lists user symbols    ", strstr(buf, "MYSYM") != NULL);
    fclose(cap);

    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, ")WMNE");
    capture_text(cap, buf, sizeof(buf));
    check_true(")WMNE lists built-in opcodes", strstr(buf, "LDA") != NULL);
    check_true(")WMNE excludes user symbols ", strstr(buf, "MYSYM") == NULL);
    fclose(cap);

    /* ---- )PRINT and )9ASCI ------------------------------------------ */
    fresh(&st);
    capture_begin(&st, &cap);
    st.loc = 01000;
    mac_line(&st, "'HI'");
    mac_line(&st, "1000 < 1000");
    mac_line(&st, ")PRINT");
    capture_text(cap, buf, sizeof(buf));
    check_true(")PRINT dumps octal words    ", strstr(buf, "001000/") != NULL);
    fclose(cap);

    /* )PUNCH produces the same dump on the object stream */
    fresh(&st);
    capture_begin(&st, &cap);
    st.loc = 01000;
    mac_line(&st, "SAA 1");
    mac_line(&st, "1000 < 1000");
    mac_line(&st, ")PUNCH");
    capture_text(cap, buf, sizeof(buf));
    check_true(")PUNCH dumps octal words    ", strstr(buf, "001000/") != NULL);
    check_true(")PUNCH shows the word value ", strstr(buf, "170401") != NULL);
    fclose(cap);

    fresh(&st);
    capture_begin(&st, &cap);
    st.loc = 01000;
    mac_line(&st, "'HI'");
    mac_line(&st, "1000 < 1000");
    mac_line(&st, ")9ASCI");
    capture_text(cap, buf, sizeof(buf));
    check_true(")9ASCI dumps characters     ", strstr(buf, "HI") != NULL);
    fclose(cap);

    /* ---- )CORE and )9MSG -------------------------------------------- */
    fresh(&st);
    capture_begin(&st, &cap);
    st.loc = 01000;
    mac_line(&st, "SAA 1");
    mac_line(&st, ")CORE");
    capture_text(cap, buf, sizeof(buf));
    check_true(")CORE reports used bounds   ", strstr(buf, "001000") != NULL);
    fclose(cap);

    fresh(&st);
    capture_begin(&st, &cap);
    mac_line(&st, ")9MSG HELLO THERE");
    capture_text(cap, buf, sizeof(buf));
    check_true(")9MSG echoes its text       ",
               strstr(buf, "HELLO THERE") != NULL);
    fclose(cap);

    /* ---- switches: )9PARI, )9LITR, )SETSM, )RESSM -------------------- */
    fresh(&st);
    check_true("parity check on initially   ", st.parity_check);
    mac_line(&st, ")9PARI");
    check_true(")9PARI toggles parity off   ", !st.parity_check);
    mac_line(&st, ")9PARI");
    check_true(")9PARI toggles parity on    ", st.parity_check);

    fresh(&st);
    check_true("literals shared initially   ", !st.dup_literals);
    mac_line(&st, ")9LITR");
    check_true(")9LITR turns duplication on ", st.dup_literals);
    /* with duplication on, two identical literals take two cells */
    st.loc = 01000;
    mac_line(&st, "LDA (7");
    mac_line(&st, "LDT (7");
    mac_line(&st, ")FILL");
    check_word(")9LITR: first cell          ", st.mem[01002], 7);
    check_word(")9LITR: second, separate    ", st.mem[01003], 7);

    fresh(&st);
    mac_line(&st, ")SETSM");
    check_true(")SETSM sets symbolic mode   ", st.symbolic_out);
    mac_line(&st, ")RESSM");
    check_true(")RESSM restores octal mode  ", !st.symbolic_out);

    /* ---- )9EXIT and its )9TSS alias --------------------------------- */
    fresh(&st);
    mac_line(&st, ")9EXIT");
    check_true(")9EXIT ends the stream      ", st.end_of_file_seen);
    fresh(&st);
    mac_line(&st, ")9TSS");
    check_true(")9TSS is an alias of )9EXIT ", st.end_of_file_seen);

    /* ---- )SOVER is a documented intentional no-op --------------------- */
    /* Per docs/TSS-ARCHITECTURE.md (overlay chapter) it lives only in the
     * "NMACF path (never assembled in the MACF builds) and would need
     * ND-100 execution of TSS's SOVER/XDISK code. mac-c correctly does
     * nothing with it: no image effect, no error. (The overlay pipeline is
     * reproduced by )9MOVE + the CDC writer instead - tested separately
     * below. )8DUMP is IMPLEMENTED - the tape puncher, test [19].)        */
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, ")SOVER");
    check_int(")SOVER emits nothing        ", st.loc, 01000);
    check_int("...and raises no error      ", st.errors, 0);
}

/* ---------------------------------------------------------------------- */
/* 9e. )9ASSM stream handling and nested include                           */
/* ---------------------------------------------------------------------- */
static void test_9assm_streams(void)
{
    printf("[9e] )9ASSM: nested source include and list/object streams\n");

    /* write a small include file */
    FILE *inc = fopen("mac_test_inc.symb", "wb");
    check_true("include file created        ", inc != NULL);
    if (inc == NULL)
    {
        return;
    }
    fprintf(inc, "INCSYM=123\n");
    fprintf(inc, "SAA 5\n");
    fclose(inc);

    /* nested )9ASSM assembles it inline */
    mac_state st;
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, ")9ASSM mac_test_inc.symb");
    /* INCSYM is six characters, so the significant name is its last five */
    check_true(")9ASSM defines include syms ", sym_is_defined(&st, "NCSYM"));
    check_word(")9ASSM assembles include    ", st.mem[01000], 0170405);
    check_int(")9ASSM advances the counter ", st.loc, 01001);

    /* )LIST goes to the OBJECT stream (third )9ASSM argument) */
    fresh(&st);
    mac_line(&st, "OBJSYM=44");
    mac_line(&st, ")9ASSM ,,\"mac_test_obj.txt\"");
    mac_line(&st, ")LIST");
    mac_close_streams(&st);
    FILE *o = fopen("mac_test_obj.txt", "rb");
    check_true("object stream file written  ", o != NULL);
    if (o != NULL)
    {
        char buf[512];
        size_t n = fread(buf, 1, sizeof(buf) - 1, o);
        buf[n] = '\0';
        fclose(o);
        /* OBJSYM -> significant name BJSYM */
        check_true(")LIST writes to object      ",
                   strstr(buf, "BJSYM=000044") != NULL);
    }

    /* "0" selects the dummy device: )LIST then produces nothing */
    fresh(&st);
    mac_line(&st, "QUIET=1");
    mac_line(&st, ")9ASSM ,,0");
    mac_line(&st, ")LIST");   /* must not crash with a NULL object stream */
    check_int("dummy object stream is safe ", st.errors, 0);
    mac_close_streams(&st);

    remove("mac_test_inc.symb");
    remove("mac_test_obj.txt");
}

/* ---------------------------------------------------------------------- */
/* 9f. )BPUN / )9READ round trip                                           */
/* ---------------------------------------------------------------------- */
static void test_bpun_commands(void)
{
    printf("[9f] )BPUN emission and )9READ reload with checksum check\n");
    mac_state st;
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAA 1; SAA 2; SAA 3");
    mac_line(&st, "START=1000");
    mac_line(&st, "1000 < 1002");
    mac_line(&st, ")9ASSM ,,\"mac_test_bp.bpun\"");
    mac_line(&st, ")BPUN START");
    mac_close_streams(&st);

    /* reload into a fresh state and compare */
    mac_state ld;
    fresh(&ld);
    check_true(")9READ accepts the tape     ",
               mac_read_bpun(&ld, "mac_test_bp.bpun"));
    check_word(")BPUN/)9READ word 0         ", ld.mem[01000], 0170401);
    check_word(")BPUN/)9READ word 1         ", ld.mem[01001], 0170402);
    check_word(")BPUN/)9READ word 2         ", ld.mem[01002], 0170403);
    check_int(")9READ sets low bound       ", ld.lo_used, 01000);
    check_int(")9READ sets high bound      ", ld.hi_used, 01002);

    /* a corrupted tape must fail the checksum */
    FILE *f = fopen("mac_test_bp.bpun", "r+b");
    if (f != NULL)
    {
        fseek(f, 204 + 304 + 4, SEEK_SET);   /* first data word */
        fputc(0xFF, f);
        fclose(f);
        mac_state bad;
        fresh(&bad);
        check_true("corrupt tape fails checksum ",
                   !mac_read_bpun(&bad, "mac_test_bp.bpun"));
    }
    check_true("missing tape rejected       ",
               !mac_read_bpun(&st, "no_such_tape.bpun"));
    remove("mac_test_bp.bpun");

    /* )BPUN to the dummy device must not crash */
    fresh(&st);
    mac_line(&st, "S2=1000");
    mac_line(&st, ")BPUN S2");
    check_int(")BPUN to dummy device safe  ", st.errors, 0);
    /* )BPUN with an undefined symbol is an error */
    fresh(&st);
    mac_line(&st, ")BPUN NOSUCH");
    check_true(")BPUN undefined sym errors  ", st.errors > 0);

    /* ---- mac_write_bpun_range called directly ------------------------ */
    fresh(&st);
    st.loc = 02000;
    mac_line(&st, "SAA 1; SAA 2");
    FILE *rf = fopen("mac_test_range.bpun", "wb");
    check_true("range tape file opened      ", rf != NULL);
    if (rf != NULL)
    {
        check_true("mac_write_bpun_range ok     ",
                   mac_write_bpun_range(&st, rf, 02000, 02001, 0));
        fclose(rf);
        mac_state rl;
        fresh(&rl);
        check_true("range tape reloads          ",
                   mac_read_bpun(&rl, "mac_test_range.bpun"));
        check_word("range tape word 0           ", rl.mem[02000], 0170401);
        check_word("range tape word 1           ", rl.mem[02001], 0170402);
        remove("mac_test_range.bpun");
    }
    /* the dummy device must be reported, not crash */
    check_true("bpun_range to NULL is false ",
               !mac_write_bpun_range(&st, NULL, 0, 0, 0));

    /* ---- mac_report_undefined called directly ------------------------ */
    fresh(&st);
    mac_line(&st, "LDA NEVERDEF");
    FILE *uf = tmpfile();
    check_true("undef report tmpfile        ", uf != NULL);
    if (uf != NULL)
    {
        mac_report_undefined(&st, uf);
        char buf[512];
        fflush(uf);
        rewind(uf);
        size_t n = fread(buf, 1, sizeof(buf) - 1, uf);
        buf[n] = '\0';
        fclose(uf);
        check_true("mac_report_undefined lists  ",
                   strstr(buf, "ERDEF") != NULL);  /* NEVERDEF -> last five */
    }
    /* with nothing undefined it says so */
    fresh(&st);
    mac_line(&st, "ALLOK=1");
    uf = tmpfile();
    if (uf != NULL)
    {
        mac_report_undefined(&st, uf);
        char buf[512];
        fflush(uf);
        rewind(uf);
        size_t n = fread(buf, 1, sizeof(buf) - 1, uf);
        buf[n] = '\0';
        fclose(uf);
        check_true("mac_report_undefined empty  ",
                   strstr(buf, "no undefined") != NULL);
    }
}

/* ---------------------------------------------------------------------- */
/* 10. save / load round trip                                              */
/* ---------------------------------------------------------------------- */
static void test_save_load(void)
{
    printf("[10] image save/load round trip and BPUN tape emission\n");
    mac_state st;
    fresh(&st);
    st.loc = 01000;
    mac_line(&st, "SAA 1; SAA 2; SAA 3");
    mac_line(&st, "'HI'");

    const char *img = "mac_test_image.bin";
    check_true("mac_write_image succeeded   ", mac_write_image(&st, img));

    mac_state ld;
    fresh(&ld);
    check_true("mac_read_image succeeded    ", mac_read_image(&ld, img));
    check_int("reloaded base               ", ld.lo_used, st.lo_used);
    check_int("reloaded top                ", ld.hi_used, st.hi_used);

    bool identical = true;
    for (uint32_t a = st.lo_used; a <= st.hi_used; a++)
    {
        if (ld.mem[a] != st.mem[a])
        {
            identical = false;
            printf("  FAIL word %06o: got %06o want %06o\n",
                   a, ld.mem[a], st.mem[a]);
            break;
        }
    }
    check_true("round-trip words identical  ", identical);

    /* bad magic must be rejected */
    FILE *bad = fopen("mac_test_bad.bin", "wb");
    if (bad != NULL)
    {
        fwrite("NOTMAC", 1, 6, bad);
        fclose(bad);
        mac_state b;
        fresh(&b);
        check_true("bad magic rejected          ",
                   !mac_read_image(&b, "mac_test_bad.bin"));
        remove("mac_test_bad.bin");
    }
    check_true("missing file rejected       ",
               !mac_read_image(&ld, "no_such_file_here.bin"));

    /* BPUN tape: verify structure and the additive checksum */
    const char *tape = "mac_test_tape.bpun";
    check_true("mac_write_bpun succeeded    ", mac_write_bpun(&st, tape, 0));
    FILE *f = fopen(tape, "rb");
    check_true("BPUN tape opened            ", f != NULL);
    if (f != NULL)
    {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char *buf = (unsigned char *)malloc((size_t)sz);
        size_t nread = fread(buf, 1, (size_t)sz, f);
        fclose(f);
        check_true("BPUN tape fully read        ", nread == (size_t)sz);

        /* leader is 204 null bytes, then the octal section ending in '!'   */
        bool leader_null = true;
        for (int i = 0; i < 204; i++)
        {
            if (buf[i] != 0)
            {
                leader_null = false;
            }
        }
        check_true("BPUN 204-byte null leader   ", leader_null);
        check_true("BPUN octal section ends '!' ", buf[204 + 303] == '!');

        /* binary block: base, count, data..., checksum */
        long p = 204 + 304;
        uint16_t base = (uint16_t)((buf[p] << 8) | buf[p + 1]);
        uint16_t cnt = (uint16_t)((buf[p + 2] << 8) | buf[p + 3]);
        check_int("BPUN block base             ", base, st.lo_used);
        check_int("BPUN block count            ",
                  cnt, st.hi_used - st.lo_used + 1);
        uint16_t sum = 0;
        for (uint16_t i = 0; i < cnt; i++)
        {
            long o = p + 4 + i * 2;
            sum = (uint16_t)(sum + ((buf[o] << 8) | buf[o + 1]));
        }
        long cso = p + 4 + cnt * 2;
        uint16_t stored = (uint16_t)((buf[cso] << 8) | buf[cso + 1]);
        check_word("BPUN additive checksum      ", stored, sum);
        free(buf);
        remove(tape);
    }
    remove(img);
}

/* ---------------------------------------------------------------------- */
/* 11. assembling a source file end to end                                 */
/* ---------------------------------------------------------------------- */
static void test_file_assembly(void)
{
    printf("[11] end-to-end file assembly with )LINE termination\n");
    const char *src = "mac_test_src.symb";
    FILE *f = fopen(src, "wb");
    if (f == NULL)
    {
        check_true("could not create test source", false);
        return;
    }
    fprintf(f, "%% a small program\n");
    fprintf(f, "1000/\n");
    fprintf(f, "START, SAA 1; SAA 2\n");
    fprintf(f, "       JMP START\n");
    fprintf(f, "VAL=52\n");
    fprintf(f, "       LDA (VAL\n");
    fprintf(f, "       )FILL\n");
    fprintf(f, "       )LINE\n");
    fprintf(f, "SAA 77\n"); /* must NOT be assembled: after )LINE */
    fclose(f);

    mac_state st;
    fresh(&st);
    check_true("mac_assemble_file succeeded ", mac_assemble_file(&st, src));
    check_word("file: SAA 1                 ", st.mem[01000], 0170401);
    check_word("file: SAA 2                 ", st.mem[01001], 0170402);
    /* JMP START at 01002, START=01000 -> disp = -2 = 0376 */
    check_word("file: JMP START             ", st.mem[01002], 0124376);
    /* LDA (VAL at 01003, literal cell at 01004 -> disp 1 */
    check_word("file: LDA (VAL              ", st.mem[01003], 044001);
    check_word("file: literal cell = 52     ", st.mem[01004], 052);
    check_int("file: stopped at )LINE      ", st.errors, 0);
    check_true("file: nothing after )LINE   ", st.mem[01005] == 0);
    remove(src);
}

/* ---------------------------------------------------------------------- */
/* 12. Overlay pipeline: )9MOVE block copy, )CHANGE via OLD/NEW/MASK,       */
/*     )PUNCH object-stream routing, and the CDC-disc image writer.         */
/*     Oracles: ND-60.096.01 (C.1.1.1 )9MOVE, 4.2.3.5 )CHANGE, line 2394    */
/*     )PUNCH) and docs/TSS-ARCHITECTURE.md (overlay chapter) (sec 2/4 disc contract).       */
/* ---------------------------------------------------------------------- */

/* read one big-endian 16-bit word at word index 'w' from a raw image file */
static uint16_t img_word_at(const char *path, uint32_t w)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return 0xFFFF;
    }
    fseek(f, (long)(w * 2), SEEK_SET);
    int hi = fgetc(f), lo = fgetc(f);
    fclose(f);
    if (lo < 0)
    {
        return 0xFFFF;
    }
    return (uint16_t)((hi << 8) | lo);
}

static void test_overlay_pipeline(void)
{
    printf("[12] overlay pipeline: )9MOVE, )CHANGE cells, )PUNCH stream, "
           "CDC image\n");
    mac_state st;
    char buf[4096];

    /* ---- )9MOVE: verbatim block copy within the image ---------------- */
    /* [manual C.1.1.1] )9MOVE src dst count. Copy 4 words 02000->040000.  */
    fresh(&st);
    mac_line(&st, "ROVER=2000");   /* source base as a value symbol        */
    mac_line(&st, "VOR=40000");    /* destination base                     */
    mac_line(&st, "VORS=4");       /* word count                           */
    st.loc = 02000;
    mac_line(&st, "111; 222; 333; 444");
    mac_line(&st, ")9MOVE ROVER VOR VORS");
    check_word(")9MOVE copies word 0        ", st.mem[040000], 0111);
    check_word(")9MOVE copies word 1        ", st.mem[040001], 0222);
    check_word(")9MOVE copies word 2        ", st.mem[040002], 0333);
    check_word(")9MOVE copies word 3        ", st.mem[040003], 0444);
    check_word(")9MOVE does not clobber src ", st.mem[02000], 0111);
    check_word(")9MOVE leaves gap untouched ", st.mem[040004], 0);
    check_int(")9MOVE raises no error       ", st.errors, 0);

    /* numeric operands (no symbols) also work: 3 words 03000->05000       */
    fresh(&st);
    st.loc = 03000;
    mac_line(&st, "1; 2; 3");
    mac_line(&st, ")9MOVE 3000 5000 3");
    check_word(")9MOVE numeric operands     ", st.mem[05000], 1);
    check_word(")9MOVE numeric operands 2   ", st.mem[05002], 3);

    /* an undefined operand is a hard error, copies nothing */
    fresh(&st);
    mac_line(&st, ")9MOVE NOSRC NODST NOCNT");
    check_true(")9MOVE undefined -> error   ", st.errors > 0);

    /* ---- )CHANGE reads OLD/NEW/MASK memory cells (manual 4.2.3.5) ----- */
    /* Deposit 177 into OLD, 0 into NEW, 0777 into MASK, then )CHANGE over
     * an interval: low 9 bits == 177 get zeroed.                          */
    fresh(&st);
    st.loc = 0100;
    mac_line(&st, "OLD, 177");     /* OLD=0100, mem[0100]=177              */
    mac_line(&st, "NEW, 0");       /* NEW=0101, mem[0101]=0                */
    mac_line(&st, "MASK, 777");    /* MASK=0102, mem[0102]=0777            */
    st.loc = 02000;
    mac_line(&st, "177; 177; 200");
    mac_line(&st, "2000 < 2002");
    mac_line(&st, ")CHANGE");
    check_word(")CHANGE via OLD cell w0     ", st.mem[02000], 0);
    check_word(")CHANGE via OLD cell w1     ", st.mem[02001], 0);
    check_word(")CHANGE leaves non-match    ", st.mem[02002], 0200);

    /* ---- )PUNCH goes to the OBJECT stream, not the list stream -------- */
    /* [manual line 2394] Use two DISTINCT streams and confirm the octal
     * dump lands on object and the list stream stays empty.               */
    fresh(&st);
    FILE *lst = tmpfile();
    FILE *obj = tmpfile();
    st.listing = lst;
    st.object = obj;
    st.loc = 01000;
    mac_line(&st, "SAA 1");
    mac_line(&st, "1000 < 1000");
    mac_line(&st, ")PUNCH");
    capture_text(obj, buf, sizeof(buf));
    check_true(")PUNCH writes object stream ", strstr(buf, "001000/") != NULL);
    check_true(")PUNCH shows word value     ", strstr(buf, "170401") != NULL);
    capture_text(lst, buf, sizeof(buf));
    check_true(")PUNCH silent on list stream", buf[0] == '\0');
    fclose(lst);
    fclose(obj);

    /* ---- CDC-disc image writer -------------------------------------- */
    /* Synthetic 2-overlay set. Constants read from the symbol table:
     *   OVDK=4  VORS=01000(512)  RQR=2(count)  VOR=final=base+count*VORS
     * VOR_base = VOR - RQR*VORS = 040000. Windows at 040000 and 041000.
     * Overlay n -> sectors OVDK+2n / OVDK+2n+1 (TSS-ARCHITECTURE.md overlay chapter sec 2). */
    fresh(&st);
    mac_line(&st, "OVDK=4");
    mac_line(&st, "VORS=1000");        /* 512 words = two 256-word sectors */
    mac_line(&st, "RQR=2");            /* two overlays                     */
    mac_line(&st, "VOR=42000");        /* 040000 + 2*01000                 */
    /* deposit sentinels at known offsets inside each 512-word window      */
    st.mem[040000] = 0xA000;           /* overlay0 sec0 word0              */
    st.mem[040000 + 255] = 0xA0FF;     /* overlay0 sec0 last word          */
    st.mem[040000 + 256] = 0xA100;     /* overlay0 sec1 word0              */
    st.mem[040000 + 511] = 0xA1FF;     /* overlay0 sec1 last word          */
    st.mem[041000] = 0xB000;           /* overlay1 sec0 word0              */
    st.mem[041000 + 256] = 0xB100;     /* overlay1 sec1 word0              */

    const char *cdc = "mac_test_cdc.img";

    /* First pin cdc_dkadr itself - the LOGICAL->PHYSICAL map the running DKADR
     * applies. The live anchor (nd100x --mms1 --trace, IOX 503 A=000660 for
     * logical 0244) is ground truth; the closed form is
     *   DKADR(L) = 32*floor(L/12) + 2*(L mod 12)
     * verified per-instruction off the live A/D/T trace with SHR fixed (see the
     * cdc_dkadr header in mac.c). TWO old formulas are superseded: 72*L (from a
     * DKADR whose RGDIV divide was undefined) and 8*floor(L*65537/12)+64*L =
     * 077300 (from a DKADR whose "SHR" shifts were miscompiled as additive left
     * shifts before the eval_expr SHR fix). */
    check_int("cdc_dkadr live anchor 0244   ", (int)cdc_dkadr(0244), 0660);
    check_int("cdc_dkadr(4)                 ", (int)cdc_dkadr(4), 8);
    check_int("cdc_dkadr(5)                 ", (int)cdc_dkadr(5), 10);
    check_int("cdc_dkadr(6)                 ", (int)cdc_dkadr(6), 12);
    check_int("cdc_dkadr(7)                 ", (int)cdc_dkadr(7), 14);
    /* Track boundary: 12 logical sectors per track. L=013(11.)->2*11=22.;
     * L=014(12.) crosses to track 1 -> 32*1+0 = 32. */
    check_int("cdc_dkadr last of track 0    ", (int)cdc_dkadr(013), 22);
    check_int("cdc_dkadr track 1 boundary   ", (int)cdc_dkadr(014), 32);
    check_int("cdc_dkadr masks to 13 bits   ", (int)cdc_dkadr(0244), (int)cdc_dkadr(0244 | 060000));

    check_true("CDC writer succeeds         ", mac_write_cdc_disc(&st, cdc));

    /* Overlays are placed at the PHYSICAL sector DKADR(logical), NOT at the
     * linear logical sector - the running reader runs the logical sector
     * through DKADR before the disc read (see cdc_dkadr / TSS-ARCHITECTURE.md overlay chapter
     * "DKADR physical addressing"). With OVDK=4 and DKADR=32*floor(L/12)+2*(L%12):
     *   overlay0 sec0: logical 4 -> phys  8 ; sec1: logical 5 -> 10
     *   overlay1 sec0: logical 6 -> phys 12 ; sec1: logical 7 -> 14
     * sector S starts at word index S*256.                                  */
    check_word("CDC ovl0 sec0 word0         ", img_word_at(cdc, 8 * 256), 0xA000);
    check_word("CDC ovl0 sec0 last word     ", img_word_at(cdc, 8 * 256 + 255), 0xA0FF);
    check_word("CDC ovl0 sec1 word0         ", img_word_at(cdc, 10 * 256), 0xA100);
    check_word("CDC ovl0 sec1 last word     ", img_word_at(cdc, 10 * 256 + 255), 0xA1FF);
    check_word("CDC ovl1 sec0 word0         ", img_word_at(cdc, 12 * 256), 0xB000);
    check_word("CDC ovl1 sec1 word0         ", img_word_at(cdc, 14 * 256), 0xB100);
    /* sector 0 is not an overlay physical sector here, so it stays zeroed    */
    check_word("CDC pre-overlay region zeroed", img_word_at(cdc, 0), 0);
    /* phys sectors 8,10,12,14 are used; 9 is an odd-sector gap between them   */
    check_word("CDC gap between phys sectors ", img_word_at(cdc, 9 * 256), 0);

    /* file size covers up to the MAX physical sector over ALL overlay sectors:
     * max over {4,5,6,7} is DKADR(7)=14, so (14+1) sectors * 256 words * 2 bytes. */
    FILE *cf = fopen(cdc, "rb");
    long sz = 0;
    if (cf != NULL)
    {
        fseek(cf, 0, SEEK_END);
        sz = ftell(cf);
        fclose(cf);
    }
    check_int("CDC image size correct       ", sz, (14 + 1) * 256 * 2);

    /* writer fails cleanly when the overlay symbols are absent            */
    fresh(&st);
    check_true("CDC writer needs OVDK/etc   ", !mac_write_cdc_disc(&st, cdc));
    remove(cdc);

    /* ---- forward references vs the )9MOVE snapshot ------------------- */
    /* This pins the exact staging semantics that determine whether the CDC
     * overlay image on disc holds the FINAL fixed-up words. In a MACF build
     * each overlay body is assembled at ROVER, then the "MACF OVERX macro
     * snapshots it to the next VOR window with )9MOVE ROVER VOR VORS
     * (TSS3.SYMB:83). )9MOVE is a RAW block copy (ND-60.096.01 C.1.1.1) - it
     * copies whatever words are in the source window AT THAT MOMENT.
     *
     * [VERIFIED live, 2026-07-21] Instrumenting every )9MOVE in the real
     * DRUM build showed ZERO forward references or literal references still
     * unresolved inside the source window at snapshot time: every overlay's
     * internal forward labels and its )FILL'd literals are defined BEFORE the
     * closing OVERX, so the snapshot already holds the patched words. The
     * two cases below lock that contract in:
     *   (a) a forward ref RESOLVED BEFORE )9MOVE -> patched in the VOR copy
     *       and therefore on the CDC disc (the case TSS actually relies on);
     *   (b) a forward ref RESOLVED AFTER )9MOVE -> the VOR copy keeps the
     *       pre-patch word (faithful raw-copy semantics: the fixup patches
     *       the ROVER source, which the next overlay reuses, never the copy).
     * If a future change made )9MOVE snapshot too early, or retro-patched the
     * copy, one of these assertions flips.                                  */

    /* (a) forward ref resolved BEFORE the snapshot lands PATCHED in VOR.
     * Mirror the "MACF OVERX sequence (TSS3.SYMB:80-93): )9MOVE while VOR
     * still points at the window base (40000), THEN advance VOR by VORS.    */
    fresh(&st);
    mac_line(&st, "ROVER=2000");        /* source window base (a value sym) */
    mac_line(&st, "OVDK=4");
    mac_line(&st, "VORS=1000");         /* 512-word window = two sectors    */
    mac_line(&st, "RQR=1");             /* one overlay                      */
    mac_line(&st, "VOR=40000");         /* window base for this overlay     */
    st.loc = 02000;
    mac_line(&st, "LDA FWD");           /* P-relative forward ref @ 02000   */
    mac_line(&st, "STZ 0");
    mac_line(&st, "FWD, STZ 0");        /* FWD=02002 -> resolves @ 02000    */
    check_word("overlay fwd-ref patched src ", st.mem[02000], 044002);
    mac_line(&st, ")9MOVE ROVER VOR VORS");   /* snapshot 02000.. -> 40000.. */
    check_word("overlay fwd-ref patched VOR ", st.mem[040000], 044002);
    /* OVERX then advances the VOR window pointer to its final value; the
     * writer derives VOR_base = VOR - RQR*VORS = 41000 - 1000 = 40000.      */
    mac_line(&st, ")KILL VOR");
    mac_line(&st, "VOR=41000");
    /* it reaches the CDC disc at the DKADR physical sector DKADR(OVDK=4)=8 */
    check_true("CDC writer (fwd-ref) ok     ", mac_write_cdc_disc(&st, cdc));
    check_word("overlay fwd-ref on disc     ", img_word_at(cdc, 8 * 256), 044002);
    remove(cdc);

    /* (b) forward ref resolved AFTER the snapshot: VOR copy stays pre-patch */
    fresh(&st);
    mac_line(&st, "ROVER=2000");
    mac_line(&st, "VORS=1000");
    mac_line(&st, "VOR=40000");         /* window base                      */
    st.loc = 02000;
    mac_line(&st, "GBL2");              /* data word, GBL2 undefined here    */
    mac_line(&st, ")9MOVE ROVER VOR VORS");   /* snapshot BEFORE GBL2 defined */
    mac_line(&st, "GBL2=1234");         /* now define -> patches ROVER src   */
    check_word("post-9MOVE def patches src  ", st.mem[02000], 01234);
    check_word("post-9MOVE def leaves VOR   ", st.mem[040000], 0);
}

/* ---------------------------------------------------------------------- */
/* 13. undefined-instruction guard                                         */
/* An undefined symbol in the OPCODE position of a statement that carries an
 * operand term is a hard error (mac_check_undefined_opcodes), so the build
 * fails instead of silently emitting garbage. A bare undefined mark and a
 * resolved forward reference must NOT trip it. Motivated by the N10/drum
 * build's "RGDIV ST" (src/TSS1.SYMB:3588); see assemble_stmt().            */
static void test_undefined_opcode_guard(void)
{
    printf("[13] undefined-instruction guard\n");
    mac_state st;

    /* ---- undefined opcode WITH operand -> hard error ----------------- */
    /* "ZZZUNDEF ST": ZZZUNDEF is not a permsym/macro/label; ST=060 is a
     * register. MAC's PLAIN fallback would emit (0)+(060)=000060 silently.  */
    fresh(&st);
    mac_line(&st, "ZZZUNDEF ST");
    check_int("undef opcode: no error yet  ", st.errors, 0); /* one-pass: not yet */
    check_int("undef opcode: swept as error", mac_check_undefined_opcodes(&st), 1);
    check_true("undef opcode: st.errors set ", st.errors > 0);

    /* ---- bare undefined single token -> NOT flagged (library mark) --- */
    /* "ZZZMARK" alone is a library-mark / lone data word, never an opcode.  */
    fresh(&st);
    mac_line(&st, "ZZZMARK");
    check_int("bare mark: not swept as error", mac_check_undefined_opcodes(&st), 0);
    check_int("bare mark: no error          ", st.errors, 0);

    /* ---- "SYM+5": one token with operators -> NOT flagged ------------ */
    /* No whitespace-separated operand term, and '+' is not alphanumeric.    */
    fresh(&st);
    mac_line(&st, "ZZZEXPR+5");
    check_int("SYM+const: not an opcode     ", mac_check_undefined_opcodes(&st), 0);

    /* ---- forward reference resolved later -> NOT an error ------------- */
    /* "FWD ST" flags FWD, but "FWD, 0" defines it before the sweep runs.    */
    fresh(&st);
    mac_line(&st, "FWD ST");        /* FWD used as opcode, still undefined   */
    st.loc = 02000;
    mac_line(&st, "FWD, 0");        /* now define FWD as a label             */
    check_int("resolved fwd ref: no error   ", mac_check_undefined_opcodes(&st), 0);
    check_int("resolved fwd ref: errors==0  ", st.errors, 0);

    /* ---- the RGDIV fix: RGDIV=RDIV then "RGDIV ST" -> 0141660 -------- */
    /* Mirrors the N10-gated equate added to src/TSS1.SYMB: RDIV=0141600, so
     * the PLAIN sum "RGDIV ST" is 0141600|060 = 0141660, with no error.     */
    fresh(&st);
    mac_line(&st, "RGDIV=RDIV");    /* RDIV is a permsym = 0141600           */
    uint16_t at = st.loc;
    mac_line(&st, "RGDIV ST");
    check_word("RGDIV ST assembles RDIV|ST  ", st.mem[at], 0141660);
    check_int("RGDIV defined: no opcode err ", mac_check_undefined_opcodes(&st), 0);
    check_int("RGDIV defined: errors==0     ", st.errors, 0);
}

/* ---------------------------------------------------------------------- */
/* 14. undefined-OPERAND guard + CLD permsym                               */
/* An undefined symbol consumed as the VALUE OPERAND of a defined instr
 * (e.g. "SAT STR1", the STR->XTR rename casualty) would silently emit
 * "SAT 0". The guard flags used_as_operand and hard-errors at end. Also
 * checks CLD (=0100), the register-op modifier the extraction had missed,
 * against the values verified on the REAL MAC.                            */
static void test_undefined_operand_guard(void)
{
    printf("[14] undefined-operand guard + CLD\n");
    mac_state st;

    /* ---- ARG8 (SAT) undefined operand -> silently SAT 0, hard error --- */
    fresh(&st);
    uint16_t at = st.loc;
    mac_line(&st, "SAT ZZZUNDEF");
    check_word("SAT undef: emitted SAT 0     ", st.mem[at], 0171000);
    check_int("SAT undef: swept as error    ", mac_check_undefined_opcodes(&st), 1);
    check_true("SAT undef: st.errors set     ", st.errors > 0);

    /* ---- MRI (STT ,B) undefined operand -> hard error ---------------- */
    fresh(&st);
    mac_line(&st, "STT ZZZUNDEF,B");
    check_int("STT undef,B: swept as error  ", mac_check_undefined_opcodes(&st), 1);

    /* ---- resolved forward-ref operand -> NOT an error ---------------- */
    fresh(&st);
    mac_line(&st, "SAT FWDOP");
    st.loc = 02000;
    mac_line(&st, "FWDOP, 0");
    check_int("resolved operand fwd: no error", mac_check_undefined_opcodes(&st), 0);
    check_int("resolved operand fwd: errs==0 ", st.errors, 0);

    /* ---- bare mark line: undefined but NOT an operand -> not flagged -- */
    fresh(&st);
    mac_line(&st, "ZZZMARK2");
    check_int("bare mark2: not an operand    ", mac_check_undefined_opcodes(&st), 0);

    /* ---- CLD = 0100: verified on REAL MAC (SWAP CLD DT SA -> 144156,
     *      SWAP DT SA -> 144056; difference 0100). Confirms the added
     *      permsym encodes the clear-destination bit and CLD is defined.  */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "SWAP CLD DT SA");
    check_word("SWAP CLD DT SA = 144156      ", st.mem[at], 0144156);
    at = st.loc;
    mac_line(&st, "SWAP DT SA");
    check_word("SWAP DT SA = 144056          ", st.mem[at], 0144056);
    check_int("CLD defined: no undef error   ", mac_check_undefined_opcodes(&st), 0);
}

/* [15] Forward-reference expressions keep their constant addend.
 *
 * Found LIVE on nd100x (login debug session 2026-07-23): TSS2 ROBJ ends
 *     RF4,  SAA 4;  JMP RFN+2
 *     RF9,  SAA 11; JMP RFN+2
 *     RFN,  STZ I (BTEMP-4; SAA -1; STA AREG,B; JPL I (UNLOK; RET
 * RFN is a forward reference, and the old PREL8/ARG8 patch wrote
 * (sym - pc) into the displacement, DISCARDING the +2/+3 - so all three
 * exits landed on RFN itself. RF9's error 9 ("index out of range") was
 * then rewritten by "SAA -1" to -1 ("empty object"), GBARR treated that
 * as a valid empty directory slot, and LOGON's OPEN of ()SCRATCH
 * enumerated the SYSTEM user's file directory forever (observed index
 * 124505 octal and climbing) = the interactive-login hang. The golden
 * symbol dumps cannot see this class: word COUNT is unchanged.
 * Real MAC must add the constant: the 1973-78 TSS ran in production, so
 * ROBJ's error exits worked there.                                        */
static void test_forward_ref_addend(void)
{
    printf("[15] forward-ref expression addend\n");
    mac_state st;

    /* ---- the exact ROBJ shape: JMP RFN+2 across a forward label ------- */
    fresh(&st);
    uint16_t at = st.loc;
    mac_line(&st, "SAA 11; JMP RFN+2");           /* at, at+1              */
    mac_line(&st, "RFN, STZ I (BTEMP2; SAA -1; STA 5,B"); /* at+2,+3,+4    */
    /* JMP at at+1 must target RFN+2 = at+4 -> disp 3 (target - here)      */
    check_word("JMP RFN+2 fwd: disp keeps +2 ", st.mem[at + 1], 0124003);

    /* ---- forward JUMP8 with +3 (the R2 "JMP RFN+3" success exit) ------ */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "JMP FW3+3");
    mac_line(&st, "FW3, 0; 0; 0; 0");
    check_word("JMP FW3+3 fwd: disp = 4      ", st.mem[at], 0124004);

    /* ---- forward JUMP8 with a NEGATIVE addend ------------------------- */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "JMP FWN-1");                    /* target = FWN-1 = at+1 */
    mac_line(&st, "0");                            /* at+1                  */
    mac_line(&st, "FWN, 0");                       /* at+2                  */
    check_word("JMP FWN-1 fwd: disp = 1      ", st.mem[at], 0124001);

    /* ---- forward MRI address operand with +1 (LDA FWD+1) -------------- */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "LDA FWM+1");
    mac_line(&st, "FWM, 0; 0");                    /* FWM=at+1, +1 -> at+2  */
    check_word("LDA FWM+1 fwd: disp = 2      ", st.mem[at], 0044002);

    /* ---- forward ARG8 value operand with +2 (SAA CON+2) --------------- */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "SAA FCON+2");
    mac_line(&st, "FCON=5");
    check_word("SAA FCON+2 fwd: arg = 7      ", st.mem[at], 0170407);

    /* ---- backward reference with addend: unchanged behaviour ---------- */
    fresh(&st);
    mac_line(&st, "BK, 0; 0");
    at = st.loc;
    mac_line(&st, "JMP BK+2");                     /* target = at itself... */
    /* BK = at-2, BK+2 = at -> disp 0                                      */
    check_word("JMP BK+2 back: disp = 0      ", st.mem[at], 0124000);

    /* ---- plain forward ref (no addend) still exact -------------------- */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "JMP FW0");
    mac_line(&st, "0");
    mac_line(&st, "FW0, 0");                       /* FW0 = at+2 -> disp 2  */
    check_word("JMP FW0 fwd: disp = 2        ", st.mem[at], 0124002);
}


/* ---------------------------------------------------------------------- */
/* 16. -L address listing (mac_open_asm_listing)                           */
/*                                                                         */
/* The listing exists to map a source line to the address it assembled to. */
/* The two properties that matter are (a) the address column is the        */
/* address of the FIRST word the line emitted, and (b) a FORWARD reference */
/* is recorded like any other word - six sites in mac_stmt.c used to       */
/* duplicate emit()'s body inline and were invisible to the hook, which    */
/* made every forward reference show a blank row while '*' silently        */
/* advanced.                                                               */
/* ---------------------------------------------------------------------- */
static void test_asm_listing(void)
{
    printf("[16] -L address listing\n");
    const char *lst = "mac_test_asm.lst";
    mac_state st;
    fresh(&st);

    check_true("mac_open_asm_listing opens    ",
               mac_open_asm_listing(&st, lst));

    mac_line(&st, "2000/");
    mac_line(&st, "L1,    SAA 20; IOX 505");   /* two words at 2000        */
    mac_line(&st, "L2,    UNDEF9-1");          /* forward ref: one word    */
    mac_line(&st, "L3,    5");
    mac_close_streams(&st);

    FILE *f = fopen(lst, "rb");
    if (f == NULL)
    {
        check_true("listing file was created  ", false);
        return;
    }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    remove(lst);

    /* the two-word line is anchored at 002000 and shows BOTH words        */
    check_true("row: addr + both words       ",
               strstr(buf, "002000  170420 164505") != NULL);
    /* the forward reference is recorded, not blank: UNDEF9 evaluates 0,
       so the emitted word is the constant part -1 = 177777               */
    check_true("row: forward ref recorded    ",
               strstr(buf, "002002  177777") != NULL);
    /* and the line after it is at 002003, proving '*' advanced by one     */
    check_true("row: next line follows at +1 ",
               strstr(buf, "002003  000005") != NULL);
    /* source text is carried through                                      */
    check_true("row: statement text present  ",
               strstr(buf, "UNDEF9-1") != NULL);
    /* the header is written once                                          */
    check_true("listing carries a header     ",
               strstr(buf, "emitted words (octal)") != NULL);

    /* with -L off, nothing is recorded and nothing crashes                */
    fresh(&st);
    st.asm_list = NULL;
    mac_line(&st, "SAA 20");
    check_word("listing off: still assembles ", st.mem[st.loc - 1], 0170420);
}

/* ---------------------------------------------------------------------- */
/* 17. mode 6 ",I ,X" is INDIRECT P-RELATIVE indexed, not X-relative       */
/*                                                                         */
/* ND-60.096.01's addressing-mode table gives mode 6 as ((P)+D)+(X) with   */
/* bits X I B = 1 1 0: D locates the indirect POINTER WORD relative to P,  */
/* and X is added after that word is fetched. Only mode 4 "(X)+D" is       */
/* X-relative. mac_stmt.c used to select the X location counter on the X   */
/* bit alone, which caught mode 6 as well; with xcounter 0 the symbol's    */
/* absolute low byte survived into the displacement field.                 */
/*                                                                         */
/* The bug was invisible from a forward reference, which is why it lived   */
/* so long: a forward ref emits a bare opcode and is patched by            */
/* MAC_FIX_PREL8, which is P-relative and right. Only BACKWARD references  */
/* reach the arithmetic below. The golden dumps cannot see this class at   */
/* all - the word count never changes, only the displacement byte.         */
/*                                                                         */
/* Live consequence: seven sites in TSS1's swapper, whose locals block     */
/* sits between SW4 (forward, correct) and SW5 (backward, broken). At      */
/* 006732 "LDA I J2,X" addressed a fixed word of code instead of           */
/* PDTBL[J], so the page scan never saw a free frame, IDXA and IDXB both   */
/* stayed -1, and SW6 fell straight into FTLER.                            */
/* ---------------------------------------------------------------------- */
static void test_mode6_is_p_relative(void)
{
    printf("[17] mode 6 (,I ,X) is P-relative, backward and forward\n");
    mac_state st;

    /* ---- the exact SW5 shape: pointer cell BEFORE the reference ------- */
    fresh(&st);
    mac_line(&st, "PTR, 0");                       /* the indirect pointer  */
    uint16_t at = st.loc;
    mac_line(&st, "LDA I PTR,X");                  /* disp = PTR - here     */
    /* PTR = at-1, so disp = -1 = 0377; LDA 044000 + mode 6 03000          */
    check_word("LDA I PTR,X back: disp = -1  ", st.mem[at], 0047377);

    /* a second reference one word further on MUST encode a different      */
    /* displacement - identical words from different P was the signature   */
    at = st.loc;
    mac_line(&st, "STA I PTR,X");                  /* PTR = at-2 -> -2      */
    check_word("STA I PTR,X back: disp = -2  ", st.mem[at], 0007376);

    /* ---- forward reference: was already correct, must stay correct ---- */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "LDA I FPTR,X");
    mac_line(&st, "0");
    mac_line(&st, "FPTR, 0");                      /* FPTR = at+2 -> +2     */
    check_word("LDA I FPTR,X fwd: disp = +2  ", st.mem[at], 0047002);

    /* ---- mode 4 ",X" alone stays X-relative (xcounter is 0) ----------- */
    fresh(&st);
    mac_line(&st, "XC, 0");
    at = st.loc;
    mac_line(&st, "LDA XC,X");                     /* absolute, not P-rel   */
    check_word("LDA XC,X mode 4: absolute    ",
               st.mem[at], (uint16_t)(0046000 + ((at - 1) & 0377)));

    /* ---- ",B" forms are unaffected: still base-relative --------------- */
    fresh(&st);
    mac_line(&st, "BC, 0");
    at = st.loc;
    mac_line(&st, "LDA BC,B");
    check_word("LDA BC,B mode 1: absolute    ",
               st.mem[at], (uint16_t)(0044400 + ((at - 1) & 0377)));

    /* ---- mode 7 ",I ,X ,B" stays B-relative (B wins over X) ----------- */
    fresh(&st);
    mac_line(&st, "BX, 0");
    at = st.loc;
    mac_line(&st, "LDA I BX,B,X");
    check_word("LDA I BX,B,X mode 7: B-rel   ",
               st.mem[at], (uint16_t)(0047400 + ((at - 1) & 0377)));
}

/* ---------------------------------------------------------------------- */
/* 18. NLZ/DNZ scaling is an 8-bit field - a negative one must not borrow  */
/*     out of it and corrupt the opcode                                    */
/*                                                                         */
/* MAC statements are sums, so a PLAIN classification computes "DNZ -20"   */
/* as 0152000 - 0000020 = 0151760. That is not DNZ: the scaling field is   */
/* bits 0-7 (NLZ 0151400-0151777, DNZ 0152000-0152377), so the borrow ran  */
/* into the opcode and turned DNZ into NLZ+0360 - the machine normalises   */
/* where the program asked it to denormalise. ND-60.096.01 sec 3.2.2.4     */
/* documents the borrow and the correction: examine bit 7 of the result    */
/* and add 0400 when it is set. ARG8's "opc + (arg & 0377)" does exactly   */
/* that, giving 0152360.                                                   */
/*                                                                         */
/* Live consequence: all twelve "DNZ -20" in TSS are the float-to-integer  */
/* step of TBANG (src/TSS2.SYMB:1371), which decomposes elapsed clock      */
/* ticks into days/hours/minutes/seconds. Measured on a running machine    */
/* with a correct float input of 43 ticks, the miscompiled instruction     */
/* returned -30396 days. Invisible to the golden dumps - one word either   */
/* way, so every symbol address is unchanged.                              */
/* ---------------------------------------------------------------------- */
static void test_nlz_dnz_scaling_field(void)
{
    printf("[18] NLZ/DNZ 8-bit scaling field, negative operands\n");
    mac_state st;

    /* ---- the exact TBANG shape -------------------------------------- */
    fresh(&st);
    uint16_t at = st.loc;
    mac_line(&st, "DNZ -20");
    check_word("DNZ -20: stays in DNZ range ", st.mem[at], 0152360);

    /* the bug produced NLZ's opcode; assert we are not back there       */
    check_true("DNZ -20: not in NLZ range   ",
               st.mem[at] >= 0152000 && st.mem[at] <= 0152377);

    /* ---- positive operands must be untouched ------------------------ */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "DNZ 20");
    check_word("DNZ 20: unchanged           ", st.mem[at], 0152020);

    fresh(&st);
    at = st.loc;
    mac_line(&st, "NLZ 20");
    check_word("NLZ 20: unchanged           ", st.mem[at], 0151420);

    /* ---- NLZ with a negative scaling has the same hazard ------------ */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "NLZ -20");
    check_word("NLZ -20: stays in NLZ range ", st.mem[at], 0151760);

    /* ---- the full TBANG line still assembles as five words ---------- */
    fresh(&st);
    at = st.loc;
    mac_line(&st, "K1, 0; 0");
    mac_line(&st, "FDV K1; DNZ -20; SAA 0; NLZ 20; FMU K1");
    check_word("TBANG line: DNZ word correct", st.mem[at + 3], 0152360);
}

/* ---------------------------------------------------------------------- */
/* 19. )8DUMP - the TSS-binary-format distribution tape, byte for byte    */
/*                                                                        */
/* )8DUMP is MAC's )SYMBOL form executing TSS's own 8DUMP routine         */
/* (src/TSS3.SYMB:191-231, "%DUMP ONTO TSS BINARY FORMAT TAPE"). Every    */
/* byte below is derived from that routine:                               */
/*   - the 127-zero leader        (SAX -177; SAA 0; punch; JNC *-2)       */
/*   - 8NOUT: 6 octal ASCII digits, top bit first (SAD 1 then 5x SAD 3)   */
/*   - the "<HLOAD>/ words CRLF ... <HLOAD>!" ASCII framing (8LOOP)       */
/*   - 8WOUT: HIGH byte then LOW byte (SAD ZIN SHR 10 / SAD 10)           */
/*   - the sync word 125252 = 0xAAAA, which TBOOT scans for and which is  */
/*     byte-phase proof (both bytes 0xAA)                                 */
/*   - block = [8CADR][8DKA][8NWD][words][16-bit sum checksum] (8D2/8LP2) */
/*   - trailer = 177777, 8DKA twice (8WOUT restores A), leader (8D1)      */
/* The disc is NOT touched: the original wrote the disc only when the     */
/* punched tape was BOOTED (TBOOT TB3 -> HDKOP, src/TSS3.SYMB:126).       */
/* ---------------------------------------------------------------------- */
static void expect_bytes(const char *what, const unsigned char *got,
                         const unsigned char *want, int n)
{
    int ok = 1;
    for (int i = 0; i < n; i++)
    {
        if (got[i] != want[i]) { ok = 0; break; }
    }
    check_true(what, ok != 0);
}

static void test_8dump_tape(void)
{
    printf("[19] )8DUMP punches the TSS binary format tape\n");
    mac_state st;
    fresh(&st);

    /* a miniature bootstrap region with the real symbol names            */
    mac_line(&st, "40000/");
    mac_line(&st, "HLOAD, 123456; 54321");
    mac_line(&st, "HLDE=*");
    mac_line(&st, "1234; 174321");
    mac_line(&st, "8TBE=*");
    /* the parameter cells, and a payload for the block form              */
    mac_line(&st, "8FCN, 0");
    mac_line(&st, "8CADR, 0");
    mac_line(&st, "8DKA, 0");
    mac_line(&st, "8NWD, 0");
    mac_line(&st, "PAY, 111; 222; 333");

    st.punch = fopen("mac_test_8dump.tape", "wb");
    check_true(")8DUMP: punch file open      ", st.punch != NULL);

    mac_line(&st, ")8DUMP");                       /* fcn=0: tape header  */
    mac_line(&st, "8FCN/ 1");
    mac_line(&st, "8CADR/ PAY");
    mac_line(&st, "8DKA/ 2");
    mac_line(&st, "8NWD/ 3");
    mac_line(&st, ")8DUMP");                       /* one load block      */
    mac_line(&st, "8CADR/ -1");
    mac_line(&st, "8DKA/ 7");
    mac_line(&st, ")8DUMP");                       /* the trailer         */
    fclose(st.punch);
    st.punch = NULL;
    check_int(")8DUMP: no errors            ", st.errors, 0);

    unsigned char buf[512];
    FILE *f = fopen("mac_test_8dump.tape", "rb");
    long n = (long)fread(buf, 1, sizeof(buf), f);
    fclose(f);

    /* total = header 163 + block 14 + trailer 133                        */
    check_int(")8DUMP: tape length          ", n, 163 + 14 + 133);

    int p = 0, ok;
    for (ok = 1; p < 0177; p++) { if (buf[p] != 0) { ok = 0; } }
    check_true(")8DUMP: 127-zero leader      ", ok != 0);
    expect_bytes(")8DUMP: '<HLOAD>/' framing   ", buf + p,
                 (const unsigned char *)"040000/", 7);
    p += 7;
    expect_bytes(")8DUMP: word 1 octal ASCII   ", buf + p,
                 (const unsigned char *)"123456\r\n", 8);
    p += 8;
    expect_bytes(")8DUMP: word 2 octal ASCII   ", buf + p,
                 (const unsigned char *)"054321\r\n", 8);
    p += 8;
    expect_bytes(")8DUMP: '<HLOAD>!' framing   ", buf + p,
                 (const unsigned char *)"040000!", 7);
    p += 7;
    {   /* binary section, 8WOUT high byte first: 001234, 174321, sync    */
        const unsigned char bin[6] =
            { 0x02, 0x9C, 0xF8, 0xD1, 0xAA, 0xAA };
        expect_bytes(")8DUMP: binary + AAAA sync   ", buf + p, bin, 6);
        p += 6;
    }
    check_int(")8DUMP: header length        ", p, 163);
    {   /* block: [PAY][2][3][111][222][333][chk 666]                     */
        uint16_t pay = sym_lookup_value(&st, "PAY");
        const unsigned char blk[14] =
            { (unsigned char)(pay >> 8), (unsigned char)(pay & 0xFF),
              0x00, 0x02,  0x00, 0x03,
              0x00, 0x49,  0x00, 0x92,  0x00, 0xDB,   /* 111 222 333 */
              0x01, 0xB6 };                           /* sum = 000666 */
        expect_bytes(")8DUMP: load block bytes     ", buf + p, blk, 14);
        p += 14;
    }
    {   /* trailer: 177777, 8DKA=7 twice, then the blank leader           */
        const unsigned char tr[6] =
            { 0xFF, 0xFF, 0x00, 0x07, 0x00, 0x07 };
        expect_bytes(")8DUMP: trailer words        ", buf + p, tr, 6);
        p += 6;
        for (ok = 1; p < n; p++) { if (buf[p] != 0) { ok = 0; } }
        check_true(")8DUMP: trailer leader       ", ok != 0);
    }

    /* without a punch device )8DUMP must error, not silently vanish      */
    mac_state st2;
    fresh(&st2);
    mac_line(&st2, "8FCN, 1");
    mac_line(&st2, ")8DUMP");
    check_int(")8DUMP: no punch = error     ", st2.errors, 1);

    remove("mac_test_8dump.tape");
}

/* ---------------------------------------------------------------------- */
int main(void)
{
    printf("=== MAC-C assembler unit tests ===\n\n");

    test_all_permanent_symbols();
    test_every_mri_every_mode();
    test_every_jump_and_arg();
    test_register_class_forms();
    test_addressing_modes();
    test_p_relative();
    test_statement_forms();
    test_strings();
    test_literals_and_fill();
    test_forward_references();
    test_library_marks();
    test_commands();
    test_macros();
    test_corpus_constructs();
    test_remaining_commands();
    test_9assm_streams();
    test_bpun_commands();
    test_save_load();
    test_file_assembly();
    test_overlay_pipeline();
    test_undefined_opcode_guard();
    test_undefined_operand_guard();
    test_forward_ref_addend();
    test_asm_listing();
    test_mode6_is_p_relative();
    test_nlz_dnz_scaling_field();
    test_8dump_tape();

    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
