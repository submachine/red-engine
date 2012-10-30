#include <string.h>
#include <microhttpd.h>

/* The core of the redirect engine.  */

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

  /* We only support redirecting GET and HEAD requests.  */
  if (strcmp (method, MHD_HTTP_METHOD_GET)
      && strcmp (method, MHD_HTTP_METHOD_HEAD))
    {
      /* TODO: Return an appropriate response code instead of rudely closing
      the socket.  */
      return MHD_NO;
    }

  /* TODO: This is just a placeholder which returns a sane response.  */
  response = MHD_create_response_from_buffer (0, NULL, MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response (connection, MHD_HTTP_SERVICE_UNAVAILABLE, response);
  MHD_destroy_response (response);
  return ret;
}

