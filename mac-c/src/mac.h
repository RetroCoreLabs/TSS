/**************************************************************************
** MAC-C - HOST-SIDE REIMPLEMENTATION OF THE ND MAC ASSEMBLER (1978)     **
**                                                                       **
** A C99 cross-assembler for the MAC dialect used by the NORD TSS 3.0    **
** sources (TSS1:SYMB..TSS5:SYMB, MINIT, TDUMP). Written from three      **
** authorities:                                                          **
**   1. ND-60.096.01 "MAC Interactive Assembly and Debugging System"     **
**      (the language spec).                                             **
**   2. The permanent symbol table extracted from the real MAC.BPUN      **
**      binary MAC.BPUN ("MAC - 14. MARS 1978"): 3-word                **
**      entries at 0xE9B3, names packed 5 chars x 6 bits. Every opcode   **
**      value in mac_permsym[] is byte-verified against that table.      **
**   3. The TSS sources themselves as the target corpus; the archived    **
**      ASYMB:SYMB / BSYMB:SYMB symbol dumps are the golden oracle:      **
**      a correct assembly of ASSYSA must reproduce them exactly.        **
**                                                                       **
** Core model: ONE-PASS with undefined-symbol fixup chains, like the     **
** real MAC. Two-pass would break the )KILL / )PCL time-ordered local    **
** symbol semantics that TSS leans on heavily (per-routine reuse of      **
** TCL/TCT etc.).                                                        **
**                                                                       **
** Ronny Hansen                                                          **
***************************************************************************/
#ifndef MAC_H
#define MAC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ND memory is 64K x 16-bit words. The whole assembly happens into this
 * image, exactly like real MAC assembling into machine memory.           */
#define MAC_MEM_WORDS 65536

/* MAC symbols are significant to 5 characters (verified: the binary's
 * name packing is 5 x 6 bits). We keep 6 + NUL for readable storage.     */
#define MAC_SYM_LEN 6

/* -------- symbol classification -------------------------------------- */
/* How a statement whose FIRST token is this symbol assembles its operand.
 * This distinction cannot be derived from the value alone: STZ has value
 * 000000, so "STZ LABEL" and a bare "LABEL" data word are numerically
 * identical expressions - only the leading-token class tells them apart. */
typedef enum
{
    MAC_CLS_PLAIN = 0,   /**< pure 16-bit sum of all terms (data, COPY, IOX, MON...) */
    MAC_CLS_MRI,         /**< memory-reference instr: address operand -> P-relative
                              8-bit displacement; ,B/,X operand -> direct 8-bit disp */
    MAC_CLS_JUMP8,       /**< conditional jumps JAP..JXN: 8-bit P-relative disp only  */
    MAC_CLS_ARG8         /**< argument instructions SAA/AAA/...: operand & 0377       */
} mac_sym_class;

/* -------- permanent (built-in) symbol table -------------------------- */
/* Populated in mac_permsym.c from the real MAC.BPUN binary. */
typedef struct
{
    const char   *name;
    uint16_t      value;
    mac_sym_class cls;
} mac_permsym_entry;

extern const mac_permsym_entry MAC_PERMSYM[];
extern const size_t            MAC_PERMSYM_COUNT;

/* -------- symbol table entry ------------------------------------------ */
typedef struct mac_sym
{
    char           name[MAC_SYM_LEN + 1]; /**< truncated to 5 significant chars      */
    uint16_t       value;                 /**< current value (if defined)            */
    bool           defined;               /**< false = referenced but not yet defined */
    bool           address;               /**< value is an address (label / *), which
                                               makes MRI operands P-relative          */
    bool           permanent;             /**< from the built-in table; )KILL-able but
                                               never listed by )LIST                  */
    mac_sym_class  cls;                   /**< statement class when leading token     */
    uint32_t       def_seq;               /**< definition sequence number, for )PCL   */
    bool           used_as_opcode;        /**< set when this (still-undefined) symbol
                                               appeared in the OPCODE position of a
                                               statement that also had an operand term
                                               (e.g. "RGDIV ST"). If it is STILL
                                               undefined at end of assembly it is a
                                               hard error: MAC's PLAIN fallback would
                                               otherwise silently emit garbage for it.
                                               A forward reference clears this by being
                                               defined; a bare library mark never sets
                                               it (single token, no operand).          */
    bool           used_as_operand;       /**< set when this (still-undefined) symbol
                                               was consumed as the VALUE OPERAND of a
                                               DEFINED instruction (MRI/JUMP8/ARG8),
                                               e.g. "SAT STR1" or "STT STRX,B". MAC's
                                               undefined->0 fallback would otherwise
                                               silently emit a wrong operand (SAT 0),
                                               invisible to the golden ADDRESS dumps.
                                               If STILL undefined at end of assembly it
                                               is a hard error. A forward reference
                                               clears it by being defined; library
                                               marks (used only in "MARK conditionals
                                               or bare mark-declaration lines, never as
                                               an instruction operand) never set it.    */
    struct mac_sym *next;
} mac_sym;

