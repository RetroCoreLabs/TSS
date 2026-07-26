/**************************************************************************
** MAC-C DRIVER - command line front end                                  **
**                                                                       **
** Replays an ASSYSA-style build: define the preamble symbols, set the    **
** library marks, then assemble the source files in order and dump the    **
** symbol table in the archived ASYMB:SYMB format so it can be diffed     **
** against the golden dump from the original 1978 build.                  **
**                                                                       **
** Ronny Hansen                                                          **
***************************************************************************/
#include "mac.h"
#include <string.h>
#include <stdlib.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage: %s [options] file.symb [file.symb ...]\n"
        "  -m MARK      set a library mark (repeatable), e.g. -m CDC -m N10\n"
        "  -d NAME=VAL  define a symbol, VAL is octal, e.g. -d 9TTI=40\n"
        "  -l FILE      write the symbol list ()LIST format) to FILE\n"
        "  -L FILE      write an ADDRESS LISTING to FILE: one row per source\n"
        "               line, 'file:line  address  words  statement'. Not a\n"
        "               MAC feature - a mac-c aid for mapping source to core.\n"
        "  -o FILE      write the assembled image (MACIMG format)\n"
        "  -c FILE      write a CDC-disc overlay image (see\n"
        "               docs/TSS-ARCHITECTURE.md (overlay chapter)). Requires an overlay/OVERX\n"
        "               build so OVDK/VORS/RQR/VOR are defined.\n"
        "  -b FILE      write a bootable BPUN paper tape\n"
        "  -e ENTRY     BPUN autostart address: an octal number or a symbol\n"
        "               name (e.g. -e ISTRT). Sets the tape's start address.\n"
        "  -u           report symbols still undefined at the end\n",
        argv0);
}

int main(int argc, char **argv)
{
    mac_state st;
    mac_init(&st);

    const char *listfile = NULL;
    const char *asmlistfile = NULL;   /* -L: source-to-address listing */
    const char *imgfile = NULL;
    const char *cdcfile = NULL;   /* -c: CDC-disc overlay image */
    const char *bpunfile = NULL;
    const char *entryarg = NULL;   /* -e: BPUN autostart (symbol or octal) */
    bool report_undef = false;
    int nfiles = 0;

    /* first pass over argv: options (marks/defines must precede assembly) */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
        {
            mac_reference_mark(&st, argv[++i]);
        }
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
        {
            char buf[128];
            strncpy(buf, argv[++i], sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            char *eq = strchr(buf, '=');
            if (eq == NULL)
            {
                fprintf(stderr, "bad -d (need NAME=VAL): %s\n", buf);
                return 2;
            }
            *eq = '\0';
            mac_define(&st, buf, (uint16_t)strtol(eq + 1, NULL, 8));
        }
        else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc)
        {
            asmlistfile = argv[++i];
        }
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
        {
            listfile = argv[++i];
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            imgfile = argv[++i];
        }
        else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cdc") == 0)
                 && i + 1 < argc)
        {
            cdcfile = argv[++i];
        }
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
        {
            bpunfile = argv[++i];
        }
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc)
        {
            entryarg = argv[++i];   /* resolved after assembly (symbols known) */
        }
        else if (strcmp(argv[i], "-u") == 0)
        {
            report_undef = true;
        }
        else if (argv[i][0] == '-')
        {
            usage(argv[0]);
            return 2;
        }
    }

    /* -L must be open before any line is processed, since the listing is
     * produced as a side effect of assembly, not afterwards.              */
    if (asmlistfile != NULL && !mac_open_asm_listing(&st, asmlistfile))
    {
        fprintf(stderr, "cannot create %s\n", asmlistfile);
        return 1;
    }

    /* second pass: assemble every non-option argument in order */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "-L") == 0 ||
            strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-e") == 0 ||
            strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cdc") == 0)
        {
            i++; /* skip the option argument */
            continue;
        }
        if (argv[i][0] == '-')
        {
            continue;
        }
        if (!mac_assemble_file(&st, argv[i]))
        {
            return 1;
        }
        nfiles++;
    }

    if (nfiles == 0)
    {
        usage(argv[0]);
        return 2;
    }

    /* End-of-assembly sweep: an undefined symbol used in the OPCODE position
     * of a statement with an operand (e.g. the N10/drum build's "RGDIV ST")
     * is a hard error - MAC's PLAIN fallback would otherwise emit silent
     * garbage for it. Run AFTER every file so genuine forward references are
     * already resolved. This bumps st.errors, so the final return below fails
     * the build (and the DIAGNOSTICS line + ERROR text reach build.err).     */
    mac_check_undefined_opcodes(&st);

    /* flush and close anything )9ASSM opened, so the object stream (which
     * is where )LIST writes the symbol table) actually reaches the disk */
    mac_close_streams(&st);

    fprintf(stderr, "%06o DIAGNOSTICS\n", st.errors);

    if (report_undef)
    {
        mac_report_undefined(&st, stderr);
    }
    if (listfile != NULL)
    {
        FILE *f = fopen(listfile, "wb");
        if (f == NULL)
        {
            fprintf(stderr, "cannot write %s\n", listfile);
            return 1;
        }
        mac_list_symbols(&st, f);
        fclose(f);
    }
    if (imgfile != NULL && !mac_write_image(&st, imgfile))
    {
        fprintf(stderr, "cannot write %s\n", imgfile);
        return 1;
    }
    /* CDC-disc overlay image: the overlays were staged into the VOR windows
     * by )9MOVE inside the OVERX macro during assembly; this dumps them onto
     * the disc sectors the run-time reader expects. See TSS-ARCHITECTURE.md (overlay chapter)
     * and mac_write_cdc_disc. */
    if (cdcfile != NULL && !mac_write_cdc_disc(&st, cdcfile))
    {
        fprintf(stderr, "cannot write CDC image %s\n", cdcfile);
        return 1;
    }
    /* Resolve the BPUN autostart entry now that all symbols are defined.
     * -e accepts an octal address (leading digit) or a symbol name. */
    uint16_t entry = 0;
    if (entryarg != NULL)
    {
        if (entryarg[0] >= '0' && entryarg[0] <= '9')
        {
            entry = (uint16_t)strtol(entryarg, NULL, 8);
        }
        else if (sym_is_defined(&st, entryarg))
        {
            entry = sym_lookup_value(&st, entryarg);
        }
        else
        {
            fprintf(stderr, "warning: -e entry '%s' is undefined; "
                            "BPUN written with no autostart\n", entryarg);
        }
    }
    if (bpunfile != NULL && !mac_write_bpun(&st, bpunfile, entry))
    {
        fprintf(stderr, "cannot write %s\n", bpunfile);
        return 1;
    }
    return st.errors == 0 ? 0 : 1;
}
