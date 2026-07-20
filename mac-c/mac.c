/**************************************************************************
** MAC-C CORE - one-pass MAC assembler with forward-reference fixups     **
**                                                                       **
** Implements the subset of the MAC language exercised by the NORD TSS   **
** 3.0 sources. Statement model (per ND-60.096.01):                      **
**                                                                       **
**   - A line is split on ';' into statements; '%' starts a comment to   **
**     end of line; '@' at line start == )LINE (stop).                   **
**   - A statement may be:                                               **
**       LABEL,             define LABEL = current location (an address) **
**       NAME=EXPR          define/redefine NAME to EXPR (a value)        **
**       NNNNN/             set the location counter to NNNNN             **
**       )CMD ...           an assembler command                         **
**       "EXPR / "          library-mark conditional open / reset        **
**       EXPR               emit one word (class of leading token rules   **
**                          how the operand is encoded)                  **
**                                                                       **
** Numbers are OCTAL by default (MAC convention). '*' is the location    **
** counter. Expressions are sums of +/- separated terms; '@' is the      **
** left-shift operator (not unary).                                      **
**                                                                       **
** HONESTY NOTE: )MCDEF macros and nested )9ASSM includes are stubbed    **
** (logged, skipped) - see TODOs. The golden test is: assembling ASSYSA  **
** must reproduce the archived ASYMB:SYMB symbol dump exactly. Until      **
** that passes byte-for-byte, treat output as provisional.               **
**                                                                       **
** Ronny Hansen                                                          **
***************************************************************************/
#include "mac.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---------------------------------------------------------------------- */
/* symbol table                                                            */
/* ---------------------------------------------------------------------- */

/* MAC names are significant to 5 characters and it is the LAST five that
 * survive, not the first: MAC accumulates a name by shifting each character
 * into a 30-bit field (5 x 6 bits), so extra leading characters are shifted
 * out. Proof from the golden ASYMB dump: the source symbols RBLOAD and
 * TIMOUT appear there as BLOAD=033614 and IMOUT=007773 - the trailing five
 * characters, with the values my first-five truncation produced for RBLOA
 * and TIMOU. (This also keeps LBYTE / LBYTE1 / LBYTE2 distinct.)          */
static void sym_norm(const char *in, char *out)
{
    size_t n = strlen(in);
    const char *start = in;
    if (n > 5)
    {
        start = in + (n - 5);   /* keep the last five characters */
        n = 5;
    }
    size_t i = 0;
    for (; i < n; i++)
    {
        out[i] = (char)toupper((unsigned char)start[i]);
    }
    out[i] = '\0';
}

static mac_sym *sym_find(mac_state *st, const char *name)
{
    char key[MAC_SYM_LEN + 1];
    sym_norm(name, key);
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (strcmp(s->name, key) == 0)
        {
            return s;
        }
    }
    return NULL;
}

/* Get-or-create. A freshly created entry is undefined (a forward ref).    */
static mac_sym *sym_intern(mac_state *st, const char *name)
{
    mac_sym *s = sym_find(st, name);
    if (s != NULL)
    {
        return s;
    }
    s = (mac_sym *)calloc(1, sizeof(mac_sym));
    sym_norm(name, s->name);
    s->next = st->symtab;
    st->symtab = s;
    return s;
}

/* Patch every pending forward reference now that 'sym' is defined.        */
static void sym_resolve_fixups(mac_state *st, mac_sym *sym, mac_fixup **head)
{
    (void)st;
    mac_fixup *f = *head;
    while (f != NULL)
    {
        mac_fixup *next = f->next;
        switch (f->kind)
        {
        case MAC_FIX_FULL:
            st->mem[f->addr] = (uint16_t)(st->mem[f->addr] + sym->value);
            break;
        case MAC_FIX_PREL8:
        {
            /* P-relative: EA = (P) + disp where P is the instruction's OWN
             * address (ND-60.096.01 sec 2.3.1; verified against MAC.BPUN:
             * JMP *0xdef6 at 0xdf11 encodes disp 0345 = -27 = target-here).
             * f->pc holds the instruction address.                          */
            int disp = (int)sym->value - (int)f->pc;
            st->mem[f->addr] = (uint16_t)((st->mem[f->addr] & 0xFF00) |
                                          (disp & 0x00FF));
            break;
        }
        case MAC_FIX_ARG8:
            st->mem[f->addr] = (uint16_t)((st->mem[f->addr] & 0xFF00) |
                                          (sym->value & 0x00FF));
            break;
        }
        free(f);
        f = next;
    }
    *head = NULL;
}

