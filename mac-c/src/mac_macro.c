/* MAC-C - )MCDEF macro capture and expansion
 * Split out of the former single mac.c; see mac_internal.h.
 */
#include "mac_internal.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* macros ()MCDEF)                                                         */
/* ---------------------------------------------------------------------- */
/* Per ND-60.096.01 sec 4.2.9.2:
 *   )MCDEF NAME $P1, $P2 ...      definition line, ends at the newline
 *   <body lines>                  substituted at call time
 *   ]                             macro termination character
 * A dummy parameter in the body is recognised only when followed by a space
 * or newline, and that following character is NOT part of the body. Calls
 * pass a comma-separated argument list: NAME arg1, arg2.                  */

int macro_find(mac_state *st, const char *name)
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
void macro_begin(mac_state *st, const char *args)
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
bool macro_capture_line(mac_state *st, const char *line)
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
void macro_expand(mac_state *st, int idx, const char *arglist)
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
