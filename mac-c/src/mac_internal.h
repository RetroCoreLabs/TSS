/**************************************************************************
** MAC-C INTERNAL HEADER                                                  **
**                                                                       **
** Shared between the mac_*.c translation units that were split out of   **
** the former single mac.c. NOT a public interface - mac.h is. Nothing   **
** outside src/ should include this.                                     **
**                                                                       **
** The split is by section, along the banners the single file already    **
** carried:                                                             **
**   mac_symtab.c  symbol table, fixup chains, diagnostics, emission     **
**   mac_expr.c    expression evaluation, MRI operand evaluator          **
**   mac_macro.c   )MCDEF capture and expansion                          **
**   mac_cmd.c     the ')' command implementations and stream plumbing   **
**   mac_stmt.c    statement dispatch, library marks, line processing    **
**   mac_api.c     the public mac_* entry points                         **
**   mac_bpun.c    image / CDC disc / BPUN tape readers and writers      **
***************************************************************************/
#ifndef MAC_INTERNAL_H
#define MAC_INTERNAL_H

#include "mac.h"

/* ---------------------------------------------------------------------- */
/* shared types                                                            */
/* ---------------------------------------------------------------------- */

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

/* Split of an MRI operand into mode bits and displacement. Kept separate:
 * the final word is opcode + mode + (disp & 0377), and a negative
 * displacement must be masked to 8 bits BEFORE the mode bits are added,
 * else the sign extension corrupts the mode field (ND-60.096.01 sec 2.6). */
typedef struct
{
    uint16_t mode;      /**< OR of 02000/01000/0400 from mode symbols       */
    uint16_t disp;      /**< 16-bit displacement value (address or literal) */
    bool     is_addr;   /**< disp is an address -> P-relative if no B/X mode */
    mac_sym *undef;     /**< first undefined symbol, for a fixup            */
    int      literal;   /**< index into st->lits if operand was '(EXPR', else -1 */
} mac_operand;

/* ---------------------------------------------------------------------- */
/* shared module state (defined in mac_symtab.c)                           */
/* ---------------------------------------------------------------------- */

extern pending *g_pending;  /* forward-ref chains by symbol                */
extern litref  *g_litrefs;  /* literal references awaiting )FILL           */

/* ---------------------------------------------------------------------- */
/* mac_symtab.c                                                            */
/* ---------------------------------------------------------------------- */

void        sym_norm(const char *in, char *out);
mac_sym    *sym_find(mac_state *st, const char *name);
mac_sym    *sym_intern(mac_state *st, const char *name);
void        sym_resolve_fixups(mac_state *st, mac_sym *sym, mac_fixup **head);
mac_fixup **pending_chain_for(mac_sym *sym);
void        pending_add(mac_sym *sym, uint16_t addr, uint16_t pc,
                        mac_fix_kind k, uint16_t addend);
void        mac_err(mac_state *st, const char *msg, const char *detail);
void        emit(mac_state *st, uint16_t word);

/* ---------------------------------------------------------------------- */
/* mac_expr.c                                                              */
/* ---------------------------------------------------------------------- */

const char *skip_char_const(const char *p);
uint16_t    eval_term(mac_state *st, const char *tok, mac_sym **undef_sym,
                      bool *is_addr);
uint16_t    eval_expr(mac_state *st, const char *expr, mac_sym **undef_sym,
                      bool *is_addr);
int         literal_intern(mac_state *st, const char *inner);
void        eval_operand(mac_state *st, const char *expr, mac_operand *out);

/* ---------------------------------------------------------------------- */
/* mac_macro.c                                                             */
/* ---------------------------------------------------------------------- */

int  macro_find(mac_state *st, const char *name);
void macro_begin(mac_state *st, const char *args);
bool macro_capture_line(mac_state *st, const char *line);
void macro_expand(mac_state *st, int idx, const char *arglist);

/* ---------------------------------------------------------------------- */
/* mac_cmd.c                                                               */
/* ---------------------------------------------------------------------- */

void  cmd_fill(mac_state *st);
void  cmd_kill(mac_state *st, const char *args);
void  cmd_pcl(mac_state *st, const char *args);
void  nd_path(const char *name, char *out, size_t cap,
              const char *deftype, bool probe);
FILE *open_stream_arg(mac_state *st, const char *arg, bool *owned,
                      FILE *current, bool current_owned,
                      const char *deftype);
bool  next_word(const char **pp, char *out, size_t cap);
void  cmd_9set(mac_state *st, const char *args);
void  cmd_zero(mac_state *st, const char *args);
void  cmd_change(mac_state *st);
void  cmd_9move(mac_state *st, const char *args);
void  cmd_write(mac_state *st, const char *args, FILE *out);
void  cmd_wrus(mac_state *st, FILE *out);
void  cmd_wloc(mac_state *st, FILE *out);
void  cmd_wmne(mac_state *st, FILE *out);
void  cmd_print(mac_state *st, FILE *out);
void  cmd_9asci(mac_state *st, FILE *out);
void  cmd_core(mac_state *st, FILE *out);

/* ---------------------------------------------------------------------- */
/* mac_stmt.c                                                              */
/* ---------------------------------------------------------------------- */

mac_sym_class leading_class(mac_state *st, const char *tok);
void          first_token(const char *stmt, char *op, const char **rest);
void          cmd_interval(mac_state *st, char *stmt, char *lt);
void          assemble_stmt(mac_state *st, char *stmt);
bool          eval_mark(mac_state *st, const char *expr);

#endif /* MAC_INTERNAL_H */