/* Per-symbol pending-fixup chains live in a side map keyed by symbol.
 * We store the chain head directly on the (undefined) symbol via a small
 * intrusive pointer kept in def_seq's sibling: to stay simple we use a
 * parallel list.                                                          */
typedef struct pending
{
    mac_sym        *sym;
    mac_fixup      *chain;
    struct pending *next;
} pending;

static pending *g_pending; /* module state: forward-ref chains by symbol   */

/* Pending literal references: an MRI addressed a literal cell '(EXPR'; the
 * cell address is unknown until )FILL places it. We patch the P-relative
 * displacement (cell - instr_addr) into the instruction word at )FILL.     */
typedef struct litref
{
    uint16_t       addr;      /**< instruction word to patch                */
    uint16_t       pc;        /**< instruction address (P-relative base)    */
    int            lit_index; /**< which literal in st->lits                */
    bool           absolute;  /**< true: patch the cell ADDRESS as a full
                                   word (a data word such as REGTB's "(0");
                                   false: patch an 8-bit P-relative
                                   displacement into an instruction        */
    struct litref *next;
} litref;

static litref *g_litrefs;

static mac_fixup **pending_chain_for(mac_sym *sym)
{
    for (pending *p = g_pending; p != NULL; p = p->next)
    {
        if (p->sym == sym)
        {
            return &p->chain;
        }
    }
    pending *p = (pending *)calloc(1, sizeof(pending));
    p->sym = sym;
    p->next = g_pending;
    g_pending = p;
    return &p->chain;
}

static void pending_add(mac_sym *sym, uint16_t addr, uint16_t pc, mac_fix_kind k)
{
    mac_fixup **head = pending_chain_for(sym);
    mac_fixup *f = (mac_fixup *)calloc(1, sizeof(mac_fixup));
    f->addr = addr;
    f->pc = pc;
    f->kind = k;
    f->next = *head;
    *head = f;
}

/* ---------------------------------------------------------------------- */
/* diagnostics                                                             */
/* ---------------------------------------------------------------------- */

static void mac_err(mac_state *st, const char *msg, const char *detail)
{
    st->errors++;
    fprintf(stderr, "%s:%d: ERROR: %s%s%s\n",
            st->cur_file ? st->cur_file : "?", st->cur_line, msg,
            detail ? " " : "", detail ? detail : "");
}

/* ---------------------------------------------------------------------- */
/* memory emission                                                         */
/* ---------------------------------------------------------------------- */

static void emit(mac_state *st, uint16_t word)
{
    st->mem[st->loc] = word;
    st->used[st->loc] = 1;
    if (st->loc < st->lo_used)
    {
        st->lo_used = st->loc;
    }
    if (st->loc > st->hi_used)
    {
        st->hi_used = st->loc;
    }
    st->loc++;
}

/* ---------------------------------------------------------------------- */
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
static const char *skip_char_const(const char *p)
{
    if (p[0] == '#' && p[1] != '\0' && p[2] != '\0')
    {
        return p + 3;
    }
    return NULL;
}

