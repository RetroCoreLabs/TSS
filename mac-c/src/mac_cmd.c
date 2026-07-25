/* MAC-C - the ')' command implementations and stream plumbing
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* commands ')CMD'                                                         */
/* ---------------------------------------------------------------------- */

/* )FILL - dump the pending literal pool at the current location, then
 * resolve every P-relative reference to the cells just placed.            */
void cmd_fill(mac_state *st)
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
                pending_add(s, st->loc, st->loc, MAC_FIX_FULL, 0);
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
void cmd_kill(mac_state *st, const char *args)
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
void cmd_pcl(mac_state *st, const char *args)
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
void nd_path(const char *name, char *out, size_t cap,
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
FILE *open_stream_arg(mac_state *st, const char *arg, bool *owned,
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
bool next_word(const char **pp, char *out, size_t cap)
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
void cmd_9set(mac_state *st, const char *args)
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
void cmd_zero(mac_state *st, const char *args)
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
 * chg_old has those bits replaced by chg_new (ND-60.096.01 sec 4.2.3.5).
 *
 * [VERIFIED manual sec 4.2.3.5, line 3006/3017] The command takes NO
 * operands. The three constants live in the CONTENTS of the memory
 * locations labelled OLD, NEW and MASK ("The memory locations labeled OLD,
 * NEW and MASK are added to MAC as part of this option"). The user deposits
 * them with e.g. "OLD/ 000177" before executing )CHANGE. We therefore load
 * chg_old/chg_new/chg_mask from mem[OLD]/mem[NEW]/mem[MASK] whenever those
 * symbols are defined; if they are not defined (the TSS corpus never uses
 * )CHANGE, so they never are) we fall back to the st->chg_* fields, which a
 * driver/test may set directly. This makes the previously-inert command
 * behave exactly as the manual describes without adding permanent symbols
 * that could perturb the golden symbol dump.                              */
void cmd_change(mac_state *st)
{
    /* Pull the sought/replacement/mask constants from the OLD/NEW/MASK
     * cells if the source defined them (the manual's mechanism).          */
    mac_sym *so = sym_find(st, "OLD");
    mac_sym *sn = sym_find(st, "NEW");
    mac_sym *sm = sym_find(st, "MASK");
    if (so != NULL && so->defined) { st->chg_old  = st->mem[so->value]; }
    if (sn != NULL && sn->defined) { st->chg_new  = st->mem[sn->value]; }
    if (sm != NULL && sm->defined) { st->chg_mask = st->mem[sm->value]; }

    for (uint32_t a = st->ilow; a <= st->ihigh && a < MAC_MEM_WORDS; a++)
    {
        if ((st->mem[a] & st->chg_mask) == (st->chg_old & st->chg_mask))
        {
            st->mem[a] = (uint16_t)((st->mem[a] & ~st->chg_mask) |
                                    (st->chg_new & st->chg_mask));
        }
    }
}

/* )9MOVE src dst count - verbatim block copy of assembled words within the
 * image (NO relocation/patching of the copied words).
 *
 * [VERIFIED manual sec C.1.1.1, line 5130-5140] "used to move a block of
 * image from one place to another. )9MOVE must be followed by three
 * standard MAC symbols separated by spaces ... 1. a source address 2. a
 * destination address 3. word count." It is an FMAC/MACF extension, not a
 * base-MAC command.
 *
 * [VERIFIED docs/TSS-ARCHITECTURE.md (overlay chapter) sec 1.3, line 83-87] "It is a raw
 * block copy of assembled words - no relocation/patching." In the TSS
 * corpus it appears once, in the "MACF variant of the OVERX macro
 * (TSS3.SYMB:83 ")9MOVE ROVER VOR VORS"), staging each 1000-octal-word
 * overlay from its assembly window at ROVER into the next VOR slot so all
 * 31 overlays survive in MAC's memory image (VOR advances by VORS=01000 per
 * overlay: 040000, 041000, ... 076000). See TSS-ARCHITECTURE.md (overlay chapter) sec 6.
 *
 * Operands may be a symbol or a number (each is run through eval_expr, so a
 * small expression works too). If any operand is undefined we flag an error
 * and copy nothing - a )9MOVE with unresolved addresses cannot be honoured. */
void cmd_9move(mac_state *st, const char *args)
{
    char w[3][64];
    const char *p = args;
    /* three space/comma separated operands */
    if (!next_word(&p, w[0], sizeof(w[0])) ||
        !next_word(&p, w[1], sizeof(w[1])) ||
        !next_word(&p, w[2], sizeof(w[2])))
    {
        mac_err(st, ")9MOVE needs src dst count", NULL);
        return;
    }

    uint16_t v[3];
    for (int k = 0; k < 3; k++)
    {
        mac_sym *u = NULL;
        bool a = false;
        v[k] = eval_expr(st, w[k], &u, &a);
        if (u != NULL && !u->defined)
        {
            mac_err(st, ")9MOVE operand is undefined:", w[k]);
            return;
        }
    }

    uint16_t src   = v[0];
    uint16_t dst   = v[1];
    uint16_t count = v[2];

    /* Verbatim copy, word for word. memmove-style semantics are irrelevant
     * for the corpus (ROVER and VOR windows never overlap), but we copy in a
     * direction-safe way anyway so an overlapping )9MOVE would still be a
     * true block move. Wrap at the 64K image boundary like real memory.    */
    if (dst > src)
    {
        /* copy high-to-low so an overlapping forward move is not corrupted */
        for (uint32_t i = count; i-- > 0;)
        {
            uint16_t s = (uint16_t)(src + i);
            uint16_t d = (uint16_t)(dst + i);
            st->mem[d] = st->mem[s];
            st->used[d] = 1;
            if (d < st->lo_used) { st->lo_used = d; }
            if (d > st->hi_used) { st->hi_used = d; }
        }
    }
    else
    {
        for (uint32_t i = 0; i < count; i++)
        {
            uint16_t s = (uint16_t)(src + i);
            uint16_t d = (uint16_t)(dst + i);
            st->mem[d] = st->mem[s];
            st->used[d] = 1;
            if (d < st->lo_used) { st->lo_used = d; }
            if (d > st->hi_used) { st->hi_used = d; }
        }
    }
}

/* )WRITE SYM... - list the named user symbols and their values; symbols
 * referenced but not yet defined print "/NF" (ND-60.096.01 )WRITE).
 * Ignored unless write mode is on ()WRTM).                               */
void cmd_write(mac_state *st, const char *args, FILE *out)
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
void cmd_wrus(mac_state *st, FILE *out)
{
    mac_report_undefined(st, out);
}

/* )WLOC - all user defined symbols, six per line.                        */
void cmd_wloc(mac_state *st, FILE *out)
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
void cmd_wmne(mac_state *st, FILE *out)
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
void cmd_print(mac_state *st, FILE *out)
{
    for (uint32_t a = st->ilow; a <= st->ihigh && a < MAC_MEM_WORDS; a++)
    {
        fprintf(out, "%06o/ %06o\n", (unsigned)a, st->mem[a]);
    }
}

/* )9ASCI - dump the '<' interval as ASCII, two characters per word.      */
void cmd_9asci(mac_state *st, FILE *out)
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
void cmd_core(mac_state *st, FILE *out)
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
