/* middfs-server.c --  middfs server main() routine
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "middfs-sock.h"
#include "middfs-conn.h"
#include "middfs-rsrc.h"
#include "middfs-util.h"

#define LISTEN_PORT_DEFAULT "56789"

int main(int argc, char *argv[]) {
  char *listen_port = LISTEN_PORT_DEFAULT;
  int exitno = 0;
  
  /* parse command-line args */
  int c;
  char *optstring = "p:h";
  const char *usage = "usage: %s [-p <listen-port>] <mountpoint>\n";
  int optvalid = 1;
  while ((c = getopt(argc, argv, optstring)) >= 0) {
    switch (c) {
    case 'p':
      listen_port = optarg;
      break;
    case 'h':
    case '?':
    default:
      fprintf(stderr, usage, argv[0]);
      optvalid = 0;
    }
  }

  if (!optvalid) {
    return 1;
  }

  if (argc - optind != 1) {
    fprintf(stderr, usage, argv[0]);
    return 1;
  }

  /* start server for listening */
  int servfd; /* server file descriptor */
  if ((servfd = server_start("4321", 10)) < 0) {
    return 2;
  }

  /* initialize socket list to poll on */
  struct middfs_socks socks;
  if (middfs_socks_init(&socks) < 0) {
    exitno = 3;
    close(servfd);
    return 3;
  }

  /* add server listening socket to socket list to poll */
  struct middfs_sockinfo serv_sockinfo = {MFD_LSTN};
  if (middfs_socks_add(servfd, &serv_sockinfo, &socks) < 0) {
    exitno = 4;
    close(servfd);
    goto cleanup;
  }

  /* call server loop */
  while (server_loop(&socks) >= 0) {}
  
 cleanup:
  if (middfs_socks_delete(&socks) < 0) {
    exitno = 5;
  }

  return exitno;
}