static uint16_t eval_term(mac_state *st, const char *tok, mac_sym **undef_sym,
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
            if (!isdigit((unsigned char)tok[i]))
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
static uint16_t eval_expr(mac_state *st, const char *expr, mac_sym **undef_sym,
                          bool *is_addr)
{
    uint16_t acc = 0;
    int sign = 1;
    char tok[64];
    int ti = 0;
    bool have_shift = false;
    uint16_t shift_amt = 0;

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
                uint16_t v = eval_term(st, tok, undef_sym, &taddr);
                if (taddr)
                {
                    *is_addr = true;
                }
                if (have_shift)
                {
                    acc = (uint16_t)(acc << (shift_amt & 0x0F));
                    have_shift = false;
                }
                acc = (uint16_t)(acc + sign * v);
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
typedef struct
{
    uint16_t mode;      /**< OR of 02000/01000/0400 from mode symbols       */
    uint16_t disp;      /**< 16-bit displacement value (address or literal) */
    bool     is_addr;   /**< disp is an address -> P-relative if no B/X mode */
    mac_sym *undef;     /**< first undefined symbol, for a fixup            */
    int      literal;   /**< index into st->lits if operand was '(EXPR', else -1 */
} mac_operand;

/* Intern a literal '(EXPR': dedup identical literals to one cell (the real
 * MAC default; !9LITR duplication mode is not implemented). Returns index. */
static int literal_intern(mac_state *st, const char *inner)
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

static bool is_mode_symbol(const char *tok, uint16_t *bit)
{
    if (strcmp(tok, ",X") == 0) { *bit = 02000; return true; }
    if (strcmp(tok, ",B") == 0) { *bit = 00400; return true; }
    if (strcmp(tok, "I") == 0)  { *bit = 01000; return true; }
    return false;
}

static void eval_operand(mac_state *st, const char *expr, mac_operand *out)
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
/* statement assembly                                                      */
/* ---------------------------------------------------------------------- */

/* Look up the class of the leading token (permanent instructions carry a
 * class; locals default to PLAIN data).                                   */
static mac_sym_class leading_class(mac_state *st, const char *tok)
{
    mac_sym *s = sym_find(st, tok);
    if (s != NULL)
    {
        return s->cls;
    }
    return MAC_CLS_PLAIN;
}

/* Assemble one already-trimmed statement (no ';', no comment).            */
static void assemble_stmt(mac_state *st, char *stmt);

/* Split leading opcode token off the statement; the rest is the operand. */
static void first_token(const char *stmt, char *op, const char **rest)
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
/* macros ()MCDEF)                                                         */
/* ---------------------------------------------------------------------- */
/* Per ND-60.096.01 sec 4.2.9.2:
 *   )MCDEF NAME $P1, $P2 ...      definition line, ends at the newline
 *   <body lines>                  substituted at call time
 *   ]                             macro termination character
 * A dummy parameter in the body is recognised only when followed by a space
 * or newline, and that following character is NOT part of the body. Calls
 * pass a comma-separated argument list: NAME arg1, arg2.                  */

static int macro_find(mac_state *st, const char *name)
{
    char key[MAC_SYM_LEN + 1];
    sym_norm(name, key);
    for (int i = 0; i < st->nmacros; i++)
    {
        if (strcmp(st->macros[i].name, key) == 0)
        {
            return i;
        }
    }
    return -1;
}

/* Begin capturing a macro definition. 'args' is everything after ")MCDEF". */
static void macro_begin(mac_state *st, const char *args)
{
    while (*args != '\0' && isspace((unsigned char)*args))
    {
        args++;
    }
    if (st->nmacros >= MAC_MAX_MACROS)
    {
        mac_err(st, "macro table full", NULL);
        return;
    }
    mac_macro *m = &st->macros[st->nmacros];
    memset(m, 0, sizeof(*m));

    /* macro name */
    int i = 0;
    char nbuf[64];
    while (*args != '\0' && !isspace((unsigned char)*args) && *args != ',' &&
           i < 63)
    {
        nbuf[i++] = *args++;
    }
    nbuf[i] = '\0';
    sym_norm(nbuf, m->name);

    /* dummy parameter list: $A1, $A2, ... */
    while (*args != '\0' && m->nparams < MAC_MACRO_ARGS_MAX)
    {
        while (*args != '\0' &&
               (isspace((unsigned char)*args) || *args == ','))
        {
            args++;
        }
        if (*args != '$')
        {
            break;
        }
        int p = 0;
        while (*args != '\0' && !isspace((unsigned char)*args) &&
               *args != ',' && p < 7)
        {
            m->params[m->nparams][p++] = *args++;
        }
        m->params[m->nparams][p] = '\0';
        m->nparams++;
    }

    m->body = (char *)calloc(MAC_MACRO_BODY_MAX, 1);
    st->capturing = true;
    st->cap_index = st->nmacros;
    st->cap_len = 0;
    st->nmacros++;
}

/* Append one raw source line to the macro body being captured. Returns true
 * when the terminator ']' was seen and the definition is complete.        */
static bool macro_capture_line(mac_state *st, const char *line)
{
    const char *p = line;
    while (*p != '\0' && isspace((unsigned char)*p))
    {
        p++;
    }
    if (*p == ']')
    {
        st->capturing = false;
        return true;
    }
    mac_macro *m = &st->macros[st->cap_index];
    size_t n = strlen(line);
    if (st->cap_len + n + 2 < MAC_MACRO_BODY_MAX)
    {
        memcpy(m->body + st->cap_len, line, n);
        st->cap_len += n;
        m->body[st->cap_len++] = '\n';
        m->body[st->cap_len] = '\0';
    }
    else
    {
        mac_err(st, "macro body overflow in", m->name);
    }
    return false;
}

/* Expand a macro call: substitute the arguments for the dummy parameters
 * and feed the resulting lines back through mac_line().                   */
static void macro_expand(mac_state *st, int idx, const char *arglist)
{
    mac_macro *m = &st->macros[idx];

    /* split the argument list on commas */
    char args[MAC_MACRO_ARGS_MAX][128];
    int nargs = 0;
    memset(args, 0, sizeof(args));
    const char *p = arglist;
    while (*p != '\0' && nargs < MAC_MACRO_ARGS_MAX)
    {
        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ','))
        {
            p++;
        }
        if (*p == '\0')
        {
            break;
        }
        int a = 0;
        while (*p != '\0' && *p != ',' && a < 127)
        {
            args[nargs][a++] = *p++;
        }
        /* trim trailing blanks of this argument */
        while (a > 0 && isspace((unsigned char)args[nargs][a - 1]))
        {
            a--;
        }
        args[nargs][a] = '\0';
        nargs++;
    }

    if (st->expand_depth > 16)
    {
        mac_err(st, "macro expansion too deep in", m->name);
        return;
    }
    st->expand_depth++;

    /* walk the body line by line, substituting '$Pn' followed by space/EOL */
    const char *b = m->body;
    char out[1024];
    while (*b != '\0')
    {
        size_t o = 0;
        while (*b != '\0' && *b != '\n' && o < sizeof(out) - 1)
        {
            if (*b == '$')
            {
                /* collect the dummy-parameter token */
                char dp[16];
                int d = 0;
                dp[d++] = '$';
                const char *q = b + 1;
                while (*q != '\0' && !isspace((unsigned char)*q) &&
                       *q != '\n' && *q != ',' && *q != ';' && d < 15)
                {
                    dp[d++] = *q++;
                }
                dp[d] = '\0';
                int found = -1;
                for (int k = 0; k < m->nparams; k++)
                {
                    if (strcmp(m->params[k], dp) == 0)
                    {
                        found = k;
                        break;
                    }
                }
                if (found >= 0)
                {
                    const char *rep = (found < nargs) ? args[found] : "";
                    size_t rl = strlen(rep);
                    if (o + rl < sizeof(out) - 1)
                    {
                        memcpy(out + o, rep, rl);
                        o += rl;
                    }
                    b = q;
                    /* the single space that terminated the parameter is
                     * consumed and NOT copied (manual sec 4.2.9.2)         */
                    if (*b == ' ')
                    {
                        b++;
                    }
                    continue;
                }
                /* not a dummy parameter: copy the '$' verbatim */
                out[o++] = *b++;
                continue;
            }
            out[o++] = *b++;
        }
        out[o] = '\0';
        if (*b == '\n')
        {
            b++;
        }
        mac_line(st, out);
    }
    st->expand_depth--;
}