/* -------- fixup (undefined-reference backpatch) ----------------------- */
/* Real MAC chains undefined references through memory; we keep an explicit
 * list per symbol instance. When the symbol is defined, every pending use
 * is patched according to how it was consumed.                            */
typedef enum
{
    MAC_FIX_FULL,   /**< add full 16-bit value into the word (the word
                         already holds the expression's constant part)    */
    MAC_FIX_PREL8,  /**< value is an address: patch
                         (((val + addend) - pc) & 0377)                   */
    MAC_FIX_ARG8    /**< patch ((val + addend) & 0377) into the low byte  */
} mac_fix_kind;

typedef struct mac_fixup
{
    uint16_t         addr;   /**< word that needs patching                 */
    uint16_t         pc;     /**< location counter of the instruction, for
                                  P-relative displacement computation      */
    uint16_t         addend; /**< constant part of the expression the
                                  undefined symbol appeared in, e.g. the 2
                                  of "JMP RFN+2" when RFN is a forward
                                  reference. MUST be added to the symbol
                                  value when patching PREL8/ARG8 words.
                                  Dropping it silently retargeted every
                                  forward "JMP SYM+n" to SYM: TSS2 ROBJ's
                                  "SAA 11; JMP RFN+2" fell into "SAA -1",
                                  turning error 9 (index out of range)
                                  into -1 (empty object), which made LOGON's
                                  ()SCRATCH open enumerate the file
                                  directory forever = the login hang.
                                  (FULL fixups instead keep the constant
                                  pre-stored in the word and add to it.)  */
    mac_fix_kind     kind;
    struct mac_fixup *next;
} mac_fixup;

/* -------- literal pool ------------------------------------------------ */
/* "(expr" allocates a literal cell; )FILL dumps pending cells at the
 * current location. Identical values share one cell (the real MAC default;
 * !9LITR duplication mode is not implemented).                            */
#define MAC_MAX_LITERALS 128
typedef struct
{
    uint16_t value;       /**< resolved value (or 0 while unresolved)      */
    bool     resolved;
    char     sym[MAC_SYM_LEN + 1]; /**< if unresolved: the symbol to add   */
    uint16_t extra;       /**< constant part added to the symbol           */
    uint16_t cell;        /**< assigned memory address once dumped         */
    bool     placed;
} mac_literal;

/* -------- macros ()MCDEF) --------------------------------------------- */
#define MAC_MAX_MACROS      64
#define MAC_MACRO_BODY_MAX  8192
#define MAC_MACRO_ARGS_MAX  8

/* Most words any one source line can contribute to the -L address listing.
 * A packed text line ('...') is the worst case in this corpus; 64 covers
 * every line in TSS1-5, MINIT and TDUMP. Beyond it the row is marked "...". */
#define MAC_ASM_LIST_MAX    64
typedef struct
{
    char name[MAC_SYM_LEN + 1];
    char params[MAC_MACRO_ARGS_MAX][8]; /**< "$A1" style dummy names        */
    int  nparams;
    char *body;                         /**< heap copy, lines end with \n   */
} mac_macro;

