#ifndef RED_ENGINE_H
#define RED_ENGINE_H

#define _BSD_SOURCE

#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MHD_PLATFORM_H
#include <microhttpd.h>

#define RED_IDENT "red-engine"

/* Initializes the redirect engine. Returns 0 on success, -1 on error. */
extern int
red_init (const char * home_dir);


/* Terminates the redirect engine. Returns 0 on success, -1 on erroring out. */
extern int
red_terminate (void);


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

#endif
