/* MAC-C - statement dispatch, library marks, line processing
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* statement assembly                                                      */
/* ---------------------------------------------------------------------- */

/* Look up the class of the leading token (permanent instructions carry a
 * class; locals default to PLAIN data).                                   */
mac_sym_class leading_class(mac_state *st, const char *tok)
{
    mac_sym *s = sym_find(st, tok);
    if (s != NULL)
    {
        return s->cls;
    }
    return MAC_CLS_PLAIN;
}

/* Assemble one already-trimmed statement (no ';', no comment).            */
void assemble_stmt(mac_state *st, char *stmt);

/* Split leading opcode token off the statement; the rest is the operand. */
void first_token(const char *stmt, char *op, const char **rest)
{
    int i = 0;
    while (stmt[i] != '\0' && !isspace((unsigned char)stmt[i]))
    {
        op[i] = stmt[i];
        i++;
    }
    op[i] = '\0';
    while (isspace((unsigned char)stmt[i]))
    {
        i++;
    }
    *rest = stmt + i;
}

/* ---------------------------------------------------------------------- */
void cmd_interval(mac_state *st, char *stmt, char *lt)
{
    *lt = '\0';
    mac_sym *u1 = NULL, *u2 = NULL;
    bool a1 = false, a2 = false;
    uint16_t lo = eval_expr(st, stmt, &u1, &a1);
    uint16_t hi = eval_expr(st, lt + 1, &u2, &a2);
    if (u1 != NULL || u2 != NULL)
    {
        /* "If there is an undefined expression ... the command is ignored" */
        return;
    }
    st->ilow = lo;
    st->ihigh = hi;
}

/* ---------------------------------------------------------------------- */

