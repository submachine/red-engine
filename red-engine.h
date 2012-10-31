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

struct red_conf
{
  unsigned short port;
  char *home_dir;
};


/* Initializes the redirect engine. Returns 0 on success, -1 on error. */
extern int
red_init (const struct red_conf conf);


/* Terminates the redirect engine. Returns 0 on success, -1 on erroring out. */
extern int
red_terminate (void);

#endif
