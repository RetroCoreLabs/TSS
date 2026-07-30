/* MAC-C - expression evaluation and the MRI operand evaluator
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* expression evaluation                                                   */
/* ---------------------------------------------------------------------- */
/* Returns the 16-bit value. Sets *undef_sym to the FIRST undefined symbol
 * encountered (so the caller can register a fixup) and *is_addr if the
 * value is an address (label / '*'), which turns MRI operands P-relative. */

/* '#ab' is MAC's character-pair constant: '#' followed by exactly TWO
 * characters yields the word (a << 8) | b, i.e. the two characters packed
 * as they would be by a text string.
 *
 * This single rule explains every usage in the corpus:
 *   TSS2 "SINQ, #SY; #ST; #EM"  -> data words 'SY', 'ST', 'EM'
 *   "SAA ##?"                   -> ('#'<<8)|'?' ; SAA keeps the low byte,
 *                                  so the effect is the character '?'
 *   "SAT ##/", "AAA ##0", "SAA ## " (space), "SAA ##\", "SAT ##,"
 * The original 8-bit bytes of TSS2:843 were checked to confirm the single
 * '#' there is genuine and not a parity-stripping artefact.
 * Returns the char after a '#ab' sequence, or NULL if p is not one.       */
const char *skip_char_const(const char *p)
{
    if (p[0] == '#' && p[1] != '\0' && p[2] != '\0')
    {
        return p + 3;
    }
    return NULL;
}

uint16_t eval_term(mac_state *st, const char *tok, mac_sym **undef_sym,
                          bool *is_addr)
{
    /* '*' = current location counter (an address). '*N' is the documented
     * shorthand for '*+N' - the disassembler prints exactly this form
     * ("400/ LDA *003" in ND-60.096.01 sec 4.2.7), and TSS writes
     * "JMP *3" / "JMP *2" to skip over following instructions.           */
    if (tok[0] == '*')
    {
        *is_addr = true;
        if (tok[1] == '\0')
        {
            return st->loc;
        }
        bool all_digits = true;
        for (int i = 1; tok[i] != '\0'; i++)
        {
            if (!isdigit((unsigned char)tok[i]))
            {
                all_digits = false;
                break;
            }
        }
        if (all_digits)
        {
            return (uint16_t)(st->loc +
                              (uint16_t)strtol(tok + 1, NULL, 8));
        }
        /* not '*' or '*N': fall through and treat the whole token as a
         * symbol so the undefined-symbol machinery reports it            */
        *is_addr = false;
    }
    /* '#ab' character-pair constant: (a << 8) | b */
    if (tok[0] == '#' && tok[1] != '\0' && tok[2] != '\0')
    {
        return (uint16_t)(((unsigned char)tok[1] << 8) |
                          (unsigned char)tok[2]);
    }
    /* Numeric literal vs digit-leading SYMBOL. MAC symbols may begin with a
     * digit (TSS uses 8LP, 9TTI, 9PPT, 9ASSM, and the built-in 2OR3), so a
     * token is a number ONLY if every character is a digit, with an optional
     * trailing '.' meaning decimal. Otherwise it is a symbol.              */
    if (isdigit((unsigned char)tok[0]))
    {
        size_t n = strlen(tok);
        bool decimal = (n > 0 && tok[n - 1] == '.');
        size_t digits = decimal ? n - 1 : n;
        bool all_digits = (digits > 0);
        for (size_t i = 0; i < digits; i++)
        {
            unsigned char c = (unsigned char)tok[i];
            /* The token is a NUMBER only if every character is valid IN ITS
             * RADIX. MAC numbers are octal by default (0-7); a trailing '.'
             * makes them decimal (0-9). A digit-leading token that is not a
             * valid number in its radix is a SYMBOL - TSS defines all-digit
             * pointer literals like 9377/9177/9427 (the 377 byte-mask etc.).
             * BUG (fixed): using isdigit() here accepted 8/9 as "digits", so
             * `AND 9377` was classified as a number and strtol(...,8) stopped
             * at the '9' and returned 0 -> the instruction silently became
             * `AND 0` (AND with itself), zeroing every console character in
             * WBUF/RBUF and hanging LOGON. Octal digits are 0-7 ONLY. */
            bool valid = decimal ? isdigit(c) : (c >= '0' && c <= '7');
            if (!valid)
            {
                all_digits = false;
                break;
            }
        }
        if (all_digits)
        {
            return (uint16_t)strtol(tok, NULL, decimal ? 10 : 8);
        }
        /* else: fall through and treat as a symbol */
    }
    /* symbol */
    mac_sym *s = sym_intern(st, tok);
    if (!s->defined)
    {
        if (*undef_sym == NULL)
        {
            *undef_sym = s;
        }
        return 0;
    }
    if (s->address)
    {
        *is_addr = true;
    }
    return s->value;
}