void assemble_stmt(mac_state *st, char *stmt)
{
    /* trim leading/trailing whitespace */
    while (*stmt != '\0' && isspace((unsigned char)*stmt))
    {
        stmt++;
    }
    size_t len = strlen(stmt);
    while (len > 0 && isspace((unsigned char)stmt[len - 1]))
    {
        stmt[--len] = '\0';
    }
    if (len == 0)
    {
        return;
    }
    /* A '#ab' constant may legitimately END in a blank, e.g. TSS2:2507
     * "... SAT ## " where the constant is ('#',' '). The trim above would
     * eat that blank and leave a meaningless bare "##", so put it back.
     * (The original 8-bit source was checked: the trailing space is real.) */
    if (len >= 2 && stmt[len - 2] == '#')
    {
        stmt[len] = ' ';
        stmt[len + 1] = '\0';
    }

    /* ---- interval command:  LOW < HIGH  ----------------------------- */
    /* Sets the interval used by )ZERO, )PRINT, )PUNCH, )9ASCI, )BPUN and
     * )CHANGE (ND-60.096.01 sec 3.2.3.6).                                */
    {
        /* the scan skips '#ab' constants so that "SAA ##<" (TSS3:1247) is
         * not mistaken for an interval command                            */
        char *lt = NULL;
        for (char *p = stmt; *p != '\0'; )
        {
            const char *nx = skip_char_const(p);
            if (nx != NULL)
            {
                p = (char *)nx;
                continue;
            }
            if (*p == '<')
            {
                lt = p;
                break;
            }
            p++;
        }
        if (lt != NULL && stmt[0] != '\'')
        {
            cmd_interval(st, stmt, lt);
            return;
        }
    }

    /* ---- ASCII text string:  'TEXT'  ------------------------------- */
    /* Per ND-60.096.01 sec 3.2.3.7: characters between the quotes PLUS the
     * terminating quote are packed two per word, the first character in the
     * high byte (even byte = left half, sec 2.2). An odd final character
     * zero-pads the low byte. Checked before comma/label parsing so that a
     * ',' or '=' inside the string is not mistaken for a label or an        *
     * assignment.                                                          */
    if (stmt[0] == '\'')
    {
        const char *q = stmt + 1;
        char packed[2];
        int half = 0;
        while (*q != '\0' && *q != '\'')
        {
            packed[half++] = *q++;
            if (half == 2)
            {
                emit(st, (uint16_t)(((unsigned char)packed[0] << 8) |
                                    (unsigned char)packed[1]));
                half = 0;
            }
        }
        /* the terminating quote is part of the string */
        packed[half++] = '\'';
        if (half == 2)
        {
            emit(st, (uint16_t)(((unsigned char)packed[0] << 8) |
                                (unsigned char)packed[1]));
        }
        else
        {
            emit(st, (uint16_t)((unsigned char)packed[0] << 8));
        }
        return;
    }

    /* ---- location set:  EXPR/  ...rest of statement ------------------ */
    /* This MUST be tested before the label rule: TSS writes lines such as
     *     DEVTB+NTTY+1/OEV,TTY1O;TTY2O;...
     * where an expression sets the location counter and the REST of the
     * statement continues after the '/'. Checking the label (comma) rule
     * first would swallow "DEVTB+NTTY+1/OEV" as a label name and, after the
     * 5-character truncation, silently redefine DEVTB.                    */
    {
        /* Scan for the '/' that sets the location counter, skipping '##c'
         * character constants (e.g. "SAT ##/" must NOT be read as a
         * location set) and stopping at a ',' which means this is the
         * label form instead.                                             */
        char *sl = NULL;
        for (char *p = stmt; *p != '\0'; )
        {
            const char *nx = skip_char_const(p);
            if (nx != NULL)
            {
                p = (char *)nx;
                continue;
            }
            if (*p == '/')
            {
                sl = p;
                break;
            }
            if (*p == ',')
            {
                break;
            }
            p++;
        }
        if (sl != NULL)
        {
            char addr[128];
            size_t n = (size_t)(sl - stmt);
            if (n >= sizeof(addr))
            {
                n = sizeof(addr) - 1;
            }
            memcpy(addr, stmt, n);
            addr[n] = '\0';
            mac_sym *u = NULL;
            bool a = false;
            uint16_t v = eval_expr(st, addr, &u, &a);
            if (u != NULL)
            {
                mac_err(st, "undefined symbol in location set:", u->name);
            }
            st->loc = v;
            assemble_stmt(st, sl + 1); /* continue with whatever follows */
            return;
        }
    }

    /* ---- label definition:  NAME,  (possibly followed by more stmt) --- */
    /* the comma scan also skips '##c' so that "SAA ##," is not mistaken
     * for a label definition                                             */
    char *comma = NULL;
    for (char *p = stmt; *p != '\0'; )
    {
        const char *nx = skip_char_const(p);
        if (nx != NULL)
        {
            p = (char *)nx;
            continue;
        }
        if (*p == ',')
        {
            comma = p;
            break;
        }
        p++;
    }
    if (comma != NULL)
    {
        /* only a label if the token before ',' is a bare symbol           */
        char label[64];
        size_t n = (size_t)(comma - stmt);
        bool simple = (n > 0 && n < 63);
        for (size_t i = 0; i < n && simple; i++)
        {
            if (isspace((unsigned char)stmt[i]))
            {
                simple = false;
            }
        }
        if (simple)
        {
            memcpy(label, stmt, n);
            label[n] = '\0';
            mac_sym *s = sym_intern(st, label);
            if (s->defined)
            {
                /* MAC reports ALREADY DEFINED and KEEPS the original value;
                 * ")KILL name" must precede any intentional redefinition
                 * (ND-60.096.01, )KILL). This matters because names are
                 * significant to 5 characters, so e.g. LBYTE / LBYTE1 /
                 * LBYTE2 all collide - and the golden ASYMB keeps the FIRST
                 * (LBYTE=010375), proving first-definition-wins.          */
                st->warnings++;
                assemble_stmt(st, comma + 1);
                return;
            }
            s->value = st->loc;
            s->defined = true;
            s->address = true;
            s->def_seq = ++st->def_seq;
            mac_fixup **head = pending_chain_for(s);
            sym_resolve_fixups(st, s, head);
            /* continue with the remainder after the comma */
            assemble_stmt(st, comma + 1);
            return;
        }
    }

    /* ---- assignment:  NAME=EXPR  ------------------------------------ */
    char *eq = strchr(stmt, '=');
    if (eq != NULL)
    {
        char name[64];
        size_t n = (size_t)(eq - stmt);
        while (n > 0 && isspace((unsigned char)stmt[n - 1]))
        {
            n--;
        }
        memcpy(name, stmt, n);
        name[n] = '\0';
        mac_sym *u = NULL;
        bool a = false;
        uint16_t v = eval_expr(st, eq + 1, &u, &a);
        if (u != NULL)
        {
            mac_err(st, "forward reference in '=' to", u->name);
        }
        mac_sym *s = sym_intern(st, name);
        s->value = v;
        s->defined = true;
        s->address = a;
        s->def_seq = ++st->def_seq;
        mac_fixup **head = pending_chain_for(s);
        sym_resolve_fixups(st, s, head);
        return;
    }

    /* NOTE: there is deliberately no built-in BSS here. BSS is NOT a MAC
     * permanent symbol (it is absent from the table in MAC.BPUN); TSS
     * defines it itself in TSS1 as ")MCDEF BSS $A1" with the body
     * "*+$A1 /", i.e. it only advances the location counter and emits no
     * words. Hard-coding a zero-filling BSS here would diverge from the
     * corpus, so the source's own macro governs.                          */

    /* ---- data word / instruction ------------------------------------ */
    char op[64];
    const char *rest;
    first_token(stmt, op, &rest);

    /* ---- macro call: leading token names a defined macro ------------- */
    /* The token must consist only of symbol characters. TSS2 writes
     * "PROGM&M": because '&' cannot appear in a symbol, MAC reads the
     * symbol PROGM and then '&M' as further terms, i.e. the line is an
     * EXPRESSION that emits one word, not a macro call. Without this check
     * the 5-character truncation would match the PROGM macro and swallow
     * the word (golden BCOPY=027051 proves the word is emitted).          */
    {
        bool plain_token = (op[0] != '\0');
        for (int i = 0; op[i] != '\0'; i++)
        {
            if (!isalnum((unsigned char)op[i]))
            {
                plain_token = false;
                break;
            }
        }
        if (plain_token)
        {
            int mi = macro_find(st, op);
            if (mi >= 0)
            {
                macro_expand(st, mi, rest);
                return;
            }
        }
    }

    mac_sym_class cls = leading_class(st, op);

    /* ---- undefined-instruction guard -------------------------------- */
    /* An undefined symbol in the OPCODE position of a statement that also
     * carries an operand term is almost certainly a missing/mistyped
     * instruction, not data. The motivating case is TSS1's "N10 CDC" DKADR
     * routine (src/TSS1.SYMB:3588): "SAT 14; RGDIV ST; COPY DT SA", where
     * RGDIV is the NORD-10 register-divide (= RDIV = 0141600) the source
     * spells RGDIV. RGDIV is DEFINED only as a "NN10 software-emulation
     * routine (src/TSS1.SYMB:4184, inside the "NN10 block at 4092), so in the
     * N10/drum build it is undefined. MAC's PLAIN fallback (see the sum block
     * near the end of this function) would then evaluate "RGDIV ST" as
     * (RGDIV=undefined->0) + (ST=060) = 000060 and silently emit that garbage
     * data word (a STZ 60) with a fixup that never fires - corrupting DKADR
     * and hanging the TSS boot. See CLAUDE.md and docs.
     *
     * Because this is a ONE-PASS assembler the leading symbol may be a
     * legitimate FORWARD REFERENCE (defined later in the stream), so we must
     * NOT error here. Instead we FLAG the symbol (used_as_opcode) and let the
     * end-of-assembly sweep mac_check_undefined_opcodes() promote it to a hard
     * error only if it is STILL undefined when the whole build finishes.
     *
     * Guard conditions (ALL must hold), chosen so this never fires on the
     * corpus's legitimate undefined symbols:
     *   - cls == PLAIN: real instructions (MRI/JUMP8/ARG8) were dispatched
     *     above and are by definition defined permsyms.
     *   - op is a CLEAN symbol token (all alphanumeric): excludes "&L" and
     *     "PROGM&M" (expressions by design, CLAUDE.md corpus notes) as well as
     *     "[..." floats and "(..." literals whose first token is not a symbol.
     *   - op is not a defined symbol, permsym or macro: a defined leading
     *     token is a data label legitimately summed with the rest.
     *   - there IS a further whitespace-separated operand term (rest != "")
     *     AND at least one such term is a REAL OPERAND - a number, a '#ab'
     *     character constant, or a DEFINED symbol/permsym/register. This is
     *     the key discriminator between an instruction and a MARK-DECLARATION
     *     line: the ASSYSA/ASSYSB command streams open with a bare list of
     *     library marks, e.g. "CDC MACF DIAB K14 TEL4" (src/ASSYSA.SYMB:4),
     *     which is a sum of ALL-UNDEFINED symbols (that is exactly how the
     *     marks are made). "RGDIV ST" differs because ST=060 is defined, so
     *     it looks like an instruction; the all-undefined mark list does not.
     *     A bare single token is likewise a lone mark / data word / forward
     *     label, and "SYM+5" is one token with operators (no further term).
     * The known-undefined casualties STR/STR0..2/STR1X/STR2X and REA/RKE
     * appear only as OPERANDS (e.g. "SAT STR0", "IOX REA RDR"), never as a
     * leading token, so they are unaffected (verified against the sources).  */
    if (cls == MAC_CLS_PLAIN && op[0] != '\0' && rest[0] != '\0')
    {
        bool clean_sym = true;
        for (int i = 0; op[i] != '\0'; i++)
        {
            if (!isalnum((unsigned char)op[i]))
            {
                clean_sym = false;
                break;
            }
        }
        /* Does any further term resolve to a value (i.e. is a real operand)?
         * Walk the whitespace/sign-separated terms of 'rest'; a term counts
         * as a real operand if it starts with a digit (a number), is a '#ab'
         * character constant, or names a DEFINED symbol (permsym/register/
         * label). If every further term is itself an undefined symbol the
         * statement is a mark-declaration list, not an instruction.          */
        bool has_defined_operand = false;
        if (clean_sym && macro_find(st, op) < 0)
        {
            const char *p = rest;
            while (*p != '\0')
            {
                /* skip separators: whitespace and the sign operators +/-     */
                while (*p != '\0' &&
                       (isspace((unsigned char)*p) || *p == '+' || *p == '-'))
                {
                    p++;
                }
                if (*p == '\0')
                {
                    break;
                }
                if (isdigit((unsigned char)*p) || *p == '#')
                {
                    has_defined_operand = true; /* number or char constant    */
                    break;
                }
                /* collect one symbol term (symbol characters only)           */
                char term[64];
                int ti = 0;
                while (*p != '\0' && !isspace((unsigned char)*p) &&
                       *p != '+' && *p != '-' && ti < 63)
                {
                    term[ti++] = *p++;
                }
                term[ti] = '\0';
                if (ti > 0)
                {
                    mac_sym *ts = sym_find(st, term);
                    if (ts != NULL && ts->defined)
                    {
                        has_defined_operand = true; /* defined operand symbol */
                        break;
                    }
                }
            }
        }
        if (clean_sym && has_defined_operand && macro_find(st, op) < 0)
        {
            mac_sym *ls = sym_find(st, op);
            if (ls == NULL || !ls->defined)
            {
                /* get-or-create; a real forward ref will clear the flag by
                 * being defined before mac_check_undefined_opcodes runs.     */
                ls = sym_intern(st, op);
                ls->used_as_opcode = true;
            }
        }
    }

    if (cls == MAC_CLS_MRI)
    {
        /* word = opcode + mode_bits + (displacement & 0377). Mode bits and
         * displacement are evaluated separately (see eval_operand).        */
        mac_sym *ub = NULL;
        bool baseaddr = false;
        uint16_t opc = eval_term(st, op, &ub, &baseaddr);
        (void)baseaddr;

        mac_operand oper;
        eval_operand(st, rest, &oper);
        uint16_t here = st->loc;

        if (oper.literal >= 0)
        {
            /* address a literal cell P-relative; patched at )FILL time.     */
            /* emit() so the -L listing records this word too; identical
             * semantics to the former inline store (here == st->loc). */
            emit(st, (uint16_t)(opc + oper.mode));
            litref *r = (litref *)calloc(1, sizeof(litref));
            r->addr = here;
            r->pc = here;
            r->lit_index = oper.literal;
            r->next = g_litrefs;
            g_litrefs = r;
            return;
        }

        if (oper.undef != NULL)
        {
            /* forward ref: emit opcode + any mode bits now; the displacement
             * is patched P-relative (address operand) when the label is
             * defined. TSS forward references are P-relative labels.
             * oper.disp carries the expression's constant part (undefined
             * symbols evaluate as 0), e.g. the +1 of "LDA FWD+1" - it rides
             * in the fixup so the patch targets FWD+1, not FWD.             */
            /* emit() so the -L listing records this word too; identical
             * semantics to the former inline store (here == st->loc). */
            emit(st, (uint16_t)(opc + oper.mode));
            pending_add(oper.undef, here, here, MAC_FIX_PREL8, oper.disp);
            /* Flag: undefined VALUE OPERAND of a defined MRI (e.g. "STT STRX,B").
             * A real forward ref clears this by being defined; if still
             * undefined at end, mac_check_undefined_opcodes() hard-errors.   */
            oper.undef->used_as_operand = true;
            return;
        }

        uint16_t disp;
        /* An ADDRESS operand is made relative to the matching location
         * counter (ND-60.096.01 sec 2.6 and )9SET):
         *   no ,B/,X  -> P-relative, base = this instruction's address
         *   ,B        -> base = the B location counter
         *   ,X        -> base = the X location counter
         * A non-address operand is already a displacement and is used as is,
         * which is why plain "LDA 5,B" still works (both counters are 0
         * unless )9SET changed them).                                      */
        if (oper.is_addr)
        {
            if (oper.mode & 00400)
            {
                disp = (uint16_t)((int)oper.disp - (int)st->bcounter);
            }
            else if (oper.mode & 02000)
            {
                disp = (uint16_t)((int)oper.disp - (int)st->xcounter);
            }
            else
            {
                disp = (uint16_t)((int)oper.disp - (int)here);
            }
        }
        else
        {
            disp = oper.disp;
        }
        emit(st, (uint16_t)(opc + oper.mode + (disp & 0x00FF)));
        return;
    }

    if (cls == MAC_CLS_JUMP8)
    {
        /* conditional jumps: always P-relative, 8-bit displacement, no modes */
        mac_sym *ub = NULL;
        bool a = false;
        uint16_t opc = eval_term(st, op, &ub, &a);
        mac_operand oper;
        eval_operand(st, rest, &oper);
        uint16_t here = st->loc;
        if (oper.undef != NULL)
        {
            /* emit() so the -L listing records this word too; identical
             * semantics to the former inline store (here == st->loc). */
            emit(st, opc);
            /* oper.disp = constant part of "JMP RFN+2" (2); without it all
             * three ROBJ error exits landed on RFN itself - see mac.h note */
            pending_add(oper.undef, here, here, MAC_FIX_PREL8, oper.disp);
            oper.undef->used_as_operand = true; /* undefined JUMP8 target */
            return;
        }
        int disp = (int)oper.disp - (int)here;
        emit(st, (uint16_t)(opc + (disp & 0x00FF)));
        return;
    }

    if (cls == MAC_CLS_ARG8)
    {
        /* argument instructions SAA/AAA/...: operand is a literal, & 0377   */
        mac_sym *ub = NULL;
        bool a = false;
        uint16_t opc = eval_term(st, op, &ub, &a);
        mac_sym *uo = NULL;
        bool oa = false;
        uint16_t arg = eval_expr(st, rest, &uo, &oa);
        uint16_t here = st->loc;
        if (uo != NULL)
        {
            /* emit() so the -L listing records this word too; identical
             * semantics to the former inline store (here == st->loc). */
            emit(st, opc);
            /* arg = constant part of the expression (undef sym counted 0)  */
            pending_add(uo, here, here, MAC_FIX_ARG8, arg);
            /* Flag: undefined VALUE OPERAND of a defined ARG8 instr, e.g.
             * "SAT STR1" (the STR->XTR rename casualty) -> silently SAT 0.
             * Errors at end of assembly if still undefined.                  */
            uo->used_as_operand = true;
            return;
        }
        emit(st, (uint16_t)(opc + (arg & 0x00FF)));
        return;
    }

    /* ---- floating point constant:  [NUMBER  -------------------------- */
    /* MAC exists in two variants (ND-60.096.01 sec 2.1.4 and Appendix E):
     *   48-bit (standard): 3 words, 32-bit mantissa, exponent bias 040000
     *   32-bit (optional): 2 words, 22+1 bit mantissa, exponent bias 0400
     *
     * WHICH ONE APPLIES HERE IS DECIDED BY EVIDENCE, NOT PREFERENCE:
     *   - D:\ND\BPUN\MAC.BPUN is the 48-bit variant: its permanent table
     *     carries 2OR3=3, LDR=034000 (=LDF) and STR=030000 (=STF), which
     *     Appendix E lists as the 48-bit values.
     *   - The archived ASYMB:SYMB golden dump, however, was produced by a
     *     32-bit-float MAC: TBANG..RDATE spans 55 words there, and the four
     *     "[" constants K1..K4 in that span account for the difference
     *     exactly (4x2=8 words, not 4x3=12).
     * They are therefore different MAC builds. The default reproduces the
     * TSS corpus (32-bit); set st->float48 for the 48-bit variant.        */
    if (stmt[0] == '[')
    {
        const char *np = stmt + 1;
        while (*np != '\0' && isspace((unsigned char)*np))
        {
            np++;
        }
        double v = strtod(np, NULL);
        int neg = 0;
        int expo = 0;
        if (v != 0.0)
        {
            neg = (v < 0.0);
            if (neg)
            {
                v = -v;
            }
            while (v >= 1.0) { v /= 2.0; expo++; }
            while (v < 0.5)  { v *= 2.0; expo--; }
        }

        if (st->float48)
        {
            uint16_t w0 = 0, w1 = 0, w2 = 0;
            if (v != 0.0)
            {
                unsigned long long mant =
                    (unsigned long long)(v * 4294967296.0 + 0.5);
                if (mant >= 0x100000000ULL)
                {
                    mant >>= 1;
                    expo++;
                }
                w0 = (uint16_t)(((unsigned)(expo + 040000) & 077777) |
                                (neg ? 0100000u : 0u));
                w1 = (uint16_t)((mant >> 16) & 0xFFFF);
                w2 = (uint16_t)(mant & 0xFFFF);
            }
            emit(st, w0);
            emit(st, w1);
            emit(st, w2);
        }
        else
        {
            /* 32-bit: word0 = sign(15) | exponent(14..6) | mantissa hi(5..0)
             * word1 = mantissa lo(15..0). The mantissa's most significant
             * bit is implicit (Appendix E: "the one extra bit ... is set to
             * one if not all bits in the exponent are zero ... removed in
             * the result"), so 22 explicit bits are stored.                */
            uint16_t w0 = 0, w1 = 0;
            if (v != 0.0)
            {
                unsigned long mant = (unsigned long)(v * 8388608.0 + 0.5);
                if (mant >= 0x800000UL)     /* rounding carry out of 23 bits */
                {
                    mant >>= 1;
                    expo++;
                }
                unsigned long m22 = mant & 0x3FFFFFUL; /* drop implicit MSB */
                w0 = (uint16_t)((neg ? 0100000u : 0u) |
                                (((unsigned)(expo + 0400) & 0777) << 6) |
                                (unsigned)((m22 >> 16) & 077));
                w1 = (uint16_t)(m22 & 0xFFFF);
            }
            emit(st, w0);
            emit(st, w1);
        }
        return;
    }

    /* ---- literal used as a DATA word:  (EXPR  ------------------------ */
    /* e.g. TSS1's "REGTB, (0; DRGR; RBLOK; BRGR" - the word holds the
     * ADDRESS of a literal cell that )FILL will place. Handled here
     * because the MRI path only covers literals used as operands.        */
    if (stmt[0] == '(')
    {
        int li = literal_intern(st, stmt + 1);
        uint16_t here = st->loc;
        /* emit() so the -L listing records this word too; identical
         * semantics to the former inline store (here == st->loc). */
        emit(st, 0);
        litref *r = (litref *)calloc(1, sizeof(litref));
        r->addr = here;
        r->pc = here;
        r->lit_index = li;
        r->absolute = true;
        r->next = g_litrefs;
        g_litrefs = r;
        return;
    }

    /* PLAIN: sum of all terms into one word (COPY/IOX/MON/register ops,
     * and bare data words / labels).                                      */
    {
        mac_sym *u = NULL;
        bool a = false;
        uint16_t v = eval_expr(st, stmt, &u, &a);
        uint16_t here = st->loc;
        if (u != NULL)
        {
            /* emit() so the -L listing records this word too; identical
             * semantics to the former inline store (here == st->loc). */
            emit(st, v);   /* constant part pre-stored; FULL patch ADDS */
            pending_add(u, here, here, MAC_FIX_FULL, 0);
            return;
        }
        emit(st, v);
    }
}

