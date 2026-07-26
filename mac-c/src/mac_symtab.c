/* MAC-C - symbol table, fixup chains, diagnostics, memory emission
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
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
void sym_norm(const char *in, char *out)
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

mac_sym *sym_find(mac_state *st, const char *name)
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
mac_sym *sym_intern(mac_state *st, const char *name)
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
void sym_resolve_fixups(mac_state *st, mac_sym *sym, mac_fixup **head)
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
             * f->pc holds the instruction address.
             * f->addend is the constant part of the operand expression
             * ("JMP RFN+2" -> 2): the true target is sym + addend. Found
             * live on nd100x: without it TSS2 ROBJ's three "JMP RFN+2/+3"
             * exits all landed on RFN, rewriting error 9 to -1 and looping
             * LOGON's ()SCRATCH open forever (the login hang).            */
            int disp = (int)(uint16_t)(sym->value + f->addend) - (int)f->pc;
            st->mem[f->addr] = (uint16_t)((st->mem[f->addr] & 0xFF00) |
                                          (disp & 0x00FF));
            break;
        }
        case MAC_FIX_ARG8:
            /* same addend rule for argument instructions (SAA FOO+2)      */
            st->mem[f->addr] = (uint16_t)((st->mem[f->addr] & 0xFF00) |
                                          ((sym->value + f->addend) & 0x00FF));
            break;
        }
        free(f);
        f = next;
    }
    *head = NULL;
}


pending *g_pending;   /* module state: forward-ref chains by symbol       */


litref *g_litrefs;

mac_fixup **pending_chain_for(mac_sym *sym)
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

/* addend: the already-evaluated constant part of the expression the
 * undefined symbol sits in (eval_expr sums undefined symbols as 0, so for
 * "RFN+2" the accumulator holds 2). FULL fixups keep that constant in the
 * emitted word itself and pass 0 here; PREL8/ARG8 words hold opcode+mode
 * in the field being patched, so the constant must travel in the fixup.   */
void pending_add(mac_sym *sym, uint16_t addr, uint16_t pc,
                        mac_fix_kind k, uint16_t addend)
{
    mac_fixup **head = pending_chain_for(sym);
    mac_fixup *f = (mac_fixup *)calloc(1, sizeof(mac_fixup));
    f->addr = addr;
    f->pc = pc;
    f->addend = addend;
    f->kind = k;
    f->next = *head;
    *head = f;
}

/* ---------------------------------------------------------------------- */
/* diagnostics                                                             */
/* ---------------------------------------------------------------------- */

void mac_err(mac_state *st, const char *msg, const char *detail)
{
    st->errors++;
    fprintf(stderr, "%s:%d: ERROR: %s%s%s\n",
            st->cur_file ? st->cur_file : "?", st->cur_line, msg,
            detail ? " " : "", detail ? detail : "");
}

/* ---------------------------------------------------------------------- */
/* memory emission                                                         */
/* ---------------------------------------------------------------------- */

void emit(mac_state *st, uint16_t word)
{
    /* -L address listing: record what this word was and where it went, so
     * mac_line() can print the row. Recording here rather than diffing the
     * location counter is exact - it survives a line that both moves '*'
     * and emits (e.g. ')FILL' after a location set), which a before/after
     * comparison of st->loc would misreport.                              */
    if (st->asm_list != NULL)
    {
        if (st->al_n >= 0 && st->al_n < MAC_ASM_LIST_MAX)
        {
            st->al_addr[st->al_n] = st->loc;
            st->al_word[st->al_n] = word;
            st->al_n++;
        }
        else if (st->al_n >= 0)
        {
            st->al_n = -1; /* overflowed: the row is marked "..." */
        }
    }

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
