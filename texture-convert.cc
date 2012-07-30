#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <pvrtc.h>

#include "png-wrapper.h"

static int Texture_printHelp;
static int Texture_printVersion;
static const char *Texture_format = "pvrtc";

static struct option Texture_longOptions[] =
{
    { "format", required_argument, 0, 'f' },
    { "help",     no_argument, &Texture_printHelp, 1 },
    { "version",  no_argument, &Texture_printVersion, 1 },
    { 0, 0, 0, 0 }
};

struct TEXTURE_PVRTCHeader
{
  uint32_t height;
  uint32_t width;
  uint32_t numMipmaps;
  uint32_t flags;
  uint32_t dataLength;
  uint32_t bpp;
  uint32_t bitmaskRed;
  uint32_t bitmaskGreen;
  uint32_t bitmaskBlue;
  uint32_t bitmaskAlpha;
  uint32_t pvrTag;
  uint32_t numSurfs;
};

int
main (int argc, char **argv)
{
  void *input, *output;
  unsigned int width, height, output_size;
  int use2bit = 1;

  int i;

  while (-1 != (i = getopt_long (argc, argv, "", Texture_longOptions, NULL)))
    {
      switch (i)
        {
        case 0:

          break;

        case 'f':

          Texture_format = optarg;

          break;

        case '?':

          fprintf (stderr, "Try `%s --help' for more information.\n", argv[0]);

          return EXIT_FAILURE;
        }
    }

  if (Texture_printHelp)
    {
      fprintf (stdout,
              "Usage: %s [OPTION]... IMAGE\n"
              "\n"
              "  -f, --format=FORMAT      set output format (%s)\n"
              "      --help     display this help and exit\n"
              "      --version  display version information\n"
              "\n"
              "Report bugs to <morten.hustveit@gmail.com>\n", argv[0], Texture_format);

      return EXIT_SUCCESS;
    }

  if (Texture_printVersion)
    {
      puts (PACKAGE_STRING);

      return EXIT_SUCCESS;
    }

  if (optind + 1 != argc)
    errx (EX_USAGE, "Usage: %s [OPTION]... IMAGE", argv[0]);

  if (-1 == png_load (argv[optind], &input, &width, &height))
    errx (EXIT_FAILURE, "png_load failed");

  if (!strcmp (Texture_format, "pvrtc"))
    {
      struct TEXTURE_PVRTCHeader header;
      int fdNull, stdoutBackup;

      output_size = pvrtc_size (width, height, 1 /* mip map */, use2bit);

      output = malloc (output_size);

      /* Hack to quench pvrtc_compress' progress reports */
      fdNull = open ("/dev/null", O_WRONLY);
      stdoutBackup = dup (1);
      dup2 (fdNull, 1);
      close (fdNull);

      pvrtc_info_output (NULL);
      pvrtc_compress (input, output, width, height,
                      1 /* mip map */,
                      1 /* alpha on */,
                      use2bit,
                      0 /* quality (0-4) */);

      dup2 (stdoutBackup, 1);

      memset (&header, 0, sizeof (header));
      header.width = width;
      header.height = height;

      fwrite (&header, 1, sizeof (header), stdout);
    }
  else
    err (EX_USAGE, "Unknown format '%s'", Texture_format);

  fwrite (output, 1, output_size, stdout);

  return EXIT_SUCCESS;
}
