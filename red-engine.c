#include "red-engine.h"

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <db.h>
#include <glib/gi18n.h>


#define LOG_AND_RET(err_str, ret_val) \
do { syslog (LOG_ERR, "%s: %s\n", __func__, _(err_str)); return ret_val; } while (0)

/* The core of the redirect engine.  */

/* Handle for MicroHTTPD.  */
struct MHD_Daemon *mhd_daemon;

/* Handles for Berkeley DB.  */
static DB_ENV *db_env;
static DB *db;

/* An HTTP 403 - 'Method Not Allowed' response. For requests other than GET
and HEAD.  */
#define BODY_MNA "<html><head><title>405 Method Not Allowed</title>" \
                 "</head><body>Method Not Allowed</body></html>"
static struct MHD_Response *response_MNA = NULL;

/* An HTTP 404 - 'Not Found' response. For non-existent redirect URLs.  */
#define BODY_NF "<html><head><title>404 Not Found</title>" \
                "</head><body>Not Found</body></html>"
static struct MHD_Response *response_NF = NULL;

/* An HTTP 500 - 'Internal Server Error' response.  */
#define BODY_ISE "<html><head><title>500 Internal Server Error</title>" \
                 "</head><body>Internal Server Error</body></html>"
static struct MHD_Response *response_ISE = NULL;

static struct MHD_Response *response_empty = NULL;

/* Static helper declarations.  */
static int
init_mhd_daemon (const struct red_conf conf);

static int
init_db (const struct red_conf conf);

static int
red_handler (void *cls,
             struct MHD_Connection *connection,
             const char *url,
             const char *method,
             const char *version,
             const char *upload_data,
             size_t *upload_data_size,
             void **con_cls);

static int
queue_response_for_request (struct MHD_Connection *connection,
                            const char *url,
                            const char *method);


/* This page contains function definitions for declarations exposed in the
engine header.  */


/* Initializes the redirect engine. Returns 0 on success, -1 on error. */
int
red_init (const struct red_conf conf)
{
  if (init_db (conf) < 0)
    LOG_AND_RET ("Unable to setup DB environment.", -1);

  /* Prep a canned 403 'Method Not Allowed' response.  */
  response_MNA = MHD_create_response_from_buffer (strlen (BODY_MNA),
                                                  BODY_MNA,
                                                  MHD_RESPMEM_PERSISTENT);

  /* Prep a canned 404 'Not Found' response.  */
  response_NF = MHD_create_response_from_buffer (strlen (BODY_NF),
                                                 BODY_NF,
                                                 MHD_RESPMEM_PERSISTENT);

  /* Prep a canned 500 'Internal Server Error' response.  */
  response_ISE = MHD_create_response_from_buffer (strlen (BODY_ISE),
                                                  BODY_ISE,
                                                  MHD_RESPMEM_PERSISTENT);

  /* Prep a canned empty response.  */
  response_empty = MHD_create_response_from_buffer (strlen (""),
                                                    "",
                                                    MHD_RESPMEM_PERSISTENT);

  if (init_mhd_daemon (conf) < 0)
    LOG_AND_RET ("Unable to start microhttpd daemon.", -1);

  return 0;
}


/* Terminates the redirect engine. Returns 0 on success, -1 on erroring out. */
int
red_terminate (void)
{
  /* Stop MHD daemon.  */
  MHD_stop_daemon (mhd_daemon);

  /* Destroy canned responses.  */
  MHD_destroy_response (response_MNA);
  MHD_destroy_response (response_NF);
  MHD_destroy_response (response_ISE);
  MHD_destroy_response (response_empty);

  /* Close the Berkeley DB environment.  */

  if (db->close (db, 0))
    LOG_AND_RET ("Error calling `db->close'.", -1);

  if (db_env->close (db_env, 0))
    LOG_AND_RET ("Error calling `db_env->close'.", -1);

  return 0;
}

/* This page contains static helper functions.  */


