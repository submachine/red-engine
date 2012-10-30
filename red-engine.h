#include <microhttpd.h>

/* The permanent redirect handler. Must be registered with MHD when starting
the MHD daemon.  */
extern int
red_handler (void *cls,
             struct MHD_Connection *connection,
             const char *url,
             const char *method,
             const char *version,
             const char *upload_data,
             size_t *upload_data_size,
             void **con_cls);