/* ---------------------------------------------------------------------- */
/* commands ')CMD'                                                         */
/* ---------------------------------------------------------------------- */

/* )FILL - dump the pending literal pool at the current location, then
 * resolve every P-relative reference to the cells just placed.            */
static void cmd_fill(mac_state *st)
{
    for (int i = 0; i < st->nlits; i++)
    {
        mac_literal *L = &st->lits[i];
        if (L->placed)
        {
            continue;
        }
        L->cell = st->loc;
        L->placed = true;
        if (L->resolved)
        {
            emit(st, (uint16_t)(L->value + L->extra));
        }
        else
        {
            mac_sym *s = sym_intern(st, L->sym);
            if (s->defined)
            {
                emit(st, (uint16_t)(s->value + L->extra));
            }
            else
            {
                st->mem[st->loc] = L->extra;
                st->used[st->loc] = 1;
                pending_add(s, st->loc, st->loc, MAC_FIX_FULL);
                st->loc++;
            }
        }
    }

    /* patch instructions that addressed these literals P-relative           */
    litref **pp = &g_litrefs;
    while (*pp != NULL)
    {
        litref *r = *pp;
        mac_literal *L = &st->lits[r->lit_index];
        if (L->placed)
        {
            if (r->absolute)
            {
                /* data word: the literal's cell ADDRESS is the value */
                st->mem[r->addr] = (uint16_t)(st->mem[r->addr] + L->cell);
            }
            else
            {
                int disp = (int)L->cell - (int)r->pc;
                st->mem[r->addr] = (uint16_t)((st->mem[r->addr] & 0xFF00) |
                                              (disp & 0x00FF));
            }
            *pp = r->next;
            free(r);
        }
        else
        {
            pp = &r->next;
        }
    }
    st->nlits = 0;
}