/* An MHD_AccessHandlerCallback for the redirect engine.
MicroHTTPD will forward requests here.  */
static int
red_handler (void *cls,
             struct MHD_Connection *connection,
             const char *url,
             const char *method,
             const char *version,
             const char *upload_data,
             size_t *upload_data_size,
             void **con_cls)
{
  int ret;

  /* We only support GET and HEAD requests.  */
  if (strcmp (method, MHD_HTTP_METHOD_GET)
      && strcmp (method, MHD_HTTP_METHOD_HEAD))
    {
      ret = MHD_queue_response (connection,
                                MHD_HTTP_METHOD_NOT_ALLOWED,
                                response_MNA);
    }
  else
    {
      ret = queue_response_for_request (connection, url, method);
    }

  if (ret == MHD_NO)
    syslog (LOG_WARNING, _("Failed to respond to '%s %s"), method, url);

  return ret;
}


static int
init_db (const struct red_conf conf)
{
  char * db_filename;

  /* Verify and prep DB path.  */

  if ( (! conf.home_dir)
       || (conf.home_dir == '\0'))
    LOG_AND_RET ("Invalid home directory.", -1);

  db_filename = malloc (strlen (conf.home_dir)
                        + strlen ("/" RED_IDENT ".db")
                        + 1);
  strcpy (db_filename, conf.home_dir);
  strcat (db_filename, "/" RED_IDENT ".db");

  /* Set up a concurrent Berkeley DB environment.  */

  if (db_env_create (&db_env, 0))
    LOG_AND_RET ("Error calling `db_env_create'.", -1);

  if (db_env->open (db_env,
                    NULL,
                    DB_INIT_CDB | DB_INIT_MPOOL /* Concurrent data access.  */
                    | DB_THREAD | DB_CREATE,
                    0))
    LOG_AND_RET ("Error calling `db_env->open'.", -1);

  if (db_create (&db, db_env, 0))
    LOG_AND_RET ("Error calling `db_create'.", -1);

  if (db->open (db, NULL, db_filename, NULL, DB_BTREE,
                DB_CREATE | DB_THREAD,
                0))
    LOG_AND_RET ("Error calling `db->open'.", -1);

  syslog (LOG_NOTICE, _("Opened DB file %s.\n"), db_filename);
  return 0;
}


static int
init_mhd_daemon (const struct red_conf conf)
{
  /* Ask MicroHTTPD to start listening.  */

  mhd_daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY,
                                 conf.port,
                                 NULL, /* Accept policy callback.  */
                                 NULL, /* Accept policy cb arg.  */
                                 &red_handler, /* Handler callback.  */
                                 NULL, /* Handler cb arg.  */
                                 MHD_OPTION_END);

  if (mhd_daemon == NULL)
    return -1;

  syslog (LOG_NOTICE, _("Started listening on port %d.\n"), conf.port);
  return 0;
}


static int
queue_response_for_request (struct MHD_Connection *connection,
                            const char *url,
                            const char *method)
{
  DBT key, value;
  unsigned int status_code;
  struct MHD_Response *response;

  memset (&key, 0, sizeof (DBT));
  memset (&value, 0, sizeof (DBT));

  key.data = strdup (url);
  key.size = key.ulen = strlen (url) + 1;
  key.flags = DB_DBT_USERMEM;

  value.data = NULL;
  value.size = value.ulen = 0;
  value.flags = DB_DBT_MALLOC;

  /* If this is a HEAD request, we will send an empty response no matter
   * what.  */
  response = response_empty;

  switch (db->get (db, NULL, &key, &value, 0))
    {
      case 0:
        /* Redirect location was found.  */
        /* TODO: Implement this.  */
        status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
        if (strcmp (method, MHD_HTTP_METHOD_HEAD))
          response = response_ISE;
        break;

      case DB_NOTFOUND:
        /* Redirect location was not found.  */
        status_code = MHD_HTTP_NOT_FOUND;
        if (strcmp (method, MHD_HTTP_METHOD_HEAD))
          response = response_NF;
        break;

      default:
        /* Something went wrong while accessing the DB.  */
        status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
        if (strcmp (method, MHD_HTTP_METHOD_HEAD))
          response = response_ISE;
        break;
    }

  free (key.data);
  free (value.data);

  return MHD_queue_response (connection, status_code, response);
}
