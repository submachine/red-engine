#include "red-engine.h"

#include <string.h>
#include <syslog.h>
#include <db.h>
#include <glib/gi18n.h>


#define LOG_AND_RET(err_str, ret_val) \
do { syslog (LOG_ERR, _(err_str)); return ret_val; } while (0)

/* The core of the redirect engine.  */

static DB_ENV *db_env;
static DB *db;

/* An HTTP 403 - 'Method Not Allowed' response. For requests other than GET
and HEAD.  */
#define BODY_MNA "<html><head><title>405 Method Not Allowed</title>" \
                 "</head><body>Method Not Allowed</body></html>"

static struct MHD_Response *response_MNA = NULL;


/* Initializes the redirect engine. Returns 0 on success, -1 on error. */
int
red_init (const char * home_dir)
{
  char * db_filename;

  if ( (! home_dir)
       || (home_dir == '\0'))
    LOG_AND_RET ("Invalid home directory.\n", -1);

  db_filename = malloc (strlen (home_dir)
                        + strlen ("/" RED_IDENT ".db")
                        + 1);
  strcpy (db_filename, home_dir);
  strcat (db_filename, "/" RED_IDENT ".db");

  syslog (LOG_INFO, "DB File is at %s.\n", db_filename);

  /* Set up a concurrent Berkeley DB environment.  */

  if (db_env_create (&db_env, 0))
    LOG_AND_RET ("Error calling `db_env_create'.\n", -1);

  if (db_env->open (db_env,
                    NULL,
                    DB_INIT_CDB | DB_INIT_MPOOL /* Concurrent data access.  */
                    | DB_THREAD | DB_CREATE,
                    0))
    LOG_AND_RET ("Error calling `db_env->open'.\n", -1);

  if (db_create (&db, db_env, 0))
    LOG_AND_RET ("Error calling `db_create'.\n", -1);

  if (db->open (db, NULL, db_filename, NULL, DB_BTREE,
                DB_CREATE | DB_THREAD,
                0))
    LOG_AND_RET ("Error calling `db->open'.\n", -1);

  /* Prep a canned 403 'Method Not Allowed' response.  */
  response_MNA = MHD_create_response_from_buffer (strlen (BODY_MNA),
                                                  BODY_MNA,
                                                  MHD_RESPMEM_PERSISTENT);

  return 0;
}


/* Terminates the redirect engine. Returns 0 on success, -1 on erroring out. */
int
red_terminate (void)
{
  /* Destroy canned responses.  */

  MHD_destroy_response (response_MNA);

  /* Close the Berkeley DB environment.  */

  if (db->close (db, 0))
    LOG_AND_RET ("Error calling `db->close'.\n", -1);

  if (db_env->close (db_env, 0))
    LOG_AND_RET ("Error calling `db_env->close'.\n", -1);

  return 0;
}


/* An MHD_AccessHandlerCallback for the redirect engine.
MicroHTTPD will forward requests here.  */
int
red_handler (void *cls,
             struct MHD_Connection *connection,
             const char *url,
             const char *method,
             const char *version,
             const char *upload_data,
             size_t *upload_data_size,
             void **con_cls)
{
  struct MHD_Response *response;
  int ret;

  /* We only support GET and HEAD requests.  */
  if (strcmp (method, MHD_HTTP_METHOD_GET)
      && strcmp (method, MHD_HTTP_METHOD_HEAD))
    {
      ret = MHD_queue_response (connection,
                                MHD_HTTP_METHOD_NOT_ALLOWED,
                                response_MNA);
      return ret;
    }

  /* TODO: This is just a placeholder which returns a sane response.  */
  response = MHD_create_response_from_buffer (0, NULL, MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response (connection, MHD_HTTP_SERVICE_UNAVAILABLE, response);
  MHD_destroy_response (response);
  return ret;
}
