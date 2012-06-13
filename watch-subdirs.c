#define _BSD_SOURCE

#include <err.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>

static pthread_mutex_t build_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t build_cond = PTHREAD_COND_INITIALIZER;
static uint64_t dirtyPaths; /* XXX: Supports max 64 paths */

static char **paths;
static int pathCount;

extern char **environ;

static int WatchSubdirs_printHelp;
static int WatchSubdirs_printVersion;
static const char *WatchSubdirs_makefile = "Makefile";

static struct option WatchSubdirs_longOptions[] =
{
    { "makefile", required_argument, 0, 'm' },
    { "help",     no_argument, &WatchSubdirs_printHelp, 1 },
    { "version",  no_argument, &WatchSubdirs_printVersion, 1 },
    { 0, 0, 0, 0 }
};

static void *
WatchSubdirs_BuildThread (void *arg)
{
  for (;;)
    {
      pid_t child;
      int status, pathIndex;

      pthread_mutex_lock (&build_mutex);

      while (!dirtyPaths)
        pthread_cond_wait (&build_cond, &build_mutex);

      pthread_mutex_unlock (&build_mutex);

      /* Defer building for a little while, just in case we get multiple events in rapid succession */
      usleep (100000);

      while (0 != (pathIndex = ffs (dirtyPaths)))
        {
          --pathIndex;

          if (-1 == (child = fork ()))
            err (EXIT_FAILURE, "fork failed");

          if (!child)
            {
              char *args[7];

              args[0] = "/usr/bin/make";
              args[1] = "-s";
              args[2] = "-f";
              args[3] = (char *) WatchSubdirs_makefile;
              args[4] = "-C";
              args[5] = paths[pathIndex];
              args[6] = 0;

              execve (args[0], args, environ);

              exit (EXIT_FAILURE);
            }

          waitpid (child, &status, 0);

          pthread_mutex_lock (&build_mutex);
          dirtyPaths &= ~(1 << pathIndex);
          pthread_mutex_unlock (&build_mutex);
        }
    }
}

static void
WatchSubdirs_StartBuildThread (void)
{
  pthread_t threadHandle;

  pthread_create (&threadHandle, 0, WatchSubdirs_BuildThread, 0);
  pthread_detach (threadHandle);
}

int
main (int argc, char **argv)
{
  int i, inotifyFD;
  char buffer[4096];
  size_t bufferFill = 0;
  ssize_t result;

  int *watchDescriptors;

  if (-1 == (inotifyFD = inotify_init1(IN_CLOEXEC)))
    err (EXIT_FAILURE, "inotify_init1 failed");

  while(-1 != (i = getopt_long(argc, argv, "", WatchSubdirs_longOptions, NULL)))
    {
      switch(i)
        {
        case 0:

          break;

        case 'm':

          WatchSubdirs_makefile = optarg;

          break;

        case '?':

          fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);

          return EXIT_FAILURE;
        }
    }

  if(WatchSubdirs_printHelp)
    {
      fprintf(stdout,
              "Usage: %s [OPTION]... [DIRECTORY]...\n"
              "\n"
              "  -m, --makefile=<filename>  set path of Makefile\n"
              "      --help     display this help and exit\n"
              "      --version  display version information\n"
              "\n"
              "Report bugs to <morten.hustveit@gmail.com>\n", argv[0]);

      return EXIT_SUCCESS;
    }

  paths = argv + optind;
  pathCount = argc - optind;

  if (!(watchDescriptors = calloc (pathCount, sizeof (*watchDescriptors))))
    err (EXIT_FAILURE, "calloc failed");

  for (i = 0; i < pathCount; ++i)
    {
      if(-1 == (watchDescriptors[i] = inotify_add_watch (inotifyFD, paths[i], IN_CLOSE_WRITE | IN_MOVED_TO | IN_ONLYDIR)))
        err (EXIT_FAILURE, "inotify_add_watch: failed to start watching `%s'", paths[i]);
    }

  WatchSubdirs_StartBuildThread ();

  for (;;)
    {
      if (-1 == (result = read (inotifyFD, buffer + bufferFill, sizeof (buffer) - bufferFill)))
        err (EXIT_FAILURE, "read failed on inotify descriptor");

      bufferFill += result;

      while (bufferFill > sizeof (struct inotify_event))
        {
          struct inotify_event *event;
          size_t eventSize;

          event = (struct inotify_event *) buffer;
          eventSize = sizeof(*event) + event->len;

          if (event->name[0] != '.')
            {
              int pathIndex;

              for (pathIndex = 0; pathIndex < pathCount; ++pathIndex)
                {
                  if (watchDescriptors[pathIndex] == event->wd)
                    {
                      pthread_mutex_lock (&build_mutex);
                      dirtyPaths |= (1 << pathIndex);
                      pthread_cond_signal (&build_cond);
                      pthread_mutex_unlock (&build_mutex);

                      break;
                    }
                }
            }

          bufferFill -= eventSize;
          memmove (buffer, buffer + eventSize, bufferFill);
        }
    }
}
