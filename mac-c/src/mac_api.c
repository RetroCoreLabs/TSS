/* MAC-C - the public mac_* entry points
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

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
        /* Strip the ND parity bit (bit 7) from every ingested source byte.
         * The 1973 tape files carry 8-bit bytes with odd/even parity in bit 7
         * (see archive/README.md); the derived src/ copies were produced by
         * exactly this operation:  perl -pe 's/(.)/chr(ord($1)&0x7f)/ge'.
         * Doing it here means mac-c can assemble the parity-set ORIGINALS in
         * archive/ directly, and it is a strict NO-OP on the already-stripped
         * src/ copies (their bytes are all <0200), so the golden dumps are
         * byte-for-byte unaffected. Source text is 7-bit ASCII throughout —
         * including inside '..' strings and #ab char constants — so clearing
         * bit 7 never destroys a legitimate source character. (The 0x9D->0x1D
         * caveat in archive/README concerns the LIST *dump* files, not source.)
         *
         * We stop at the fgets NUL terminator; a genuine 0200 byte would strip
         * to NUL, but source files contain none (only the ASYMB/BSYMB dumps
         * carry the 0200 leader, and those are never assembled).            */
        for (char *p = line; *p != '\0'; ++p)
            *p = (char)((unsigned char)*p & 0x7f);
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

int mac_check_undefined_opcodes(mac_state *st)
{
    /* Turn every "flagged but still undefined" opcode-position symbol into a
     * hard error. This runs AFTER the whole build so genuine forward
     * references (undefined when first seen, defined later) have already
     * cleared their defined flag and are skipped here. See the flagging site
     * in assemble_stmt() for the full rationale.                            */
    int n = 0;
    for (mac_sym *s = st->symtab; s != NULL; s = s->next)
    {
        if (s->defined)
        {
            continue;
        }
        /* mac_err increments st->errors, so main() returns non-zero and the
         * build FAILS instead of shipping a silently-corrupt image. Two cases:
         *   - used_as_opcode : undefined symbol in the OPCODE position
         *     (e.g. "RGDIV ST" in a non-N10 build).
         *   - used_as_operand: undefined symbol consumed as the VALUE OPERAND
         *     of a defined instruction (e.g. "SAT STR1" -> silently SAT 0,
         *     the STR->XTR rename casualty). Both are silent-miscompile bugs
         *     that the golden ADDRESS dumps cannot see (word count unchanged). */
        if (s->used_as_opcode)
        {
            mac_err(st, "undefined instruction:", s->name);
            n++;
        }
        else if (s->used_as_operand)
        {
            mac_err(st, "undefined operand:", s->name);
            n++;
        }
    }
    return n;
}

bool mac_open_asm_listing(mac_state *st, const char *path)
{
    FILE *f = fopen(path, "w");
    if (f == NULL)
    {
        return false;
    }
    st->asm_list = f;
    st->own_asm_list = true;
    st->al_n = 0;
    st->al_depth = 0;
    fprintf(f, "%-26s %-7s %-*s %s\n", "source", "addr",
            6 * 7 - 1, "emitted words (octal)", "statement");
    fprintf(f, "%-26s %-7s %-*s %s\n", "--------------------------",
            "------", 6 * 7 - 1,
            "-----------------------------------------",
            "---------");
    return true;
}

void mac_close_streams(mac_state *st)
{
    /* write the buffered -L rows now that every fixup has been applied */
    asm_list_flush(st);
    if (st->own_asm_list && st->asm_list != NULL)
    {
        fclose(st->asm_list);
    }
    st->asm_list = NULL;
    st->own_asm_list = false;
    if (st->own_listing && st->listing != NULL)
    {
        fclose(st->listing);
    }
    if (st->own_object && st->object != NULL)
    {
        fclose(st->object);
    }
    if (st->own_punch && st->punch != NULL)
    {
        fclose(st->punch);
    }
    st->listing = NULL;
    st->object = NULL;
    st->punch = NULL;
    st->own_listing = false;
    st->own_object = false;
    st->own_punch = false;
}