/* ---------------------------------------------------------------------- */
/* library-mark condition: value is TRUE iff EXPR's symbols are all
 * *undefined* (referenced but not defined) - the classic MAC library mark.
 * A leading '-' in a token negates that token's truth.                    */
/* ---------------------------------------------------------------------- */
bool eval_mark(mac_state *st, const char *expr)
{
    /* Space/AND separated tokens; each true if the symbol is a library
     * mark (present-but-undefined). '-' prefix negates.                   */
    /* Terms are separated by whitespace AND by '-', where '-' negates the
     * term that follows it. TSS writes both spaced and unspaced forms:
     *   "N10 -VERSA   -> N10 AND NOT VERSA
     *   "CDC-DRUM     -> CDC AND NOT DRUM   (this one has no space, and
     *                    treating it as a single token left TRSFR
     *                    undefined; golden ASYMB has TRSFR=007672)
     *   "-OLA NN10    -> NOT OLA AND NN10                                 */
    bool result = true;
    char tok[64];
    const char *p = expr;
    bool any = false;
    bool negate_next = false;

    while (*p != '\0')
    {
        while (isspace((unsigned char)*p))
        {
            p++;
        }
        if (*p == '-')
        {
            negate_next = true;
            p++;
            continue;
        }
        int i = 0;
        while (*p != '\0' && !isspace((unsigned char)*p) && *p != '-' &&
               i < 63)
        {
            tok[i++] = *p++;
        }
        tok[i] = '\0';
        if (i == 0)
        {
            if (*p == '\0')
            {
                break;
            }
            continue;
        }
        any = true;
        mac_sym *s = sym_find(st, tok);
        bool is_mark = (s != NULL && !s->defined); /* present but undefined */
        bool tval = negate_next ? !is_mark : is_mark;
        negate_next = false;
        result = result && tval;
    }
    return any ? result : true;
}

