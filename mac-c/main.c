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
        "  -o FILE      write the assembled image (MACIMG format)\n"
        "  -b FILE      write a bootable BPUN paper tape\n"
        "  -u           report symbols still undefined at the end\n",
        argv0);
}

int main(int argc, char **argv)
{
    mac_state st;
    mac_init(&st);

    const char *listfile = NULL;
    const char *imgfile = NULL;
    const char *bpunfile = NULL;
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
        else if (strcmp(argv[i], "-l") == 0 && i + 1 < argc)
        {
            listfile = argv[++i];
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
        {
            imgfile = argv[++i];
        }
        else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc)
        {
            bpunfile = argv[++i];
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

    /* second pass: assemble every non-option argument in order */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "-o") == 0 ||
            strcmp(argv[i], "-b") == 0)
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
    if (bpunfile != NULL && !mac_write_bpun(&st, bpunfile, 0))
    {
        fprintf(stderr, "cannot write %s\n", bpunfile);
        return 1;
    }
    return st.errors == 0 ? 0 : 1;
}