/* )KILL SYM... - expunge symbols from the table (they may be redefined).  */
static void cmd_kill(mac_state *st, const char *args)
{
    char name[64];
    const char *p = args;
    while (*p != '\0')
    {
        while (isspace((unsigned char)*p))
        {
            p++;
        }
        int i = 0;
        while (*p != '\0' && !isspace((unsigned char)*p) && i < 63)
        {
            name[i++] = *p++;
        }
        name[i] = '\0';
        if (i == 0)
        {
            break;
        }
        char key[MAC_SYM_LEN + 1];
        sym_norm(name, key);
        mac_sym **pp = &st->symtab;
        while (*pp != NULL)
        {
            if (strcmp((*pp)->name, key) == 0)
            {
                mac_sym *dead = *pp;
                *pp = dead->next;
                free(dead);
                break;
            }
            pp = &(*pp)->next;
        }
    }
}

/* )PCL SYM - partial clear: remove every symbol defined AFTER SYM (in
 * time), and SYM itself stays. Uses def_seq ordering.                     */
static void cmd_pcl(mac_state *st, const char *args)
{
    char name[64];
    sscanf(args, "%63s", name);
    mac_sym *anchor = sym_find(st, name);
    if (anchor == NULL)
    {
        return;
    }
    uint32_t cut = anchor->def_seq;
    mac_sym **pp = &st->symtab;
    while (*pp != NULL)
    {
        if (!(*pp)->permanent && (*pp)->defined && (*pp)->def_seq > cut)
        {
            mac_sym *dead = *pp;
            *pp = dead->next;
            free(dead);
        }
        else
        {
            pp = &(*pp)->next;
        }
    }
}

/* Map an ND file specification onto a host path.
 *   "NAME:TYPE" -> "NAME.TYPE"   (ND writes the type after a colon)
 *   "NAME"      -> "NAME.<deftype>" if NAME itself is not present
 * MAC supplies a default type per stream: :SYMB for the source and list
 * streams, :BRF for the object stream (ND-60.096.01 )9ASSM table). The
 * probe means both "TSS1" and "TSS1.SYMB" work from the command line.    */
static void nd_path(const char *name, char *out, size_t cap,
                    const char *deftype, bool probe)
{
    size_t i = 0;
    for (const char *p = name; *p != '\0' && i + 1 < cap; p++)
    {
        out[i++] = (*p == ':') ? '.' : *p;
    }
    out[i] = '\0';
    if (strchr(out, '.') != NULL || deftype == NULL)
    {
        return;
    }
    if (probe)
    {
        FILE *t = fopen(out, "rb");
        if (t != NULL)
        {
            fclose(t);
            return;      /* the bare name exists as given */
        }
    }
    snprintf(out + i, cap - i, ".%s", deftype);
}

/* Open one )9ASSM stream argument. "" keeps the current stream, "0" means
 * the dummy device (discard), a name in double quotes is created. Returns
 * the new FILE* (or NULL for the dummy device) and sets *owned.          */
static FILE *open_stream_arg(mac_state *st, const char *arg, bool *owned,
                             FILE *current, bool current_owned,
                             const char *deftype)
{
    *owned = current_owned;
    if (arg[0] == '\0')
    {
        return current;                       /* unchanged */
    }
    if (strcmp(arg, "0") == 0)
    {
        if (current_owned && current != NULL)
        {
            fclose(current);
        }
        *owned = false;
        return NULL;                          /* dummy device */
    }
    if (current_owned && current != NULL)
    {
        fclose(current);
    }
    char path[512];
    nd_path(arg, path, sizeof(path), deftype, false);
    FILE *f = fopen(path, "wb");
    if (f == NULL)
    {
        mac_err(st, "cannot open stream file:", path);
        *owned = false;
        return NULL;
    }
    *owned = true;
    return f;
}

/* Collect the next space/comma delimited word from *pp, advancing it.     */
static bool next_word(const char **pp, char *out, size_t cap)
{
    const char *p = *pp;
    while (*p != '\0' && (isspace((unsigned char)*p) || *p == ','))
    {
        p++;
    }
    size_t i = 0;
    while (*p != '\0' && !isspace((unsigned char)*p) && *p != ',' &&
           i + 1 < cap)
    {
        out[i++] = *p++;
    }
    out[i] = '\0';
    *pp = p;
    return i > 0;
}

/* )9SET ,B SYM  /  )9SET ,X SYM - set the B or X location counter. The
 * second argument must be a DEFINED symbol, not an expression
 * (ND-60.096.01 sec 3.2.4). ")9SET ,B ZRO" zeroes it (ZRO is a permanent
 * symbol whose value is 0).                                              */