/* ---------------------------------------------------------------------- */
/* line processing                                                         */
/* ---------------------------------------------------------------------- */

/* The real per-line worker. mac_line() below wraps it so the -L address
 * listing can bracket every return path without disturbing this control
 * flow (which has many early returns).                                    */
static void mac_line_inner(mac_state *st, const char *line)
{
    st->cur_line++;

    /* While capturing a macro definition every line is swallowed verbatim
     * (comments included) until the ']' terminator.                       */
    if (st->capturing)
    {
        char raw[1024];
        size_t r = 0;
        for (const char *p = line; *p != '\0' && r < sizeof(raw) - 1; p++)
        {
            if (*p == '\r' || *p == '\n')
            {
                break;
            }
            raw[r++] = *p;
        }
        raw[r] = '\0';
        macro_capture_line(st, raw);
        return;
    }

    /* strip trailing CR/LF */
    char buf[1024];
    size_t n = 0;
    for (const char *p = line; *p != '\0' && n < sizeof(buf) - 1; p++)
    {
        if (*p == '\r' || *p == '\n')
        {
            break;
        }
        buf[n++] = *p;
    }
    buf[n] = '\0';

    /* Strip the '%' comment (not inside a quoted string). '##c' character
     * constants are skipped whole, otherwise a construct such as
     * "AAA -##'" would toggle the quote state and swallow the rest of the
     * line (TSS2 OPIII does exactly this).                                */
    bool inq = false;
    for (size_t i = 0; i < n; i++)
    {
        if (buf[i] == '#' && i + 2 < n)
        {
            i += 2; /* skip '#' and the two literal characters after it */
            continue;
        }
        if (buf[i] == '\'')
        {
            inq = !inq;
        }
        else if (buf[i] == '%' && !inq)
        {
            buf[i] = '\0';
            n = i;
            break;
        }
    }

    /* skip empties */
    char *s = buf;
    while (*s != '\0' && isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == '\0')
    {
        return;
    }

    /* NOTE: ')LINE' is deliberately NOT handled before the library-mark
     * test below. It is an ordinary command and must be skipped inside a
     * false conditional region like any other. TSS5:1980 opens "TSBIN and
     * TSS5:1981 is a ")LINE" that belongs to the TSBIN (binary tape) build
     * only; executing it unconditionally ends TSS5 nine hundred lines
     * early AND leaves the conditional switched off, which then silently
     * swallows the ")LIST" at the end of ASSYSA. The archived ASYMB dump
     * contains the symbols defined after that point, so the original MAC
     * plainly assembled straight past it.                                */

    /* NOTE on '&L': '&' is not a MAC operator or command character, so a
     * line such as "&L" is parsed as an ordinary expression statement and
     * therefore EMITS ONE WORD. This is not a guess: golden ASYMB gives
     * LEV7=004176 while ECH02=004165 with an 8-word data line between them,
     * i.e. the original build advanced one extra word exactly where the
     * "&L" on TSS1:1796 sits. Earlier "&L" lines (TSS1:61, TSS1:177) are
     * immediately followed by "0/" and "20/", which reset the location
     * counter, which is why they leave no trace in the symbol values.
     * So: do NOT skip these lines - let them fall through to the
     * expression path below.                                              */

    /* library-mark line:  "EXPR  (open)  or  "  (reset) */
    if (s[0] == '"')
    {
        char *expr = s + 1;
        while (*expr != '\0' && isspace((unsigned char)*expr))
        {
            expr++;
        }
        if (*expr == '\0')
        {
            st->cond_active = true; /* bare '"' re-enables */
        }
        else
        {
            st->cond_active = eval_mark(st, expr);
        }
        return;
    }

    /* if inside a false conditional region, skip everything but '"' lines  */
    if (!st->cond_active)
    {
        return;
    }

    /* '@' or ')LINE': stop assembling this stream and return control to the
     * console, reporting the diagnostic count (ND-60.096.01 )LINE).       */
    if (s[0] == '@' || strncmp(s, ")LINE", 5) == 0)
    {
        st->end_of_file_seen = true;
        if (st->listing)
        {
            fprintf(st->listing, "%06o DIAGNOSTICS\n", st->errors);
        }
        return;
    }

    /* commands starting with ')' that we handle at the line level */
    if (s[0] == ')')
    {
        if (strncmp(s, ")FILL", 5) == 0)
        {
            cmd_fill(st);
            return;
        }
        if (strncmp(s, ")KILL", 5) == 0)
        {
            cmd_kill(st, s + 5);
            return;
        }
        if (strncmp(s, ")PCL", 4) == 0)
        {
            cmd_pcl(st, s + 4);
            return;
        }
        if (strncmp(s, ")LIST", 5) == 0)
        {
            /* )LIST goes to the OBJECT stream (ND-60.096.01 sec 4.2.3.3:
             * "outputs the symbols ... to the object stream device"), which
             * is why ASSYSA writes ASYMB:SYMB with ")9ASSM TSS5,LIST5,
             * ASYMB:SYMB" followed by ")LIST".                            */
            if (st->object != NULL)
            {
                mac_list_symbols(st, st->object);
            }
            return;
        }
        if (strncmp(s, ")BPUN", 5) == 0)
        {
            /* )BPUN SYM [SYM2]: absolute binary dump of the '<' interval to
             * the object stream, with leader + bootstrap + checksum. The
             * first symbol is the start address.                          */
            char nm[64];
            const char *ap = s + 5;
            uint16_t entry = 0;
            if (next_word(&ap, nm, sizeof(nm)))
            {
                mac_sym *sy = sym_find(st, nm);
                if (sy != NULL && sy->defined)
                {
                    entry = sy->value;
                }
                else
                {
                    mac_err(st, ")BPUN symbol is not defined:", nm);
                    return;
                }
            }
            mac_write_bpun_range(st, st->object, st->ilow, st->ihigh, entry);
            return;
        }
        if (strncmp(s, ")9READ", 6) == 0)
        {
            char nm[256];
            const char *ap = s + 6;
            if (next_word(&ap, nm, sizeof(nm)) && !mac_read_bpun(st, nm))
            {
                mac_err(st, ")9READ failed for:", nm);
            }
            return;
        }
        FILE *lst = st->listing ? st->listing : stdout;

        /* write-mode switches and the )WRITE symbol dump */
        if (strncmp(s, ")WRTM", 5) == 0)
        {
            st->write_mode = true;
            return;
        }
        if (strncmp(s, ")NWRT", 5) == 0)
        {
            st->write_mode = false;
            return;
        }
        if (strncmp(s, ")WRITE", 6) == 0)
        {
            cmd_write(st, s + 6, lst);
            return;
        }
        if (strncmp(s, ")WRUS", 5) == 0)
        {
            cmd_wrus(st, lst);
            return;
        }
        if (strncmp(s, ")WLOC", 5) == 0)
        {
            cmd_wloc(st, lst);
            return;
        }
        if (strncmp(s, ")WMNE", 5) == 0)
        {
            cmd_wmne(st, lst);
            return;
        }
        if (strncmp(s, ")9SET", 5) == 0)
        {
            cmd_9set(st, s + 5);
            return;
        }
        if (strncmp(s, ")ZERO", 5) == 0)
        {
            cmd_zero(st, s + 5);
            return;
        }
        if (strncmp(s, ")CHANGE", 7) == 0)
        {
            cmd_change(st);
            return;
        }
        if (strncmp(s, ")9MOVE", 6) == 0)
        {
            /* FMAC/MACF block-copy of assembled words - drives the "MACF
             * OVERX overlay staging (see cmd_9move + TSS-ARCHITECTURE.md (overlay chapter)). */
            cmd_9move(st, s + 6);
            return;
        }
        if (strncmp(s, ")CORE", 5) == 0)
        {
            cmd_core(st, lst);
            return;
        }
        if (strncmp(s, ")PRINT", 6) == 0)
        {
            cmd_print(st, lst);
            return;
        }
        if (strncmp(s, ")PUNCH", 6) == 0)
        {
            /* [VERIFIED manual line 2394] )PUNCH "produces output similar to
             * that of )PRINT, but the output goes to the file associated
             * with the OBJECT stream. The format ... often called octal dump
             * is suitable for loading using the NORD-10's automatic read
             * mode." So it is the same octal-text dump as )PRINT, only the
             * destination differs: object stream, not list stream. (The
             * MAC-C-STUB-INVENTORY note calling for a *binary* punch is
             * incorrect - the manual specifies an octal dump.) A dummy
             * object stream (NULL) discards the output.                     */
            if (st->object != NULL)
            {
                cmd_print(st, st->object);
            }
            return;
        }
        if (strncmp(s, ")9ASCI", 6) == 0)
        {
            /* [VERIFIED manual line 2554] )9ASCI dumps memory "on the file
             * connected to the LIST stream" as ASCII (two chars per word)
             * over the '<' interval. So the list stream is correct here; the
             * older mac.h stream-table comment that grouped )9ASCI with the
             * object stream was wrong and has been corrected.               */
            cmd_9asci(st, lst);
            return;
        }
        if (strncmp(s, ")9MSG", 5) == 0)
        {
            const char *m = s + 5;
            while (*m == ' ')
            {
                m++;
            }
            fprintf(lst, "%s\n", m);
            return;
        }
        if (strncmp(s, ")CLEAR", 6) == 0)
        {
            /* )CLEAR also zeroes the B and X location counters */
            st->bcounter = 0;
            st->xcounter = 0;
            return;
        }
        if (strncmp(s, ")9PARI", 6) == 0)
        {
            /* [VERIFIED manual line 2550] )9PARI toggles odd-parity checking
             * of ASCII characters in the SOURCE INPUT stream. This is an
             * explicit, JUSTIFIED no-op here: mac-c reads source from host
             * text files that carry no 8th parity bit, so there is no parity
             * to check and nothing to reject. We still model the switch state
             * in st->parity_check (initial ON, matching the manual) and echo
             * it via MACTRACE so the flag is observable rather than a silent
             * set-but-never-read - but no host input is ever rejected on
             * parity, because host text has none.                          */
            st->parity_check = !st->parity_check;
            return;
        }
        if (strncmp(s, ")9LITR", 6) == 0)
        {
            st->dup_literals = !st->dup_literals;
            return;
        }
        if (strncmp(s, ")SETSM", 6) == 0)
        {
            /* [VERIFIED manual sec at line 3334-3347] )SETSM/)RESSM select
             * SYMBOLIC vs octal printout for the disassembler option: they
             * only change how )PRINT / the ':' examine command FORMAT memory
             * words (as disassembled instructions vs raw octal). They have
             * NO effect on the assembled image or the symbol table. mac-c is
             * a batch assembler that does not implement the interactive
             * symbolic-disassembly printout, so this is a JUSTIFIED no-op; we
             * record the mode in st->symbolic_out so the state is observable
             * (and a future symbolic )PRINT could honour it) rather than
             * silently dropping the command.                               */
            st->symbolic_out = true;
            return;
        }
        if (strncmp(s, ")RESSM", 6) == 0)
        {
            /* [VERIFIED manual line 3334] restore octal printout mode - the
             * counterpart of )SETSM; same JUSTIFIED no-op reasoning.        */
            st->symbolic_out = false;
            return;
        }
        if (strncmp(s, ")9EXIT", 6) == 0 || strncmp(s, ")9TSS", 5) == 0)
        {
            /* )9TSS is a TSS-restoration alias of )9EXIT: in the real
             * MAC.BPUN both dispatch to the same handler (MON 0 LEAVE).   */
            st->end_of_file_seen = true;
            return;
        }
        if (strncmp(s, ")9ASSM", 6) == 0)
        {
            /* ")9ASSM source,list,object" - up to three comma separated
             * stream arguments. An omitted argument keeps the current
             * stream, "0" selects the dummy device, and a name in double
             * quotes is created (ND-60.096.01 )9ASSM).                    */
            char arg[3][256];
            arg[0][0] = arg[1][0] = arg[2][0] = '\0';
            const char *p = s + 6;
            for (int k = 0; k < 3; k++)
            {
                while (*p == ' ' || *p == '\t')
                {
                    p++;
                }
                size_t j = 0;
                while (*p != '\0' && *p != ',' && j + 1 < sizeof(arg[0]))
                {
                    if (*p != '"')
                    {
                        arg[k][j++] = *p;
                    }
                    p++;
                }
                while (j > 0 && isspace((unsigned char)arg[k][j - 1]))
                {
                    j--;
                }
                arg[k][j] = '\0';
                if (*p == ',')
                {
                    p++;
                }
                else
                {
                    break;
                }
            }

            /* list and object streams first, so they are in place while the
             * new source stream is assembled                              */
            if (arg[1][0] != '\0')
            {
                bool owned;
                st->listing = open_stream_arg(st, arg[1], &owned,
                                              st->listing, st->own_listing,
                                              "SYMB");
                st->own_listing = owned;
            }
            if (arg[2][0] != '\0')
            {
                bool owned;
                st->object = open_stream_arg(st, arg[2], &owned,
                                             st->object, st->own_object,
                                             "BRF");
                st->own_object = owned;
            }

            const char *fname = arg[0];
            if (fname[0] == '\0')
            {
                return; /* ")9ASSM" or ")9ASSM ,0" - no new source stream */
            }
            if (st->include_depth > 8)
            {
                mac_err(st, ")9ASSM nested too deeply:", fname);
                return;
            }
            /* save the caller's stream position markers */
            const char *save_file = st->cur_file;
            int save_line = st->cur_line;
            bool save_eof = st->end_of_file_seen;
            st->include_depth++;
            if (!mac_assemble_file(st, fname))
            {
                mac_err(st, ")9ASSM cannot open:", fname);
            }
            st->include_depth--;
            st->cur_file = save_file;
            st->cur_line = save_line;
            st->end_of_file_seen = save_eof;
            return;
        }
        if (strncmp(s, ")MCDEF", 6) == 0)
        {
            macro_begin(st, s + 6);
            return;
        }
        /* NOTE: there was once a second, unreachable ")9ASSM" handler here
         * with a "TODO nested include" log. It was dead code - the real
         * )9ASSM handler above (which fully implements nested includes)
         * matches first and returns, so control never reached it. Removed.  */

        /* )SOVER and )8DUMP: INTENTIONAL, documented no-ops.
         * [VERIFIED docs/TSS-ARCHITECTURE.md (overlay chapter) HEADLINE + sec 1.3 + sec 6]
         * )SOVER exists only inside the "NMACF variant of the OVERX macro
         * (TSS3.SYMB:78); every golden build (ASSYSA/ASSYSB/DRUM) sets the
         * MACF mark, so "NMACF is FALSE and )SOVER is never assembled at all.
         * )8DUMP appears only in the "TSBIN binary-tape bootstrap region
         * (TSS5.SYMB:1987,1992) and in the TDUMP utility; TSBIN is FALSE in
         * these builds. Both are )SYMBOL-style invocations of assembled
         * ND-100 routines (they would run SOVER/8DUMP machine code to write
         * the overlay/core image to disc). Reproducing them would require
         * ND-100 EXECUTION, which this host assembler deliberately does not
         * do - and it is unnecessary: the run-time disc contract is
         * reproduced directly by )9MOVE (image staging) + the CDC-disc image
         * writer (mac_write_cdc_disc), per TSS-ARCHITECTURE.md (overlay chapter) sec 7. So on
         * the builds mac-c targets these commands correctly do nothing.
         * We do NOT fake ND-100 execution.                                  */

        /* Any other unhandled ')' command: accepted and ignored. The TSS
         * corpus uses none besides the documented no-ops above; there is no
         * image or symbol-table effect.                                     */
        return;
    }

    /* split remaining line on ';' into statements */
    char *start = s;
    inq = false;
    for (char *p = s;; p++)
    {
        /* skip '#ab' whole so that "##'" and "## ;" do not disturb the
         * quote state or split the statement                              */
        if (p[0] == '#' && p[1] != '\0' && p[2] != '\0')
        {
            p += 2;
            continue;
        }
        if (*p == '\'')
        {
            inq = !inq;
        }
        if ((*p == ';' && !inq) || *p == '\0')
        {
            char save = *p;
            *p = '\0';
            char tmp[512];
            strncpy(tmp, start, sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = '\0';
            assemble_stmt(st, tmp);
            if (save == '\0')
            {
                break;
            }
            start = p + 1;
        }
    }
}


/* ---------------------------------------------------------------------- */
/* -L address listing                                                      */
/*                                                                         */
/* One row per source line:                                                */
/*                                                                         */
/*     file:line   address  words...   source                              */
/*                                                                         */
/* Real MAC has no such mode - this is a mac-c debugging aid. It exists    */
/* because nothing else in the project maps a source line to the address   */
/* it assembled to: the archived LIST*.SYMB streams carry diagnostics      */
/* only, and hand-derived addresses have been wrong every time they were   */
/* attempted (see docs/HANDOFF-2026-07-26.md).                             */
/* ---------------------------------------------------------------------- */

/* Trim CR/LF and trailing blanks so the source column stays aligned.      */
static void al_trim(const char *in, char *out, size_t cap)
{
    size_t n = 0;
    for (const char *p = in; *p != '\0' && n + 1 < cap; p++)
    {
        if (*p == '\r' || *p == '\n')
        {
            break;
        }
        out[n++] = *p;
    }
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t'))
    {
        n--;
    }
    out[n] = '\0';
}

/* Basename of the current source file, so rows stay narrow.               */
static const char *al_basename(const char *path)
{
    if (path == NULL)
    {
        return "<console>";
    }
    const char *b = path;
    for (const char *p = path; *p != '\0'; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            b = p + 1;
        }
    }
    return b;
}

