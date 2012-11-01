#include "red-engine.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <getopt.h>
#include <syslog.h>

#include <sys/stat.h>
#include <signal.h>

/* Configuration, populated by `parse_options'.  */
static char *prog_name;
static int daemonize = true;
static struct red_conf conf =
{
  .port = 80,
  .home_dir = "/var/lib/" RED_IDENT,
};

static void parse_options (int, char **);
static void become_daemon (void);
static void stop_daemon (int);

/* The `red-engine' daemon.  */
int
main (int argc, char **argv)
{
  int syslog_option;

  parse_options (argc, argv);

  /* Set up logging.  */

  syslog_option = LOG_CONS | LOG_PID;

  if (!daemonize)
    /* TODO: LOG_PERROR isn't described in POSIX, but GNUs and BSDs know
    about it.  */
    syslog_option |= LOG_PERROR;

  openlog (RED_IDENT, syslog_option, LOG_DAEMON);

  if (daemonize)
    become_daemon ();

  if (red_init (conf) < 0)
    {
      syslog (LOG_ERR, _("Unable to initialize redirect engine.\n"));
      closelog ();
      exit (EXIT_FAILURE);
    }

  /* Wait for termination.  */
  signal (SIGTERM, stop_daemon);
  signal (SIGINT, stop_daemon); /* Ctrl+C when not running as a daemon.  */
  pause ();

  exit (EXIT_FAILURE); /* Never happens.  */
}


/* Handle/validate command-line arguments:  */
static void
parse_options (int argc, char **argv)
{
  static struct option long_options[] =
    {
      {"port", required_argument, NULL, 'p'}, /* Port to listen on.  */
      {"home-dir", required_argument, NULL, 'h'}, /* Working Directory.  */
      {"nodaemon", no_argument, &daemonize, false}, /* Don't daemonize.  */
      {0, 0, 0, 0}
    };
  int option;
  char *unparsed_int_string;
  long path_size;

  prog_name = argv[0];

  for (;;)
    {
      option = getopt_long (argc, argv, "p:n", long_options, NULL);

      /* No more command line arguments remain.  */
      if (option == -1)
        break;

      switch (option)
        {
          case 0:
            /* Option automatically set a flag. Nothing to do.  */
            break;

          case 'p':
            errno = 0;
            conf.port = strtol(optarg, &unparsed_int_string, 10);
            if (errno
                || *unparsed_int_string
                || unparsed_int_string == optarg
                || conf.port < 1)
              {
                fprintf (stderr, _("%s: Invalid port number '%s'.\n"),
                         prog_name, optarg);
                exit (EXIT_FAILURE);
              }
            break;

          case 'h':

            /* Store the absolute path corresponding to the --home-dir
            argument.  */

            /* An ugly way to find out how big our absolute path buffer
            should be.  */
            errno = 0;
            path_size = pathconf ("/", _PC_PATH_MAX) + 2;

            if ( (path_size < 0) && errno)
              {
                /* An error occured.  */
                fprintf (stderr, _("%s: Error calling `pathconf': '%s'\n"),
                         prog_name, strerror (errno));
                exit (EXIT_FAILURE);
              }

            if (path_size < 0)
              {
                /* No limit set. Set something big.  */
                path_size = 2048;
              }

            conf.home_dir = malloc (path_size);

            /* Finally, `realpath' gets the absolute path.  */
            if (! realpath (optarg, conf.home_dir))
              {
                fprintf (stderr, _("%s: Error calling `realpath': '%s'\n"),
                         prog_name, strerror (errno));
                exit (EXIT_FAILURE);
              }
            break;

          case '?':
            /* Something went wrong and `getopt' threw an error already.  */
            exit (EXIT_FAILURE);
            break;

          default:
            abort ();
            break;
        }
    }
  if (optind < argc)
    {
      fprintf (stderr, _("%s: Unknown option '%s'.\n"), prog_name, argv[optind]);
      exit (EXIT_FAILURE);
    }
  return;
}


/* Daemonize this process.  */
static void
become_daemon (void)
{
  /* TODO:
  1. Possibly replace with a call to:
  http://www.kernel.org/doc/man-pages/online/pages/man3/daemon.3.html
  
  2. Drop privileges; lock file */

  pid_t pid;

  if (getppid () == 1)
    /* Parent is `init' => I'm already a daemon.  */
    return;

  pid = fork ();
  /* PID is -1 when a `fork' fails.  */
  if (pid < 0)
    {
      syslog (LOG_ERR, _("Error calling `fork': '%s'\n"), strerror (errno));
      exit (EXIT_FAILURE);
    }

  /* Parent quits. Child continues.  */
  if (pid > 0)
    exit (EXIT_SUCCESS);

  umask (0);

  if (setsid () < 0)
    {
      syslog (LOG_ERR, _("Error calling `setsid': '%s'\n"), strerror (errno));
      exit (EXIT_FAILURE);
    }

  /* We're pretty sure "/" exists on all POSIXlikes.  */
  if ((chdir("/")) < 0)
    {
      syslog (LOG_ERR, _("Error calling `chdir': '%s'\n"), strerror (errno));
      exit(EXIT_FAILURE);
    }

  /* Be quiet.  */
  freopen ("/dev/null", "r", stdin);
  freopen ("/dev/null", "w", stdout);
  freopen ("/dev/null", "w", stderr);

  return;
}


/* Shut down cleanly.  */
static void
stop_daemon (int sig)
{
  syslog (LOG_NOTICE, _("Stopping.\n"));
  
  if (red_terminate () < 0)
    {
      syslog (LOG_ERR, _("Unable to terminate redirect engine.\n"));
      closelog ();
      exit (EXIT_FAILURE);
    }

  free (conf.home_dir);

  closelog ();
  exit (EXIT_SUCCESS);
}