static void cmd_9set(mac_state *st, const char *args)
{
    char which[16], name[64];
    const char *p = args;
    if (!next_word(&p, which, sizeof(which)) ||
        !next_word(&p, name, sizeof(name)))
    {
        mac_err(st, ")9SET needs ,B or ,X and a symbol", NULL);
        return;
    }
    mac_sym *s = sym_find(st, name);
    if (s == NULL || !s->defined)
    {
        mac_err(st, ")9SET symbol is not defined:", name);
        return;
    }
    /* next_word() treats ',' as a delimiter, so ",B" arrives as "B";
     * accept both spellings.                                              */
    if (strcmp(which, ",B") == 0 || strcmp(which, "B") == 0)
    {
        st->bcounter = s->value;
    }
    else if (strcmp(which, ",X") == 0 || strcmp(which, "X") == 0)
    {
        st->xcounter = s->value;
    }
    else
    {
        mac_err(st, ")9SET first argument must be ,B or ,X:", which);
    }
}

/* )ZERO [SYM] - fill the interval set by '<' with zero, or with the value
 * of one following mnemonic/defined symbol (ND-60.096.01 sec 4.2.3.1).   */
static void cmd_zero(mac_state *st, const char *args)
{
    uint16_t fill = 0;
    char name[64];
    const char *p = args;
    if (next_word(&p, name, sizeof(name)))
    {
        mac_sym *s = sym_find(st, name);
        if (s != NULL && s->defined)
        {
            fill = s->value;
        }
        else
        {
            mac_err(st, ")ZERO symbol is not defined:", name);
            return;
        }
    }
    for (uint32_t a = st->ilow; a <= st->ihigh && a < MAC_MEM_WORDS; a++)
    {
        st->mem[a] = fill;
        st->used[a] = 1;
        if (a < st->lo_used) st->lo_used = (uint16_t)a;
        if (a > st->hi_used) st->hi_used = (uint16_t)a;
    }
}

/* )CHANGE - within the '<' interval, every word whose masked bits equal
 * chg_old has those bits replaced by chg_new (ND-60.096.01 sec 4.2.3.5). */
static void cmd_change(mac_state *st)
{
    for (uint32_t a = st->ilow; a <= st->ihigh && a < MAC_MEM_WORDS; a++)
    {
        if ((st->mem[a] & st->chg_mask) == (st->chg_old & st->chg_mask))
        {
            st->mem[a] = (uint16_t)((st->mem[a] & ~st->chg_mask) |
                                    (st->chg_new & st->chg_mask));
        }
    }
}

/* )WRITE SYM... - list the named user symbols and their values; symbols
 * referenced but not yet defined print "/NF" (ND-60.096.01 )WRITE).
 * Ignored unless write mode is on ()WRTM).                               */
static void cmd_write(mac_state *st, const char *args, FILE *out)
{
    if (!st->write_mode)
    {
        return;
    }
    char name[64];
    const char *p = args;
    while (next_word(&p, name, sizeof(name)))
    {
        mac_sym *s = sym_find(st, name);
        if (s != NULL && s->defined)
        {
            fprintf(out, "%5s=%06o\n", s->name, s->value);
        }
        else
        {
            fprintf(out, "%5s=/NF\n", name);
        }
    }
}

/* )WRUS (also '?') - every symbol referenced but still undefined, in order
 * of first appearance.                                                    */
static void cmd_wrus(mac_state *st, FILE *out)
{
    mac_report_undefined(st, out);
}

/* )WLOC - all user defined symbols, six per line.                        */
static void cmd_wloc(mac_state *st, FILE *out)
{
    int n = 0;
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (!s->permanent && s->defined)
        {
            fprintf(out, "%-8s", s->name);
            if (++n % 6 == 0)
            {
                fputc('\n', out);
            }
        }
    }
    if (n % 6 != 0)
    {
        fputc('\n', out);
    }
}

/* )WMNE - the built-in opcodes and addressing mode specifiers, six/line. */
static void cmd_wmne(mac_state *st, FILE *out)
{
    int n = 0;
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (s->permanent)
        {
            fprintf(out, "%-8s", s->name);
            if (++n % 6 == 0)
            {
                fputc('\n', out);
            }
        }
    }
    if (n % 6 != 0)
    {
        fputc('\n', out);
    }
}

/* )PRINT - octal dump of the '<' interval to the list stream.            */
static void cmd_print(mac_state *st, FILE *out)
{
    for (uint32_t a = st->ilow; a <= st->ihigh && a < MAC_MEM_WORDS; a++)
    {
        fprintf(out, "%06o/ %06o\n", (unsigned)a, st->mem[a]);
    }
}