/* Evaluate a full expression string (already comment-stripped, trimmed).
 * Handles + - terms and the '@' shift operator. '(' literals and quoted
 * strings are handled by the statement layer, not here.                   */
uint16_t eval_expr(mac_state *st, const char *expr, mac_sym **undef_sym,
                          bool *is_addr)
{
    uint16_t acc = 0;
    int sign = 1;
    char tok[64];
    /* fresh undefined-use list for this expression (see mac_state.eu)     */
    st->neu = 0;
    int ti = 0;
    bool have_shift = false;
    uint16_t shift_amt = 0;
    /* SHR - "Shift right, gives negative shift counter" (ND-60.096.01 sec
     * 2.3.8). When the SHR shift modifier has just been summed, the shift-
     * count term that FOLLOWS it must be SUBTRACTED (see the detailed note
     * at the term-add below).                                              */
    bool shr_pending = false;

    for (const char *p = expr;; p++)
    {
        char c = *p;
        /* A blank separates terms just as '+' does: MAC statements are sums
         * whose "elements [are] separated by a single space, a plus sign or
         * a minus sign" (ND-60.096.01 sec 2.5). Treating a space as mere
         * padding would fuse "COPY SA DT" into one bogus symbol.          */
        /* a '#ab' constant is taken whole: its second character may be a
         * space or ';' which must not be read as a separator             */
        if (c == '#' && p[1] != '\0' && p[2] != '\0' && ti == 0)
        {
            tok[ti++] = p[0];
            tok[ti++] = p[1];
            tok[ti++] = p[2];
            p += 2;
            continue;
        }
        if (c == '+' || c == '-' || c == '@' || c == '\0' ||
            isspace((unsigned char)c))
        {
            if (ti > 0)
            {
                tok[ti] = '\0';
                bool taddr = false;
                mac_sym *tu = NULL;
                uint16_t v = eval_term(st, tok, &tu, &taddr);
                if (taddr)
                {
                    *is_addr = true;
                }
                if (have_shift)
                {
                    acc = (uint16_t)(acc << (shift_amt & 0x0F));
                    have_shift = false;
                }
                /* SHR - "Shift right, gives negative shift counter" - inverts
                 * the sign of the shift-count term that FOLLOWS it. In a NORD
                 * shift instruction (SHT/SHD/SHA/SAD) the low 7 bits (0-6) are
                 * a SIGNED shift counter, so a right shift is a NEGATIVE count.
                 * MAC's permanent-symbol value for SHR is 0200 (verified in
                 * MAC.BPUN's permsym table at 0xE9B3), which is bit-identical
                 * to SHD's register-select bit - so a plain additive SHR
                 * wrongly produced e.g. "SHA SHR 6" = 0154606 = "SAD 6", a
                 * LEFT shift of A+D. The correct behaviour (ND-60.096.01 sec
                 * 2.3.8: "SHR - Shift right, gives negative shift counter.
                 * Note that SHR must precede the specified shift counter") is:
                 * SHR still contributes its own 0200, then the NEXT numeric
                 * term is SUBTRACTED, forming a 7-bit two's-complement
                 * negative counter:
                 *   SHA SHR 6      = 0154400 + 0200 - 6        = 0154572
                 *   SHA ZIN SHR 1  = 0154400 + 02000 + 0200 -1 = 0156577
                 *   SAD ZIN SHR 20 = 0154600 + 02000 + 0200-020= 0156760 (DKADR)
                 * ROT/ZIN/LIN (01000/02000/03000, the shift-TYPE bits 9-10)
                 * are genuinely additive and are NOT touched here.           */
                int eff_sign = shr_pending ? -sign : sign;
                shr_pending = false;
                acc = (uint16_t)(acc + eff_sign * v);
                if (tu != NULL)
                {
                    /* one undefined-table entry PER USE, carrying the sign
                     * of its term (ND-60.096.01 lines 1519 and 1949)      */
                    if (*undef_sym == NULL)
                    {
                        *undef_sym = tu;
                    }
                    if (st->neu < MAC_EXPR_MAX_UNDEF)
                    {
                        st->eu[st->neu].sym = tu;
                        st->eu[st->neu].sign = eff_sign;
                        st->neu++;
                    }
                    else
                    {
                        mac_err(st, "too many undefined symbols in one "
                                    "expression:", tok);
                    }
                }
                if (strcmp(tok, "SHR") == 0)
                {
                    shr_pending = true; /* negate the following count term */
                }
                ti = 0;
                /* a blank does not change the sign; '+'/'-' below do */
                if (isspace((unsigned char)c))
                {
                    sign = 1;
                }
            }
            if (c == '\0')
            {
                break;
            }
            if (c == '+')
            {
                sign = 1;
            }
            else if (c == '-')
            {
                sign = -1;
            }
            else if (c == '@') /* shift accumulator left by the next term */
            {
                /* the value after @ is the shift count; grab it inline */
                const char *q = p + 1;
                char nbuf[16];
                int ni = 0;
                while (*q != '\0' && ni < 15 &&
                       (isalnum((unsigned char)*q) || *q == '*'))
                {
                    nbuf[ni++] = *q++;
                }
                nbuf[ni] = '\0';
                bool a2 = false;
                shift_amt = eval_term(st, nbuf, undef_sym, &a2);
                have_shift = true;
                p = q - 1;
            }
        }
        else
        {
            if (ti < 63)
            {
                tok[ti++] = c;
            }
        }
    }
    return acc;
}