/* -------- assembler state --------------------------------------------- */
typedef struct
{
    uint16_t    mem[MAC_MEM_WORDS];
    uint8_t     used[MAC_MEM_WORDS];  /**< track emitted words for BPUN range */
    uint16_t    loc;                  /**< the location counter '*'            */
    uint16_t    lo_used, hi_used;

    mac_sym    *symtab;               /**< single list: permanent + local      */
    uint32_t    def_seq;              /**< monotonic definition counter        */

    /* undefined-reference fixups keyed by symbol name (symbol entries with
     * defined == false own their pending chain)                              */
    mac_fixup  *fixups_pool;

    mac_literal lits[MAC_MAX_LITERALS];
    int         nlits;

    mac_macro   macros[MAC_MAX_MACROS];
    int         nmacros;

    /* B and X location counters ()9SET). When a ',B' or ',X' operand names
     * an ADDRESS, the displacement is (address - the matching counter);
     * both are zero after mac_init and after )CLEAR, which reproduces the
     * "old method" of writing plain displacements (ND-60.096.01 sec 3.2.4
     * )9SET).                                                             */
    uint16_t    bcounter;
    uint16_t    xcounter;

    /* interval set by the '<' command, used by )ZERO, )PRINT, )PUNCH,
     * )9ASCI, )BPUN and )CHANGE                                           */
    uint16_t    ilow;
    uint16_t    ihigh;

    /* )CHANGE operands (the option adds these three cells to MAC)         */
    uint16_t    chg_old;
    uint16_t    chg_new;
    uint16_t    chg_mask;

    bool        write_mode;   /**< )WRTM enables )WRITE, )NWRT disables it */
    bool        parity_check; /**< )9PARI toggles input parity checking    */
    bool        dup_literals; /**< )9LITR toggles literal duplication      */
    bool        symbolic_out; /**< )SETSM / )RESSM printout mode           */

    /* include depth for nested )9ASSM */
    int         include_depth;

    /* Floating-point variant for '[' constants. false (default) = the
     * optional 32-bit format, 2 words per constant, which is what the
     * archived ASYMB:SYMB golden dump was built with. true = the standard
     * 48-bit format, 3 words (the variant MAC.BPUN itself is).
     * See the '[' handler in mac.c for the evidence behind the default.   */
    bool        float48;

    /* macro capture state: between ')MCDEF NAME $P,...' and the ']'
     * termination character every line is swallowed into the body.        */
    bool        capturing;
    int         cap_index;    /**< macro being captured                    */
    size_t      cap_len;      /**< bytes used in the body buffer           */
    int         expand_depth; /**< guards runaway recursive expansion      */

    /* library-mark conditional state: '"EXPR' opens a region assembled only
     * when EXPR is true; a bare '"' line re-enables assembly. Flat, not
     * nested - matches every use in the TSS corpus.                          */
    bool        cond_active;

    /* diagnostics */
    int         errors;
    int         warnings;
    const char *cur_file;
    int         cur_line;

    /* MAC's three streams. The source stream is the file being read (see
     * mac_assemble_file / nested )9ASSM). The list stream receives )WRITE,
     * )WRUS, )WLOC, )WMNE, )PRINT, )9MSG and )CORE; the object stream
     * receives )LIST, )PUNCH and )BPUN (ND-60.096.01 sec 3.1.3). NOTE:
     * )9ASCI goes to the LIST stream, not the object stream - the per-command
     * text (manual line 2554) is explicit; an earlier version of this comment
     * grouped it with the object stream and was wrong.
     * A NULL stream means the dummy device (output discarded); 'listing'
     * doubles as the list stream for backwards compatibility.             */
    FILE       *listing;              /**< list stream (NULL = dummy)         */
    FILE       *object;               /**< object stream (NULL = dummy)       */
    bool        own_listing;          /**< close on mac_close_streams         */
    bool        own_object;
    bool        end_of_file_seen;     /**< set by )LINE                       */

    /* -------- address listing (-L) ------------------------------------
     * NOT a MAC feature: real MAC has no such mode. This is a mac-c
     * debugging aid that answers "what address is this source line at",
     * which no other artifact in the project can. emit() records every
     * (address, word) pair it writes for the line currently being
     * processed, and mac_line() flushes one row per source line.        */
    FILE       *asm_list;             /**< address listing (NULL = off)       */
    bool        own_asm_list;         /**< close on mac_close_streams         */
    uint16_t    al_addr[MAC_ASM_LIST_MAX]; /**< addresses written this line   */
    uint16_t    al_word[MAC_ASM_LIST_MAX]; /**< words written this line       */
    int         al_n;                 /**< how many, -1 once overflowed       */
    int         al_depth;             /**< mac_line recursion (macro/)9ASSM)  */
} mac_state;

/* -------- public API --------------------------------------------------- */

/** Initialize state and install the permanent symbol table (values
 *  byte-verified against MAC.BPUN's table at 0xE9B3). */
void mac_init(mac_state *st);

/** Assemble one source file (a TSSn:SYMB style stream). Returns false on
 *  hard I/O failure; assembly errors are counted in st->errors. Processing
 *  stops at )LINE or end of file, like the real MAC returning to console. */
bool mac_assemble_file(mac_state *st, const char *path);

/** Feed a single source line (the console-input equivalent). Exposed so a
 *  driver can replay ASSYSA-style preamble lines verbatim. */
void mac_line(mac_state *st, const char *line);

/** Open the -L address listing. Writes one row per source line:
 *  `file:line  address  words...  source`. Returns false if the file cannot
 *  be created. Closed by mac_close_streams(). */