#define AL_WORDS_PER_ROW 6

/* A buffered listing row. Rows are accumulated during assembly and written
 * by asm_list_flush() once every fixup has been applied, so the words
 * column shows what the image ACTUALLY contains. Printing during assembly
 * would show forward references unpatched - "STA IDXA" would read 004000
 * with a zero displacement - which is precisely the class of misreading
 * this listing exists to prevent.                                         */
typedef struct al_row
{
    char            pos[64];
    char            src[256];
    uint16_t        addr[MAC_ASM_LIST_MAX];
    int             n;      /**< word count, or -1 if the line overflowed  */
    uint16_t        loc;    /**< location counter when the line started    */
    int             depth;  /**< mac_line recursion depth                  */
    struct al_row  *next;
} al_row;

static al_row *g_al_head = NULL;
static al_row *g_al_tail = NULL;

void asm_list_flush(mac_state *st)
{
    for (al_row *r = g_al_head; r != NULL; )
    {
        al_row *next = r->next;
        if (st->asm_list != NULL)
        {
            const char *ind = (r->depth > 0) ? "+ " : "";
            if (r->n < 0)
            {
                fprintf(st->asm_list, "%-26s %06o  %-*s %s%s\n", r->pos,
                        r->loc, AL_WORDS_PER_ROW * 7 - 1, "... (>64 words)",
                        ind, r->src);
            }
            else if (r->n == 0)
            {
                fprintf(st->asm_list, "%-26s %06o  %-*s %s%s\n", r->pos,
                        r->loc, AL_WORDS_PER_ROW * 7 - 1, "", ind, r->src);
            }
            else
            {
                for (int i = 0; i < r->n; i += AL_WORDS_PER_ROW)
                {
                    char words[AL_WORDS_PER_ROW * 7 + 1];
                    size_t w = 0;
                    for (int j = i; j < r->n && j < i + AL_WORDS_PER_ROW; j++)
                    {
                        w += (size_t)snprintf(words + w, sizeof(words) - w,
                                              "%06o ", st->mem[r->addr[j]]);
                    }
                    if (w > 0) { words[w - 1] = '\0'; } else { words[0] = '\0'; }

                    if (i == 0)
                    {
                        fprintf(st->asm_list, "%-26s %06o  %-*s %s%s\n",
                                r->pos, r->addr[0], AL_WORDS_PER_ROW * 7 - 1,
                                words, ind, r->src);
                    }
                    else
                    {
                        fprintf(st->asm_list, "%-26s %06o  %s\n", "",
                                r->addr[i], words);
                    }
                }
            }
        }
        free(r);
        r = next;
    }
    g_al_head = NULL;
    g_al_tail = NULL;
}

