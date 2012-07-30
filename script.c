#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <sysexits.h>

#include "script.h"
#include "script-vm.h"

static int Script_printHelp;
static int Script_printVersion;
static const char *Script_format = "binary";
static int Script_pointerSize = 32;

static struct option Script_longOptions[] =
{
    { "format", required_argument, 0, 'f' },
    { "pointer-size", required_argument, 0, 'p' },
    { "help",     no_argument, &Script_printHelp, 1 },
    { "version",  no_argument, &Script_printVersion, 1 },
    { 0, 0, 0, 0 }
};

int
main (int argc, char **argv)
{
  struct script_parse_context context;
  int i;

  while(-1 != (i = getopt_long(argc, argv, "", Script_longOptions, NULL)))
    {
      switch(i)
        {
        case 0:

          break;

        case 'f':

          Script_format = optarg;

          break;

        case 'p':

          Script_pointerSize = atoi (optarg);

          break;

        case '?':

          fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);

          return EXIT_FAILURE;
        }
    }

  if (Script_printHelp)
    {
      fprintf(stdout,
              "Usage: %s [OPTION]... SCRIPT\n"
              "\n"
              "  -f, --format=FORMAT      set output format\n"
              "  -p, --pointer-size=BITS  set size of pointers on target platform\n"
              "      --help     display this help and exit\n"
              "      --version  display version information\n"
              "\n"
              "Report bugs to <morten.hustveit@gmail.com>\n", argv[0]);

      return EXIT_SUCCESS;
    }

  if (Script_printVersion)
    {
      puts (PACKAGE_STRING);

      return EXIT_SUCCESS;
    }

  if (optind + 1 < argc)
    err (EX_USAGE, "Usage: %s [OPTION]... [SCRIPT]", argv[0]);
  else if (optind == argc)
    script_parse_file (&context, stdin);
  else
    {
      FILE *input;

      if(!(input = fopen(argv[optind], "r")))
        err (EXIT_FAILURE, "%s: failed to open `%s' for reading", argv[0], argv[optind]);

      script_parse_file (&context, input);

      fclose (input);
    }

  SCRIPT_Optimize (&context);

  if (!strcmp (Script_format, "binary"))
    script_dump_binary (&context, Script_pointerSize);
  else if (!strcmp (Script_format, "html"))
    script_dump_html (&context);
  else
    {
      fprintf (stderr, "Unknown format: %s\n", Script_format);

      return EXIT_FAILURE;
    }

  arena_free(&context.statement_arena);

  return EXIT_SUCCESS;
}