/* ---------------------------------------------------------------------- */
/* memory-reference operand evaluator                                      */
/* ---------------------------------------------------------------------- */
/* Splits an MRI operand into (mode_bits, displacement). Mode bits come
 * ONLY from the mode symbols ,X (02000) I (01000) ,B (0400); everything
 * else contributes to the displacement. This separation is REQUIRED: the
 * final word is opcode + mode + (disp & 0377), and a negative displacement
 * must be masked to 8 bits BEFORE the mode bits are added, else the sign
 * extension corrupts the mode field (ND-60.096.01 sec 2.6, example 1947). */

/* Intern a literal '(EXPR': dedup identical literals to one cell (the real
 * MAC default; !9LITR duplication mode is not implemented). Returns index. */
int literal_intern(mac_state *st, const char *inner)
{
    /* resolve inner expression; if it is a defined value, store value; if a
     * single undefined symbol (+const), store the symbol + extra constant.  */
    mac_sym *u = NULL;
    bool a = false;
    uint16_t v = eval_expr(st, inner, &u, &a);

    /* )9LITR on: every use allocates its own cell, so skip the search
     * ("duplication of literals may appear to advantage when debugging").  */
    for (int i = 0; !st->dup_literals && i < st->nlits; i++)
    {
        mac_literal *L = &st->lits[i];
        if (L->placed)
        {
            continue; /* already dumped; a later use needs a fresh cell */
        }
        if (u == NULL && L->resolved && L->value == v)
        {
            return i;
        }
        if (u != NULL && !L->resolved && L->extra == v &&
            strcmp(L->sym, u->name) == 0)
        {
            return i;
        }
    }
    if (st->nlits >= MAC_MAX_LITERALS)
    {
        mac_err(st, "literal pool overflow", NULL);
        return 0;
    }
    mac_literal *L = &st->lits[st->nlits];
    memset(L, 0, sizeof(*L));
    if (u == NULL)
    {
        L->resolved = true;
        L->value = v;
    }
    else
    {
        L->resolved = false;
        /* L->sym is MAC_SYM_LEN+1 bytes; copy at most MAC_SYM_LEN and
         * terminate explicitly (u->name is already 5-char normalised).     */
        memcpy(L->sym, u->name, MAC_SYM_LEN);
        L->sym[MAC_SYM_LEN] = '\0';
        L->extra = v; /* constant part accompanying the symbol */
    }
    return st->nlits++;
}