void mac_line(mac_state *st, const char *line)
{
    if (st->asm_list == NULL)
    {
        mac_line_inner(st, line);
        return;
    }

    /* A nested mac_line (macro expansion, or a line read by )9ASSM from
     * inside another line) must not clobber the outer line's word list.
     * Save and restore it around the recursion; the inner line prints its
     * own row, indented by depth.                                         */
    uint16_t save_addr[MAC_ASM_LIST_MAX];
    uint16_t save_word[MAC_ASM_LIST_MAX];
    int      save_n     = st->al_n;
    int      save_count = (save_n > 0) ? save_n : 0;
    if (save_count > 0)
    {
        memcpy(save_addr, st->al_addr, (size_t)save_count * sizeof(uint16_t));
        memcpy(save_word, st->al_word, (size_t)save_count * sizeof(uint16_t));
    }

    const char *file = al_basename(st->cur_file);
    int         lno  = st->cur_line + 1; /* mac_line_inner bumps it first  */
    int         depth = st->al_depth;
    char        src[256];
    al_trim(line, src, sizeof(src));

    uint16_t loc_before = st->loc;
    st->al_n = 0;
    st->al_depth = depth + 1;

    mac_line_inner(st, line);

    st->al_depth = depth;
    int n = st->al_n;

    /* Buffer the row; asm_list_flush() prints it after fixups are applied. */
    if (!(n == 0 && src[0] == '\0'))
    {
        al_row *r = (al_row *)malloc(sizeof(al_row));
        if (r != NULL)
        {
            snprintf(r->pos, sizeof(r->pos), "%s:%d", file, lno);
            snprintf(r->src, sizeof(r->src), "%s", src);
            r->n = n;
            r->loc = loc_before;
            r->depth = depth;
            r->next = NULL;
            int keep = (n > 0) ? n : 0;
            for (int i = 0; i < keep; i++)
            {
                r->addr[i] = st->al_addr[i];
            }
            if (g_al_tail == NULL) { g_al_head = r; }
            else                   { g_al_tail->next = r; }
            g_al_tail = r;
        }
    }

    /* restore the enclosing line's accumulation */
    if (save_count > 0)
    {
        memcpy(st->al_addr, save_addr, (size_t)save_count * sizeof(uint16_t));
        memcpy(st->al_word, save_word, (size_t)save_count * sizeof(uint16_t));
    }
    st->al_n = save_n;
}