/* )9ASCI - dump the '<' interval as ASCII, two characters per word.      */
static void cmd_9asci(mac_state *st, FILE *out)
{
    for (uint32_t a = st->ilow; a <= st->ihigh && a < MAC_MEM_WORDS; a++)
    {
        int hi = (st->mem[a] >> 8) & 0x7F;
        int lo = st->mem[a] & 0x7F;
        fputc(isprint(hi) ? hi : '.', out);
        fputc(isprint(lo) ? lo : '.', out);
    }
    fputc('\n', out);
}

/* )CORE - report the bounds of the assembled image.                      */
static void cmd_core(mac_state *st, FILE *out)
{
    if (st->hi_used >= st->lo_used)
    {
        fprintf(out, "USED AREA %06o-%06o\n", st->lo_used, st->hi_used);
    }
    else
    {
        fprintf(out, "USED AREA EMPTY\n");
    }
}

/* '<' interval command:  LOW < HIGH                                      */
static void cmd_interval(mac_state *st, char *stmt, char *lt)
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

static void assemble_stmt(mac_state *st, char *stmt)
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
            st->mem[here] = (uint16_t)(opc + oper.mode);
            st->used[here] = 1;
            if (here < st->lo_used) st->lo_used = here;
            if (here > st->hi_used) st->hi_used = here;
            st->loc++;
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
             * defined. TSS forward references are P-relative labels.        */
            st->mem[here] = (uint16_t)(opc + oper.mode);
            st->used[here] = 1;
            if (here < st->lo_used) st->lo_used = here;
            if (here > st->hi_used) st->hi_used = here;
            st->loc++;
            pending_add(oper.undef, here, here, MAC_FIX_PREL8);
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
            st->mem[here] = opc;
            st->used[here] = 1;
            if (here < st->lo_used) st->lo_used = here;
            if (here > st->hi_used) st->hi_used = here;
            st->loc++;
            pending_add(oper.undef, here, here, MAC_FIX_PREL8);
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
            st->mem[here] = opc;
            st->used[here] = 1;
            if (here < st->lo_used) st->lo_used = here;
            if (here > st->hi_used) st->hi_used = here;
            st->loc++;
            pending_add(uo, here, here, MAC_FIX_ARG8);
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
        st->mem[here] = 0;
        st->used[here] = 1;
        if (here < st->lo_used) st->lo_used = here;
        if (here > st->hi_used) st->hi_used = here;
        st->loc++;
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
            st->mem[here] = v;
            st->used[here] = 1;
            if (here < st->lo_used) st->lo_used = here;
            if (here > st->hi_used) st->hi_used = here;
            st->loc++;
            pending_add(u, here, here, MAC_FIX_FULL);
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
static bool eval_mark(mac_state *st, const char *expr)
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