bool is_mode_symbol(const char *tok, uint16_t *bit)
{
    if (strcmp(tok, ",X") == 0) { *bit = 02000; return true; }
    if (strcmp(tok, ",B") == 0) { *bit = 00400; return true; }
    if (strcmp(tok, "I") == 0)  { *bit = 01000; return true; }
    return false;
}

void eval_operand(mac_state *st, const char *expr, mac_operand *out)
{
    out->mode = 0;
    out->disp = 0;
    out->is_addr = false;
    out->undef = NULL;
    out->literal = -1;

    /* literal operand: '(EXPR' (rest of the string is the literal body). It
     * may be preceded by mode symbols, e.g. "I (INBT". Handle the '(' by
     * splitting: everything before it is normal mode/disp terms.           */
    const char *lp = strchr(expr, '(');
    char head[256];
    if (lp != NULL)
    {
        size_t hn = (size_t)(lp - expr);
        if (hn >= sizeof(head)) hn = sizeof(head) - 1;
        memcpy(head, expr, hn);
        head[hn] = '\0';
        out->literal = literal_intern(st, lp + 1);
        expr = head; /* evaluate only the mode/disp part before '('         */
    }

    int sign = 1;
    char tok[64];
    int ti = 0;

    /* flush the accumulated token as either a mode symbol or a value term */
#define FLUSH_TOK()                                                         \
    do {                                                                    \
        if (ti > 0) {                                                       \
            tok[ti] = '\0';                                                 \
            uint16_t mbit;                                                  \
            if (is_mode_symbol(tok, &mbit)) {                              \
                out->mode |= mbit;                                          \
            } else {                                                        \
                bool taddr = false;                                        \
                uint16_t v = eval_term(st, tok, &out->undef, &taddr);      \
                if (taddr) { out->is_addr = true; }                        \
                out->disp = (uint16_t)(out->disp + sign * v);              \
            }                                                               \
            ti = 0;                                                        \
        }                                                                   \
    } while (0)

    for (const char *p = expr;; p++)
    {
        char c = *p;
        /* '#ab' constants are taken whole (see eval_expr)                 */
        if (c == '#' && p[1] != '\0' && p[2] != '\0' && ti == 0)
        {
            tok[ti++] = p[0];
            tok[ti++] = p[1];
            tok[ti++] = p[2];
            p += 2;
            continue;
        }
        if (c == ',')
        {
            /* a comma starts a mode symbol (,B / ,X): flush, then begin a
             * fresh token whose first char is the comma.                    */
            FLUSH_TOK();
            tok[ti++] = ',';
        }
        else if (c == '+' || c == '-' || c == '\0' ||
                 isspace((unsigned char)c))
        {
            FLUSH_TOK();
            if (c == '\0')
            {
                break;
            }
            if (c == '+')      { sign = 1; }
            else if (c == '-') { sign = -1; }
            /* whitespace: term boundary only, sign unchanged */
        }
        else
        {
            if (ti < 63)
            {
                tok[ti++] = c;
            }
        }
    }
#undef FLUSH_TOK
}

/* ---------------------------------------------------------------------- */