bool mac_open_asm_listing(mac_state *st, const char *path);

/** Define a symbol programmatically (driver preamble, -D options). */
void mac_define(mac_state *st, const char *name, uint16_t value);

/** Reference a symbol without defining it - this is how library marks are
 *  made "true" (mark truth = present in the undefined symbol table). */
void mac_reference_mark(mac_state *st, const char *name);

/** )LIST equivalent: dump non-permanent defined symbols in the exact
 *  archived ASYMB:SYMB format ("NNNNN=VVVVVV", name right-justified to 5). */
void mac_list_symbols(mac_state *st, FILE *out);

/** Report symbols that remain referenced-but-undefined (UDEF check). */
void mac_report_undefined(mac_state *st, FILE *out);

/** End-of-assembly sweep: any symbol still undefined that was used in the
 *  OPCODE position of a statement with an operand (used_as_opcode) is a hard
 *  error ("undefined instruction: NAME"), incrementing st->errors so the
 *  build fails instead of silently emitting garbage. Must be called AFTER all
 *  source is assembled (so genuine forward references have had a chance to be
 *  defined). Returns the number of undefined instructions found. */
int mac_check_undefined_opcodes(mac_state *st);

/** Value of a defined symbol, or 0 if absent/undefined (test + driver aid). */
uint16_t sym_lookup_value(mac_state *st, const char *name);

/** True if the symbol exists AND is defined. */
bool sym_is_defined(mac_state *st, const char *name);

/** Write a bootable BPUN tape: null leader, octal-ASCII section that
 *  deposits the 36-word block loader at 164316 (verbatim words recovered
 *  from MAC.BPUN) and starts it, then one binary block covering the used
 *  range, checksum, trailer. 'entry' goes into the loader start cell
 *  (164361); pass 0 for "no autostart" exactly like the archived tape.   */
bool mac_write_bpun(mac_state *st, const char *path, uint16_t entry);

/** Write the used range to a simple, self-describing image file:
 *  6-byte ASCII magic "MACIMG", then base(2), count(2), then count
 *  big-endian words. Round-trips exactly with mac_read_image. */
bool mac_write_image(mac_state *st, const char *path);

/** Write a BPUN tape for an explicit address range to an already-open
 *  stream (the ")BPUN" path). Returns false if the stream is the dummy
 *  device. See mac_write_bpun for the format. */
bool mac_write_bpun_range(mac_state *st, FILE *f, uint16_t lo, uint16_t hi,
                          uint16_t entry);

/** Close any streams MAC opened itself ()9ASSM). Safe to call twice. */
void mac_close_streams(mac_state *st);

/** Read a BPUN tape produced by mac_write_bpun (or by the real MAC) back
 *  into memory: skips the leader and octal bootstrap, loads the binary
 *  block and verifies the additive checksum. This is the ')9READ' path.
 *  Returns false on I/O error, malformed tape, or checksum mismatch. */
bool mac_read_bpun(mac_state *st, const char *path);

/** Write a CDC-disc overlay image: reproduces the run-time overlay-load
 *  contract (sector = OVDK + 2*overlay, two 256-word sectors per overlay,
 *  big-endian words at byte offset sector*512) so the running TSS overlay
 *  reader (routine S5) finds each overlay. Resolves OVDK/VORS/RQR/VOR from
 *  the symbol table and reads the staged overlay windows from the assembled
 *  image (populated by )9MOVE / OVERX). Emits a {window,sector,first-words}
 *  table to stderr for boot verification. Returns false if the overlay
 *  symbols are absent or the file cannot be written.
 *  See docs/TSS-ARCHITECTURE.md (overlay chapter). */
bool mac_write_cdc_disc(mac_state *st, const char *path);

/** Convert a LOGICAL overlay disc sector to the PHYSICAL CDC sector, exactly as
 *  the running TSS DKADR routine does (src/TSS1.SYMB:3589-3646, unit 0). This
 *  is the mapping mac_write_cdc_disc uses to place each overlay where the
 *  reader will look for it: DKADR(L) = (8*floor(L*65537/12) + 64*L) & 0xFFFF.
 *  [VERIFIED against the live nd100x DKADR trace: DKADR(0244)=0077300.]
 *  NOTE: not monotonic in L. See cdc_dkadr() in mac.c for the full derivation. */
uint32_t cdc_dkadr(uint16_t logical_sector);

/** Load an image written by mac_write_image back into memory at its base,
 *  marking those words used and updating lo/hi. Returns false on bad magic
 *  or I/O error. */
bool mac_read_image(mac_state *st, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* MAC_H */