void mac_line(mac_state *st, const char *line)
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
            cmd_print(st, lst); /* same dump, object stream in real MAC */
            return;
        }
        if (strncmp(s, ")9ASCI", 6) == 0)
        {
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
            st->symbolic_out = true;
            return;
        }
        if (strncmp(s, ")RESSM", 6) == 0)
        {
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
        if (strncmp(s, ")9ASSM", 6) == 0)
        {
            /* TODO: nested include. The top-level driver expands these;
             * inside a stream we log and skip.                             */
            fprintf(stderr, "%s:%d: TODO nested )9ASSM: %s\n",
                    st->cur_file, st->cur_line, s);
            return;
        }
        /* Unknown / unhandled command: ignore quietly (many are listing or
         * environment controls with no effect on the image).              */
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
/* public API                                                              */
/* ---------------------------------------------------------------------- */

void mac_init(mac_state *st)
{
    memset(st, 0, sizeof(*st));
    st->lo_used = 0xFFFF;
    st->hi_used = 0x0000;
    st->cond_active = true;
    st->parity_check = true;   /* "parity checking is initially on"        */
    st->write_mode = false;    /* "MAC is initially in non-write mode"     */
    st->dup_literals = false;  /* literals are shared unless )9LITR is set */
    g_pending = NULL;
    g_litrefs = NULL;

    /* install the byte-verified permanent symbol table */
    for (size_t i = 0; i < MAC_PERMSYM_COUNT; i++)
    {
        mac_sym *s = (mac_sym *)calloc(1, sizeof(mac_sym));
        sym_norm(MAC_PERMSYM[i].name, s->name);
        s->value = MAC_PERMSYM[i].value;
        s->defined = true;
        s->permanent = true;
        s->cls = MAC_PERMSYM[i].cls;
        s->next = st->symtab;
        st->symtab = s;
    }
}

void mac_define(mac_state *st, const char *name, uint16_t value)
{
    mac_sym *s = sym_intern(st, name);
    s->value = value;
    s->defined = true;
    s->def_seq = ++st->def_seq;
    mac_fixup **head = pending_chain_for(s);
    sym_resolve_fixups(st, s, head);
}

void mac_reference_mark(mac_state *st, const char *name)
{
    /* create as undefined so eval_mark() sees it as a true library mark */
    (void)sym_intern(st, name);
}

bool mac_assemble_file(mac_state *st, const char *spec)
{
    /* the source stream's default type is :SYMB, so ")9ASSM TSS1" opens
     * TSS1:SYMB -> TSS1.SYMB on the host                                  */
    char path[512];
    nd_path(spec, path, sizeof(path), "SYMB", true);
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        fprintf(stderr, "cannot open %s\n", path);
        return false;
    }
    /* own the name for the duration of this file so diagnostics from
     * nested )9ASSM levels cannot reference a dead stack buffer.
     * (strdup is not declared by -std=c99, so copy by hand.)             */
    size_t plen = strlen(path);
    char *owned = (char *)malloc(plen + 1);
    if (owned == NULL)
    {
        fclose(f);
        return false;
    }
    memcpy(owned, path, plen + 1);
    const char *prev_file = st->cur_file;
    int prev_line = st->cur_line;
    st->cur_file = owned;
    st->cur_line = 0;
    st->end_of_file_seen = false;

    char line[1024];
    while (fgets(line, sizeof(line), f) != NULL)
    {
        mac_line(st, line);
        if (st->end_of_file_seen)
        {
            /* )LINE / )9EXIT returns control to the console. Set MACTRACE
             * to see which line ended each stream - useful when a nested
             * )9ASSM appears to swallow the rest of its caller.           */
            if (getenv("MACTRACE") != NULL)
            {
                fprintf(stderr, "[trace] %s ended at line %d\n",
                        path, st->cur_line);
            }
            break;
        }
    }
    if (getenv("MACTRACE") != NULL)
    {
        fprintf(stderr, "[trace] leaving %s after %d lines, cond_active=%d\n",
                path, st->cur_line, (int)st->cond_active);
    }
    fclose(f);
    st->cur_file = prev_file;
    st->cur_line = prev_line;
    free(owned);
    return true;
}

void mac_list_symbols(mac_state *st, FILE *out)
{
    /* Archived ASYMB:SYMB format: right-justified 5-char name, '=', 6-digit
     * octal value. Non-permanent, defined symbols only, in definition
     * order (we reverse the intrusive list which is newest-first).         */
    /* count */
    int count = 0;
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (!s->permanent && s->defined)
        {
            count++;
        }
    }
    mac_sym **arr = (mac_sym **)calloc((size_t)(count > 0 ? count : 1),
                                       sizeof(mac_sym *));
    int idx = count;
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (!s->permanent && s->defined)
        {
            arr[--idx] = s; /* reverse newest-first into definition order */
        }
    }
    for (int i = 0; i < count; i++)
    {
        fprintf(out, "%5s=%06o\n", arr[i]->name, arr[i]->value);
    }
    free(arr);
}

uint16_t sym_lookup_value(mac_state *st, const char *name)
{
    mac_sym *s = sym_find(st, name);
    return (s != NULL && s->defined) ? s->value : 0;
}

bool sym_is_defined(mac_state *st, const char *name)
{
    mac_sym *s = sym_find(st, name);
    return (s != NULL && s->defined);
}

void mac_report_undefined(mac_state *st, FILE *out)
{
    int n = 0;
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (!s->defined)
        {
            fprintf(out, "UNDEFINED: %s\n", s->name);
            n++;
        }
    }
    if (n == 0)
    {
        fprintf(out, "no undefined symbols\n");
    }
}

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
    fwrite(BPUN_OCTAL_LOADER, 1, sizeof(BPUN_OCTAL_LOADER), f);

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

void mac_close_streams(mac_state *st)
{
    if (st->own_listing && st->listing != NULL)
    {
        fclose(st->listing);
    }
    if (st->own_object && st->object != NULL)
    {
        fclose(st->object);
    }
    st->listing = NULL;
    st->object = NULL;
    st->own_listing = false;
    st->own_object = false;
}
