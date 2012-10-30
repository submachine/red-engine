#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <unistd.h>
#include <getopt.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <microhttpd.h>
#include <glib/gi18n.h>

#include "red-engine.h"

#define RED_IDENT "red-engine"

char *prog_name;
long int port = 80;
int daemonize = true;
struct MHD_Daemon *red_mhd_daemon;

static void parse_options (int, char **);
static void become_daemon (void);
static void red_shutdown (int);

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

  /* Ask MicroHTTPD to start listening.  */


  red_mhd_daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY,
                                     port,
                                     NULL, /* Accept policy callback.  */
                                     NULL, /* Accept policy cb arg.  */
                                     &red_handler, /* Handler callback.  */
                                     NULL, /* Handler cb arg.  */
                                     MHD_OPTION_END);

  if (red_mhd_daemon == NULL)
    {
      syslog (LOG_ERR, _("Unable to start microhttpd daemon.\n"));
      closelog ();
      exit (EXIT_FAILURE);
    }

  syslog (LOG_NOTICE, _("Started listening.\n"));

  /* Wait for termination.  */
  signal (SIGTERM, red_shutdown);
  signal (SIGINT, red_shutdown); /* Ctrl+C when not running as a daemon.  */
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
      {"nodaemon", no_argument, &daemonize, false}, /* Don't daemonize.  */
      {0, 0, 0, 0}
    };
  int option;
  char *unparsed_int_string;

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
            port = strtol(optarg, &unparsed_int_string, 10);
            if (errno
                || *unparsed_int_string
                || unparsed_int_string == optarg
                || port < 1)
              {
                fprintf (stderr, _("%s: Invalid port number '%s'.\n"),
                         prog_name, optarg);
                exit (EXIT_FAILURE);
              }
            break;

          case '?':
            /* Something went wrong and `getopt' threw an error already.  */
            exit (EXIT_FAILURE);

          default:
            abort ();
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

  freopen ("/dev/null", "r", stdin);
  freopen ("/dev/null", "w", stdout);
  freopen ("/dev/null", "w", stderr);

  return;
}

/* Shut down cleanly.  */
static void
red_shutdown (int sig)
{
  syslog (LOG_NOTICE, _("Stopping.\n"));
  MHD_stop_daemon (red_mhd_daemon);
  closelog ();
  exit (EXIT_SUCCESS);
}
